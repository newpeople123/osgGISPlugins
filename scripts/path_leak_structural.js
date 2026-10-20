// A-42 复核：把瓦片按结构拆开，只在"会被客户端解析的 JSON 区"里判泄漏。
// path_leak_scan.js 的裸正则在 GLB 二进制 chunk 上会命中噪声，那个数不能当证据。
// 这里的口径：分发物里每个 JSON 区（feature table / batch table / GLB 的 JSON chunk）
// 先解析成对象，字符串字面量里出现任一"绝对路径"形态才算泄漏：
// 盘符、前导 / 或 \（含 UNC）、file:// URI。
//
// 退出码：0 = CLEAN；1 = 分发物 JSON 区有泄漏；3 = 尺子自己失效（结构或 JSON 解析失败）。
// 取 3 时那个 0 也不能信，必须先修脚本再读结论。
// 用法: node scripts/path_leak_structural.js <数据集目录|瓦片文件>...
const fs = require('fs');
const path = require('path');

const TILE_EXT = ['.b3dm', '.cmpt', '.i3dm', '.pnts'];
// 前一个字符不能是字母/数字：否则 "http://…" 里的 "p:/" 会被当成盘符（URI 不是泄漏）
const DRIVE = /(?:^|[^A-Za-z0-9])([A-Za-z]:[/\\])/;
const DRIVE_RE = /"[A-Za-z]:[/\\][^"]{4,}"/g;   // 与 path_leak_scan 同一个裸正则，仅用于分区计数

// 绝对偏移写入 regions；cmpt 递归内层瓦片，tag 只用于报错定位，region.name 保持可聚合
function tileRegions(buf, base, end, tag, regions, errs) {
  const magic = buf.toString('latin1', base, base + 4).toLowerCase();
  if (magic === 'cmpt') {
    regions.push({ name: 'cmptHdr', start: base, end: Math.min(base + 16, end), json: false });
    const f12 = buf.readUInt32LE(base + 12);
    let o = base + 16, k = 0;
    while (o + 12 <= end) {
      const len = buf.readUInt32LE(o + 8);
      if (len < 12 || o + len > end) { errs.push(tag + 'cmpt 内层长度越界 @' + o + ' len=' + len + ' end=' + end); return; }
      tileRegions(buf, o, o + len, tag + 'i' + k + '.', regions, errs);
      o += len; k++;
    }
    if (o !== end) errs.push(tag + 'cmpt 内层没走到尾：o=' + o + ' end=' + end);
    // 规范说偏移 12 是"所有内层块的字节总长"，本仓写出的实测是"内层块数"。
    // 两种解释都试，都不符就报失效——不能默认其中一种，否则 cmpt 会静默漏检。
    const innerBytes = end - (base + 16);
    if (f12 !== innerBytes && f12 !== k)
      errs.push(tag + 'cmpt 偏移12=' + f12 + ' 既不是内层字节总长(' + innerBytes + ')也不是块数(' + k + ')');
    else if (process.env.PLS_REGIONS === '1')
      console.log(`  [cmpt 偏移12] ${tag || ''}=${f12} → ${f12 === k ? '按块数吻合' : '按字节总长吻合'}（实走 ${k} 块）`);
    return;
  }
  if (magic !== 'b3dm' && magic !== 'i3dm' && magic !== 'pnts') {
    errs.push(tag + 'magic 不是瓦片: ' + JSON.stringify(magic));
    return;
  }
  const ftJ = buf.readUInt32LE(base + 12), ftB = buf.readUInt32LE(base + 16);
  const btJ = buf.readUInt32LE(base + 20), btB = buf.readUInt32LE(base + 24);
  // i3dm 在偏移 28 处多一个 uint32 gltfFormat；b3dm / pnts 没有这个字段
  let o = base + (magic === 'i3dm' ? 32 : 28);
  const push = (name, len, json) => {
    regions.push({ name, start: o, end: Math.min(o + len, end), json }); o += len;
  };
  push('ftJSON', ftJ, true); push('ftBIN', ftB, false);
  push('btJSON', btJ, true); push('btBIN', btB, false);

  if (o + 12 > end) { errs.push(tag + 'GLB 段不存在 @' + o + ' end=' + end); return; }
  if (buf.toString('latin1', o, o + 4) !== 'glTF') {
    errs.push(tag + 'GLB 魔数不符 @' + o + ' = ' + JSON.stringify(buf.toString('latin1', o, o + 4)));
    return;
  }
  const glbEnd = Math.min(o + buf.readUInt32LE(o + 8), end);
  let p = o + 12, i = 0;
  while (p + 8 <= glbEnd) {
    const len = buf.readUInt32LE(p), type = buf.readUInt32LE(p + 4);
    if (len % 4) errs.push(tag + 'GLB chunk' + i + ' 长度未 4 字节对齐 len=' + len);
    const name = type === 0x4E4F534A ? 'glbJSON'
      : type === 0x004E4942 ? 'glbBIN' : 'glbChunk_' + (type >>> 0).toString(16);
    regions.push({ name: name + '_' + i, start: p + 8,
      end: Math.min(p + 8 + len, end), json: type === 0x4E4F534A });
    p += 8 + len; i++;
  }
  if (p > glbEnd) errs.push(tag + 'GLB chunk 走过头 ' + p + ' > ' + glbEnd);
}

function classify(regions, off) {
  for (const r of regions) if (off >= r.start && off < r.end) return r;
  return { name: 'outsideRegions' };
}

function walkJson(node, ptr, hits, where) {
  if (typeof node === 'string') {
    // 三种"绝对路径"形式都要报：盘符、前导 / 或 \（含 UNC）、file:// URI。
    // 只认盘符会漏掉前导斜杠——那种 uri 还会让相对部署直接解析失败。
    if (DRIVE.test(node)) hits.push({ where, ptr, value: node.slice(0, 200), kind: '盘符' });
    else if (/^[/\\]/.test(node) || /^(?:\\\\|file:\/\/)/i.test(node))
      hits.push({ where, ptr, value: node.slice(0, 200), kind: '前导斜杠/UNC/file:' });
  } else if (Array.isArray(node)) {
    node.forEach((v, i) => walkJson(v, ptr + '/' + i, hits, where));
  } else if (node && typeof node === 'object') {
    for (const [k, v] of Object.entries(node)) {
      if (DRIVE.test(k)) hits.push({ where, ptr: ptr + '/' + k, value: '(key) ' + k.slice(0, 200), kind: '盘符' });
      walkJson(v, ptr + '/' + k.replace(/~/g, '~0').replace(/\//g, '~1'), hits, where);
    }
  }
}

let leak = 0, corroborate = 0, residue = 0, sidecar = 0, sideFiles = 0, tiles = 0, naked = 0, toolErrs = 0;
const byRegion = new Map();

function handleTile(file, buf) {
  tiles++;
  const regions = [], errs = [];
  try { tileRegions(buf, 0, buf.length, '', regions, errs); }
  catch (e) { errs.push('抛异常: ' + e.message); }
  for (const m of errs) { console.log(`  [尺子失效] ${file}  ${m}`); toolErrs++; }
  if (process.env.PLS_REGIONS === '1') {
    console.log(`  区表 ${path.basename(file)}: ` + regions.map(r =>
      `${r.name}[${r.start},${r.end}${r.json ? ',JSON' : ''}]`).join(' '));
  }

  const s = buf.toString('latin1');
  let m;
  DRIVE_RE.lastIndex = 0;
  while ((m = DRIVE_RE.exec(s))) {
    naked++;
    const r = classify(regions, m.index);
    byRegion.set(r.name, (byRegion.get(r.name) || 0) + 1);
    if (r.json) {
      r.naked = (r.naked || 0) + 1;
      console.log(`  [裸命中落在 JSON 区] ${file} @${m.index} in ${r.name} ${JSON.stringify(m[0].slice(0, 80))}`);
      corroborate++;
    } else if (process.env.PLS_SHOW === '1') {
      console.log(`  [二进制噪声命中] ${path.basename(file)} @${m.index} in ${r.name}` +
        ` 匹配=${JSON.stringify(m[0].replace(/[^\x20-\x7e]/g, '.').slice(0, 48))}` +
        ` 上下文 hex=${buf.subarray(m.index - 8, m.index + 32).toString('hex')}`);
    }
  }

  const hits = [];
  for (const r of regions) {
    if (!r.json || r.end <= r.start) continue;
    const txt = buf.toString('utf8', r.start, r.end).replace(/\0+$/, '').trim();
    if (!txt) { if (r.naked) { console.log(`  [JSON 区为空但含裸命中] ${file} ${r.name}`); residue++; } continue; }
    let j;
    try { j = JSON.parse(txt); }
    catch (e) { console.log(`  [尺子失效] ${file}  ${r.name} JSON 解析失败: ${e.message}`); toolErrs++; continue; }
    const before = hits.length;
    walkJson(j, '', hits, r.name);
    r.hits = hits.length - before;
  }
  for (const h of hits) {
    console.log(`  [泄漏:${h.kind}] ${file}  ${h.where}${h.ptr} = ${JSON.stringify(h.value)}`);
    leak++;
  }
  // 裸命中进了 JSON 区、解析后的字符串里却没有 → 只剩对齐 padding 里的残留字节：
  // 客户端不会读到它，但它确实随分发物一起发出去，单独计数、不算泄漏。
  for (const r of regions) if (r.json && r.naked && !r.hits) {
    console.log(`  [padding 残留] ${file}  ${r.name} 裸命中 ${r.naked} 处但解析后无含盘符字符串`);
    residue++;
  }
}

function handleSidecar(file) {
  sideFiles++;
  let j;
  try { j = JSON.parse(fs.readFileSync(file, 'utf8')); }
  catch (e) { console.log(`  [尺子失效] ${file} 旁证 JSON 解析失败: ${e.message}`); toolErrs++; return; }
  const hits = [];
  walkJson(j, '', hits, 'sidecar');
  for (const h of hits) {
    console.log(`  [旁证含绝对路径:${h.kind}] ${path.basename(file)}${h.ptr} = ${JSON.stringify(h.value)}`);
    sidecar++;
  }
}

for (const arg of process.argv.slice(2)) {
  if (!fs.existsSync(arg)) { console.error('不存在: ' + arg); process.exit(2); }
  if (fs.statSync(arg).isDirectory()) {
    (function walk(x) {
      for (const e of fs.readdirSync(x, { withFileTypes: true })) {
        const f = path.join(x, e.name);
        if (e.isDirectory()) { walk(f); continue; }
        const ext = path.extname(e.name).toLowerCase();
        if (TILE_EXT.includes(ext)) handleTile(f, fs.readFileSync(f));
        else if (ext === '.json') handleSidecar(f);
      }
    })(arg);
  } else {
    const ext = path.extname(arg).toLowerCase();
    if (ext === '.json') handleSidecar(arg); else handleTile(arg, fs.readFileSync(arg));
  }
}

console.log(`\n瓦片 ${tiles} 个 / 旁证 json ${sideFiles} 个`);
console.log('裸正则命中 ' + naked + ' 处，按区分布：' +
  (naked ? [...byRegion.entries()].sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k}=${v}`).join(' ') : '无'));
console.log(`分发物判定：${leak ? 'LEAK（' + leak + ' 处）' : 'CLEAN（JSON 区解析后无绝对路径）'}` +
  `；裸命中进 JSON 区 ${corroborate} 处 / padding 残留 ${residue} 个区` +
  `；旁证含绝对路径 ${sidecar} 处（不进退出码）；尺子失效 ${toolErrs} 处`);
process.exit(leak ? 1 : toolErrs ? 3 : 0);
