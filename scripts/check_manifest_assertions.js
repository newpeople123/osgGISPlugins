// 发布产物的三个跨文件一致性断言（手册 17 节"从源部件反查全部瓦片"链路）：
//   1. conversion-report.manifestOccurrences == feature-manifest.occurrenceCount
//   2. 每个发布了内容的聚合代理瓦片都在 manifest.aggregateProxies 里有非空条目，
//      且代理 ID 指向的源 ID 全部能在 manifest.features 里反查到
//   3. report.aggregateProxyCount - report.proxySkippedCount == 有内容的聚合代理瓦片数
//      （降级无内容的代理按 selfValidate/writeManifest 同一口径不计入 manifest）
// 用法：node scripts/check_manifest_assertions.js <数据集目录|容器目录> [...]
// 退出码：任一断言不成立 = 1。
const fs = require('fs');
const path = require('path');

function datasetDirs(arg) {
  const st = fs.statSync(arg);
  if (!st.isDirectory()) throw new Error('not a directory: ' + arg);
  if (fs.existsSync(path.join(arg, 'tileset.json'))) return [arg];
  return fs.readdirSync(arg)
    .map((n) => path.join(arg, n))
    .filter((p) => fs.existsSync(path.join(p, 'tileset.json')));
}

function check(dir) {
  const fails = [];
  const read = (f) => JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8'));
  const rep = read('conversion-report.json');
  const man = read('feature-manifest.json');
  const ts = read('tileset.json');

  // 1
  if (rep.manifestOccurrences !== man.occurrenceCount) {
    fails.push('occurrence mismatch: report=' + rep.manifestOccurrences
      + ' manifest=' + man.occurrenceCount);
  }

  // tileset 里真正发布出去的 content uri
  const published = new Set();
  (function walk(t) {
    if (!t) return;
    const put = (c) => { if (typeof c === 'string') published.add(c);
      else if (c && typeof c.uri === 'string') published.add(c.uri); };
    if (Array.isArray(t.content)) t.content.forEach(put); else put(t.content);
    (t.children || []).forEach(walk);
  })(ts.root);

  const agg = man.aggregateProxies || {};
  const featKeys = new Set(Object.keys(man.features || {}));

  // 2：每个有内容的聚合代理瓦片都要有 manifest 条目
  const withContent = (rep.tiles || []).filter((t) => t.aggregate && t.uri && published.has(t.uri));
  for (const t of withContent) {
    const e = agg[t.uri];
    if (!e || Object.keys(e).length === 0) { fails.push('no aggregateProxies entry for ' + t.uri); continue; }
    let dangling = 0, ids = 0;
    for (const [pid, src] of Object.entries(e)) {
      if (!/^\d+$/.test(pid)) { fails.push('proxy id not numeric in ' + t.uri + ': ' + pid); }
      if (!Array.isArray(src) || src.length === 0) { fails.push('empty source set for proxy ' + pid + ' in ' + t.uri); continue; }
      for (const s of src) { ids++; if (!featKeys.has(String(s))) dangling++; }
    }
    if (dangling) fails.push(t.uri + ': ' + dangling + ' proxy source ids not in manifest.features');
  }
  // 反向：manifest 里的 key 必须是已发布的瓦片
  for (const k of Object.keys(agg)) if (!published.has(k)) fails.push('aggregateProxies key not published: ' + JSON.stringify(k));

  // 3
  const built = (rep.aggregateProxyCount || 0) - (rep.proxySkippedCount || 0);
  if (built !== withContent.length) {
    fails.push('aggregate count mismatch: built-skipped=' + built + ' published-with-content=' + withContent.length
      + ' (aggregateProxyCount=' + rep.aggregateProxyCount + ' proxySkippedCount=' + rep.proxySkippedCount + ')');
  }

  return { name: path.basename(dir.replace(/[\\/]+$/, '')),
    tiles: published.size, aggProxies: Object.keys(agg).length,
    aggBuilt: rep.aggregateProxyCount || 0, aggSkipped: rep.proxySkippedCount || 0,
    maxTris: rep.maxTrianglesInTile, occ: man.occurrenceCount, fails };
}

let bad = 0, ran = 0;
const rows = [];
for (const arg of process.argv.slice(2)) {
  let dirs;
  try { dirs = datasetDirs(arg); } catch (e) { console.error('SKIP ' + arg + ': ' + e.message); continue; }
  for (const d of dirs) {
    let r;
    try { r = check(d); } catch (e) { console.error('ERROR ' + d + ': ' + e.message); bad++; continue; }
    ran++;
    rows.push(r);
    if (r.fails.length) bad++;
  }
}

console.log('\n| dataset | tiles | aggProxies | aggBuilt | aggSkipped | maxTris/tile | occurrences | 断言 |');
console.log('|---|---|---|---|---|---|---|---|');
for (const r of rows) {
  console.log('| ' + [r.name, r.tiles, r.aggProxies, r.aggBuilt, r.aggSkipped,
    r.maxTris, r.occ, r.fails.length ? 'FAIL' : 'PASS'].join(' | ') + ' |');
}
for (const r of rows) for (const f of r.fails) console.log('  FAIL ' + r.name + ': ' + f);
console.log('\ndatasets=' + ran + '  failures=' + bad);
process.exit(bad ? 1 : 0);
