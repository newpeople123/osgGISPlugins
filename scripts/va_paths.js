// 视觉验收按相机路径的明细：每个会话的每条路径给出
// 帧数 / 画出了剪影的帧 / 空洞帧（silhouette=0）/ cameraFinite=false 的帧。
// va_summary.js 只按会话聚合，看不出"某条路径整条 NaN"这类系统性失败。
// 用法：node scripts/va_paths.js <sessionFilter> [...]
// 判据口径见 docs/visual-acceptance/README.md「每帧量化度量」。
const fs = require('fs');
const path = require('path');

const runsRoot = path.join(__dirname, '..', 'docs', 'visual-acceptance', 'runs');
const filters = process.argv.slice(2);

function shots(file) {
  const txt = fs.readFileSync(file, 'utf8').trim();
  if (!txt) return [];
  return txt.split('\n').map((l) => { try { return JSON.parse(l); } catch (e) { return null; } })
    .filter((r) => r && r.kind === 'shot');
}

const rows = [];
for (const session of fs.readdirSync(runsRoot).sort()) {
  if (filters.length && !filters.some((f) => session.includes(f))) continue;
  const file = path.join(runsRoot, session, 'manifest.jsonl');
  if (!fs.existsSync(file)) continue;
  const byPath = new Map();
  for (const r of shots(file)) {
    const key = r.frame || r.path || '?';
    if (!byPath.has(key)) byPath.set(key, []);
    byPath.get(key).push(r);
  }
  for (const [key, v] of byPath) {
    const sil = (r) => (r.silhouette && r.silhouette.px) || 0;
    // 与 README「每帧量化度量」同口径：投影半径 <=20 px 的帧没资格判 HOLE，
    // 只记 tooSmall（贴 refine 阈值时薄特征本来就退化成亚像素）。
    const proj = (r) => Math.max(r.projectedRadiusPx || 0, r.contentProjectedRadiusPx || 0);
    rows.push({
      session, frame: key, shots: v.length,
      drawn: v.filter((r) => sil(r) > 0).length,
      holes: v.filter((r) => sil(r) === 0 && proj(r) > 20).length,
      tooSmall: v.filter((r) => sil(r) === 0 && proj(r) <= 20).length,
      camBad: v.filter((r) => r.cameraFinite === false).length,
      medSil: (() => { const a = v.map(sil).filter((x) => x > 0).sort((x, y) => x - y);
        return a.length ? a[a.length >> 1] : 0; })(),
      minProj: Math.round(Math.min(...v.map(proj)))
    });
  }
}

const head = ['session', 'frame', 'shots', 'drawn', 'holes', 'tooSmall', 'camBad', 'medSil', 'minProj'];
console.log('| ' + head.join(' | ') + ' |');
console.log('|' + head.map(() => '---').join('|') + '|');
for (const r of rows) {
  console.log('| ' + head.map((k) => r[k]).join(' | ') + ' |');
}
console.log('\nsessions=' + new Set(rows.map((r) => r.session)).size
  + '  paths=' + rows.length
  + '  shots=' + rows.reduce((a, r) => a + r.shots, 0)
  + '  holes=' + rows.reduce((a, r) => a + r.holes, 0)
  + '  tooSmall=' + rows.reduce((a, r) => a + r.tooSmall, 0)
  + '  camBad=' + rows.reduce((a, r) => a + r.camBad, 0));
