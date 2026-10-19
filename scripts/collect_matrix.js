// Reads the conversion-report.json + tileset.json produced by run_matrix.sh and
// prints the acceptance table (hard limits from handbook §3 + coverage counters).
// Usage: node scripts/collect_matrix.js <runsDir>
const fs = require('fs');
const path = require('path');
const out = process.argv[2];
const rows = [];
for (const name of fs.readdirSync(out)) {
  const dir = path.join(out, name);
  const rep = path.join(dir, 'conversion-report.json');
  if (!fs.statSync(dir).isDirectory() || !fs.existsSync(rep)) continue;
  const r = JSON.parse(fs.readFileSync(rep, 'utf8'));
  let ts = null;
  const tsp = path.join(dir, 'tileset.json');
  if (fs.existsSync(tsp)) ts = JSON.parse(fs.readFileSync(tsp, 'utf8'));
  let n = 0, depth = 0, content = 0;
  const seen = new Set();
  if (ts) (function w(t, d) {
    if (!t) return;
    n++; depth = Math.max(depth, d);
    if (t.content && t.content.uri) { content++; if (seen.has(t.content.uri)) console.log('DUP URI ' + t.content.uri); seen.add(t.content.uri); }
    (t.children || []).forEach(c => w(c, d + 1));
  })(ts.root, 0);
  const pr = (r.profile && r.profile.budget) ? r.profile.budget : {};
  rows.push({
    asset: name, rc: r.errors.length === 0 && !!ts ? 0 : 1,
    sec: (r.totalSeconds || 0).toFixed(2),
    rss: Math.round((r.peakRssBytes || 0) / 1048576 * 10) / 10,
    tiles: r.tileCount, tsTiles: n, depth: r.maxDepth, tsDepth: depth,
    content: r.contentTileCount, tsContent: content,
    maxBytes: r.maxContentBytes, maxTri: r.maxTrianglesInTile,
    errMax: (r.errorMaxMeters || 0).toFixed(2), errP95: (r.errorP95Meters || 0).toFixed(2),
    errMean: (r.errorMeanMeters || 0).toFixed(2),
    cert: r.nodesCertified, loose: r.nodesBoundLoose, agg: r.aggregateProxyCount,
    skip: r.proxySkippedCount === undefined ? '-' : r.proxySkippedCount,
    inst: r.instanceTileCount, srcF: r.sourceFeatureCount, occ: r.manifestOccurrences,
    missing: r.featuresMissingFromLeaves, dropped: r.proxyDroppedFeatureCount,
    warn: (r.warnings || []).length,
    budgetBytes: pr.maxTileContentBytes || 8388608, budgetTri: pr.maxTrianglesPerTile || 120000,
  });
}
const fmt = (x, w) => String(x).padEnd(w);
console.log(fmt('asset', 22) + fmt('rc', 3) + fmt('sec', 7) + fmt('MiB', 7) + fmt('tiles', 7)
  + fmt('depth', 6) + fmt('cont', 6) + fmt('maxBytes', 10) + fmt('maxTri', 8) + fmt('errMax', 12)
  + fmt('cert/loose', 11) + fmt('agg/skip', 9) + fmt('inst', 5) + fmt('src/occ', 11)
  + fmt('miss/drop', 10) + 'warn');
let bad = 0;
for (const r of rows.sort((a, b) => a.asset.localeCompare(b.asset))) {
  const over = r.maxBytes > r.budgetBytes || r.maxTri > r.budgetTri || r.depth > 48
    || r.missing > 0 || r.rc !== 0;
  if (over) bad++;
  console.log(fmt(r.asset, 22) + fmt(r.rc, 3) + fmt(r.sec, 7) + fmt(r.rss, 7) + fmt(r.tiles, 7)
    + fmt(r.depth, 6) + fmt(r.content, 6) + fmt(r.maxBytes, 10) + fmt(r.maxTri, 8)
    + fmt(r.errMax, 12) + fmt(r.cert + '/' + r.loose, 11)
    + fmt(r.agg + '/' + r.skip, 9) + fmt(r.inst, 5) + fmt(r.srcF + '/' + r.occ, 11)
    + fmt(r.missing + '/' + r.dropped, 10) + r.warn + (over ? '   <== LIMIT VIOLATION' : ''));
}
console.log('assets=' + rows.length + ' limitViolations=' + bad);
