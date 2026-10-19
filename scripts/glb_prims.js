// 读回写盘后的 glTF primitive 数，核对仓库里没有硬限闸的那一条：
// 手册 §3 第 3 条「每瓦片 ≤ 24 个 glTF primitive」。
// 分区侧只有 Load::prim（源图元去重计数）参与切分决策，编码器实际吐出
// 多少 primitive 一直没人验证过 —— STATUS.md B-9 的缺口。
//
// 用法：node scripts/glb_prims.js <datasetDir> [...]
//   datasetDir 里找 tileset.json，逐条 content.uri 打开 b3dm / i3dm，
//   也支持 cmpt（拆开内层独立瓦片）。
// 判定：prims = 单文件所有 mesh.primitives 之和，上限默认 24（WB_MAX_PRIMS 覆盖）；
//       expandedTris = accessor 面数 x i3dm INSTANCES_LENGTH，
//       上限默认 120000（WB_MAX_TRIS 覆盖）—— 手册 §3 的另一条硬限。
const fs = require('fs');
const path = require('path');

const MAX = Number(process.env.WB_MAX_PRIMS || 24);
const MAX_TRIS = Number(process.env.WB_MAX_TRIS || 120000);
const dirs = process.argv.slice(2);
if (!dirs.length) {
  console.error('usage: node scripts/glb_prims.js <datasetDir> [...]');
  process.exit(2);
}

function readGlbJson(buf, off) {
  // GLB: 12B 头（magic/version/length）+ 若干 chunk（8B 头：len + type，type 0 = JSON）
  if (buf.toString('utf8', off, off + 4) !== 'glTF') {
    throw new Error('not a GLB at offset ' + off);
  }
  const version = buf.readUInt32LE(off + 4);
  const chunkLen = buf.readUInt32LE(off + 12);
  const chunkType = buf.readUInt32LE(off + 16);
  if (chunkType !== 0x4E4F534A /* "JSON" */) {
    throw new Error('first GLB chunk is not JSON (0x' + chunkType.toString(16) + ')');
  }
  const json = JSON.parse(buf.toString('utf8', off + 20, off + 20 + chunkLen));
  return { json, version };
}

// 返回 { prims, meshes, tris, instances, expandedTris }
function statsOfGltf(json, instances) {
  let prims = 0, tris = 0;
  const meshes = json.meshes ? json.meshes.length : 0;
  const accCache = new Map();
  for (const m of (json.meshes || [])) {
    for (const p of (m.primitives || [])) {
      prims++;
      // 三角形数：优先 indices accessor 分量数 / 3，否则用 POSITION 顶点数 / 3
      let n = 0;
      if (typeof p.indices === 'number') {
        n = accessorCount(json, p.indices) / 3;
      } else if (typeof p.attributes.POSITION === 'number') {
        n = accessorCount(json, p.attributes.POSITION) / 3;
      }
      tris += Math.floor(n);
    }
  }
  function accessorCount(j, i) {
    if (accCache.has(i)) return accCache.get(i);
    const a = (j.accessors || [])[i];
    const c = a ? a.count : 0;
    accCache.set(i, c);
    return c;
  }
  // i3dm 的 accessor 只存一份原型，运行时按实例数展开绘制：
  // 手册 §3 的三角形硬限管的是展开后的真实绘制量（见 AUDIT A-33）。
  const inst = instances > 0 ? instances : 1;
  return { prims, meshes, tris, instances: inst, expandedTris: tris * inst };
}

function innerCmpt(buf) {
  // cmpt: 16B 头 + 若干**独立**瓦片（magic 在 +0，自己的 byteLength 在 +8 当步长，8B 对齐）
  const out = [];
  let off = 16;
  const total = buf.readUInt32LE(8);
  while (off + 12 <= Math.min(total, buf.length)) {
    const len = buf.readUInt32LE(off + 8);
    out.push({ magic: buf.toString('utf8', off, off + 4), off, len });
    off += len + (len % 8 ? 8 - (len % 8) : 0);
  }
  return out;
}

function analyzeFile(file) {
  const buf = fs.readFileSync(file);
  const magic = buf.toString('utf8', 0, 4);
  const rows = [];
  const one = (tileOff, label) => {
    // 外壳头 = magic/version/byteLength + 四张表长，b3dm 是 28B，
    // i3dm 多一个 uint32 glTFFormat 所以是 32B（实测 mixed.cmpt：
    // 按 28B 算出的 GLB 偏移 24996，真实 magic 在 25000）。
    // GLB 紧跟在 feature/batch 表之后 —— 固定偏移是错的（该 b3dm 的两张表
    // 占了 260B，GLB 落在 288）。
    const magic = buf.toString('latin1', tileOff, tileOff + 4);
    const hdr = magic === 'i3dm' ? 32 : 28;
    const ftj = buf.readUInt32LE(tileOff + 12);
    const ftb = buf.readUInt32LE(tileOff + 16);
    const btj = buf.readUInt32LE(tileOff + 20);
    const btb = buf.readUInt32LE(tileOff + 24);
    let instances = 1;
    if (magic === 'i3dm' && ftj > 0) {
      // i3dm feature table JSON 里的 INSTANCES_LENGTH 就是运行时展开倍数
      try {
        const ft = JSON.parse(buf.toString('utf8', tileOff + hdr, tileOff + hdr + ftj));
        if (typeof ft.INSTANCES_LENGTH === 'number') { instances = ft.INSTANCES_LENGTH; }
      } catch (e) { /* 交给下面的 glTF 解析报错，这里不猜 */ }
    }
    const { json, version } = readGlbJson(buf, tileOff + hdr + ftj + ftb + btj + btb);
    const s = statsOfGltf(json, instances);
    s.name = label;
    s.glTFVersion = version;
    rows.push(s);
  };
  if (magic === 'cmpt') {
    for (const t of innerCmpt(buf)) one(t.off, path.basename(file) + '#' + t.magic);
  } else if (magic === 'b3dm' || magic === 'i3dm') {
    one(0, magic);
  } else {
    throw new Error('unknown container magic ' + JSON.stringify(magic));
  }
  return rows;
}

let bad = 0, files = 0;
const worst = [];
for (const dir of dirs) {
  const tsFile = path.join(dir, 'tileset.json');
  let tileset;
  try { tileset = JSON.parse(fs.readFileSync(tsFile, 'utf8')); }
  catch (e) { console.error('SKIP ' + dir + ': ' + e.message); continue; }

  const uris = [];
  (function walk(t) {
    if (!t) return;
    const c = t.content;
    if (typeof c === 'string') uris.push(c);
    else if (c && typeof c.uri === 'string') uris.push(c.uri);
    (t.children || []).forEach(walk);
  })(tileset.root);

  const name = path.basename(dir.replace(/[\\/]+$/, ''));
  let maxPrims = 0, maxTris = 0, maxExp = 0, maxFile = '', expFile = '', totTris = 0;
  for (const u of uris) {
    const f = path.join(dir, u);
    if (!fs.existsSync(f)) { console.error('  MISSING content ' + u); continue; }
    files++;
    let rows;
    try { rows = analyzeFile(f); }
    catch (e) { console.error('  PARSE FAIL ' + u + ': ' + e.message); bad++; continue; }
    for (const r of rows) {
      totTris += r.expandedTris;
      if (r.expandedTris > maxExp) { maxExp = r.expandedTris; expFile = u; }
      if (r.tris > maxTris) { maxTris = r.tris; }
      if (r.prims > maxPrims) { maxPrims = r.prims; maxFile = u; }
      if (r.prims > MAX) {
        console.error('  OVER-LIMIT ' + u + ': ' + r.prims + ' primitives > ' + MAX);
        bad++;
      }
      if (r.expandedTris > MAX_TRIS) {
        console.error('  OVER-LIMIT ' + u + ': ' + r.expandedTris
                      + ' expanded triangles > ' + MAX_TRIS
                      + ' (' + r.instances + ' instances x ' + r.tris + ')');
        bad++;
      }
    }
  }
  worst.push({ asset: name, tiles: uris.length, maxPrims, overAt: maxFile,
    readbackTris: maxTris, maxExpanded: maxExp, expAt: expFile, sumTris: totTris });
}

console.log('\n| dataset | tiles | max prims/tile | prims 最大文件 | max tris（去重） | max tris（实例展开） | 展开最大文件 | 展开合计 tris |');
console.log('|---|---|---|---|---|---|---|---|');
for (const w of worst) {
  console.log('| ' + [w.asset, w.tiles, w.maxPrims, w.overAt || '-',
    w.readbackTris, w.maxExpanded, w.expAt || '-', w.sumTris].join(' | ') + ' |');
}
console.log('\nfiles=' + files + '  violations+parseFailures=' + bad
  + '  limits: prims=' + MAX + ' expandedTriangles=' + MAX_TRIS);
console.log('口径：prims = glTF mesh.primitives 之和（结构上限，不随实例翻倍）；'
  + '"去重" = accessor 里实际存的面数；"实例展开" = 去重面数 x i3dm INSTANCES_LENGTH，'
  + '即运行时真实绘制量（手册 §3 三角形硬限按这一列判）。');
process.exit(bad ? 1 : 0);
