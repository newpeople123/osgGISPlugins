// 全量瓦片 POSITION accessor 三件事：
//   1. 声明的 accessor min/max 与按字节实算的是否逐条全等（不等 = 包围体与几何脱钩，
//      Cesium 的 LOD/剔除就建立在假的范围上）；
//   2. 是否有非有限分量（NaN/±Inf 写进 POSITION 数组）；
//   3. 是否出现 |coord| > 阈值（默认 1e6 m）的荒谬顶点——A-38 那一族"单个离谱顶点"的判据。
// 两条分开统计 b3dm（主树几何）与 i3dm（实例烘焙坐标）：v15 实测 >1e6 只有 i3dm，
// 那是 A-41 的烘焙错位，不是 A-38；混在一起看会把两个问题读成一个。
//
// 两条判据互相独立，都要留着：A-38 那一次污染的顶点**同时**被写进了声明的 max
// （matrix_a34/t02 实测 声明≠实算=0、>1e6=3 个瓦片），所以"min/max 全等"根本拦不住
// 荒谬顶点；反过来"坐标幅度"也拦不住包围体与几何脱钩（把某个顶点改成 1234.5 只让
// 前者报警）。两条都已做阳性对照，见 docs/model23dtiles-production/evidence/。
// 阈值 1e6 是按真实地物尺度定的启发式：**t08_big_coords 这个 fixture 本来就该是 1e7**，
// 整批扫描里它必然 FAIL，那是设计如此，不是缺陷（要跳过它就用 --abs 1e8）。
//
// 用法: node scripts/accessor_extents_scan.js <数据集目录|容器目录|tiles 目录> [...] [--abs 1e6]
// 退出码：有违例 = 1。
const fs = require('fs');
const path = require('path');

const absArg = process.argv.indexOf('--abs');
const ABS = absArg > 0 ? Number(process.argv[absArg + 1]) : 1e6;
const positional = process.argv.slice(2).filter((a, i) =>
  !a.startsWith('--') && !(absArg > 0 && i + 2 === absArg + 1));

function tilesOf(arg) {
  const st = fs.statSync(arg);
  if (!st.isDirectory()) throw new Error('not a directory: ' + arg);
  if (fs.existsSync(path.join(arg, 'tileset.json')))
    return [{ name: path.basename(arg.replace(/[\\/]+$/, '')), dir: path.join(arg, 'tiles') }];
  // 已经是 tiles/ 目录
  const own = fs.readdirSync(arg).filter((f) => /\.(b3dm|i3dm)$/.test(f));
  if (own.length) return [{ name: path.basename(arg.replace(/[\\/]+$/, '')), dir: arg }];
  return fs.readdirSync(arg)
    .map((n) => path.join(arg, n))
    .filter((p) => { try { return fs.statSync(p).isDirectory(); } catch (e) { return false; } })
    .filter((p) => fs.existsSync(path.join(p, 'tileset.json')))
    .map((p) => ({ name: path.basename(p),
      dir: fs.existsSync(path.join(p, 'tiles')) ? path.join(p, 'tiles') : p }));
}

// 外壳头：b3dm 28B，i3dm 32B（多一个 glTFFormat）；GLB 紧跟四张表之后。
function glbOf(buf) {
  const magic = buf.toString('latin1', 0, 4);
  const hdr = magic === 'i3dm' ? 32 : 28;
  const ftj = buf.readUInt32LE(12), ftb = buf.readUInt32LE(16);
  const btj = buf.readUInt32LE(20), btb = buf.readUInt32LE(24);
  const g = hdr + ftj + ftb + btj + btb;
  if (buf.toString('latin1', g, g + 4) !== 'glTF') throw new Error('no GLB at ' + g);
  const jl = buf.readUInt32LE(g + 12);
  return { json: JSON.parse(buf.toString('utf8', g + 20, g + 20 + jl)), binOff: g + 20 + jl + 8 };
}

function scan(dir) {
  const r = { files: 0, b3dm: 0, i3dm: 0, acc: 0, mismatch: [], absurdB3dm: [], absurdI3dm: [],
    nonFinite: [],
    maxB3dm: 0, maxB3dmFile: '', maxI3dm: 0, maxI3dmFile: '', skipped: 0 };
  let list = [];
  try { list = fs.readdirSync(dir).filter((f) => /\.(b3dm|i3dm)$/.test(f)).sort(); } catch (e) { return r; }
  for (const f of list) {
    let buf, j, binOff;
    try { buf = fs.readFileSync(path.join(dir, f)); const g = glbOf(buf); j = g.json; binOff = g.binOff; }
    catch (e) { r.skipped++; continue; }
    r.files++;
    const isI3dm = /\.i3dm$/.test(f);
    isI3dm ? r.i3dm++ : r.b3dm++;
    const bvs = j.bufferViews || [];
    // 只统计真正被图元引用的 VEC3 float accessor（POSITION/NORMAL 都算，未引用的不算产物语义）
    const used = new Set(Object.values(j.meshes || {}).flatMap((m) => m.primitives || [])
      .flatMap((p) => Object.values(p.attributes || {})));
    (j.accessors || []).forEach((acc, i) => {
      if (acc.type !== 'VEC3' || acc.componentType !== 5126 || !used.has(i)) return;
      const bv = bvs[acc.bufferView];
      if (!bv || (bv.byteStride || 12) !== 12) { r.skipped++; return; }
      const base = binOff + (bv.byteOffset || 0) + (acc.byteOffset || 0);
      if (!Number.isInteger(acc.count) || acc.count <= 0 || base + acc.count * 12 > buf.length) { r.skipped++; return; }
      r.acc++;
      const mn = [Infinity, Infinity, Infinity], mx = [-Infinity, -Infinity, -Infinity];
      let peak = 0, nonFinite = 0;
      for (let k = 0; k < acc.count * 3; k++) {
        const v = buf.readFloatLE(base + k * 4);
        if (!Number.isFinite(v)) nonFinite++;
        const comp = k % 3;
        if (v < mn[comp]) mn[comp] = v;
        if (v > mx[comp]) mx[comp] = v;
        if (Math.abs(v) > peak) peak = Math.abs(v);
      }
      if (nonFinite) r.nonFinite.push(f + ' acc' + i + '(' + nonFinite + ' 个分量)');
      if (peak > (isI3dm ? r.maxI3dm : r.maxB3dm)) {
        if (isI3dm) { r.maxI3dm = peak; r.maxI3dmFile = f + ' acc' + i; }
        else { r.maxB3dm = peak; r.maxB3dmFile = f + ' acc' + i; }
      }
      if (peak > ABS) (isI3dm ? r.absurdI3dm : r.absurdB3dm).push(f);
      if (acc.min && acc.max) {
        for (let c = 0; c < 3; c++) {
          if (Math.abs(acc.min[c] - mn[c]) > 1e-4 || Math.abs(acc.max[c] - mx[c]) > 1e-4) {
            r.mismatch.push(f + ' acc' + i); break;
          }
        }
      }
    });
  }
  return r;
}

let bad = 0, ran = 0;
console.log('\n| dataset | b3dm | i3dm | VEC3 acc | 声明≠实算 accessor | 非有限分量 | b3dm >1e6 瓦片 | i3dm >1e6 瓦片 | b3dm max\|c\| | i3dm max\|c\| | 判 |');
console.log('|---|---|---|---|---|---|---|---|---|---|---|');
for (const arg of positional) {
  let sets;
  try { sets = tilesOf(arg); } catch (e) { console.error('SKIP ' + arg + ': ' + e.message); continue; }
  if (!sets.length) { console.error('SKIP ' + arg + ': 没找到 tileset/tiles'); continue; }
  for (const s of sets) {
    const r = scan(s.dir);
    if (!r.files) { console.error('SKIP ' + s.name + ': ' + s.dir + ' 无瓦片'); continue; }
    ran++;
    const fail = r.mismatch.length || r.absurdB3dm.length || r.nonFinite.length;
    if (fail) bad++;
    const uniq = (a) => new Set(a).size;
    console.log('| ' + [s.name, r.b3dm, r.i3dm, r.acc, r.mismatch.length, r.nonFinite.length,
      uniq(r.absurdB3dm), uniq(r.absurdI3dm), r.maxB3dm.toExponential(3), r.maxI3dm.toExponential(3),
      fail ? 'FAIL' : 'PASS'].join(' | ') + ' |');
    if (r.nonFinite.length) console.log('  非有限分量: ' + r.nonFinite.slice(0, 6).join(', '));
    if (r.mismatch.length) console.log('  声明≠实算: ' + r.mismatch.slice(0, 6).join(', ')
      + (r.mismatch.length > 6 ? ' …' : ''));
    if (r.absurdB3dm.length) console.log('  b3dm 荒谬顶点: ' + [...new Set(r.absurdB3dm)].slice(0, 6).join(', '));
    if (r.absurdI3dm.length) console.log('  i3dm >' + ABS + '（A-41 烘焙口径，不计入本判据）: '
      + [...new Set(r.absurdI3dm)].slice(0, 6).join(', '));
    if (r.skipped) console.log('  跳过（解析失败/非连续布局）accessor+文件 ' + r.skipped + ' 个');
  }
}
console.log('\ndatasets=' + ran + '  违例数据集=' + bad + '  阈值 |coord| > ' + ABS);
process.exit(bad ? 1 : 0);
