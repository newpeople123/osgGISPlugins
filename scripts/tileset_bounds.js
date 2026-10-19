// 校验 tileset.json 里"父包围盒覆盖子包围盒"（3D Tiles 1.0 §10.2.2 语义要求，
// Cesium 用它做视锥剔除与 error 计算；父不覆盖子会导致子树被误剔除）。
// tileset.box 是 [cx,cy,cz, xx,0,0, 0,yy,0, 0,0,zz]（轴对齐，无旋转）。
// 用法：node scripts/tileset_bounds.js <数据集目录|容器目录> [...]
// 退出码：有违例 = 1。
const fs = require('fs');
const path = require('path');

const EPS_REL = 1e-6;

function datasetDirs(arg) {
  const st = fs.statSync(arg);
  if (!st.isDirectory()) throw new Error('not a directory: ' + arg);
  if (fs.existsSync(path.join(arg, 'tileset.json'))) return [arg];
  return fs.readdirSync(arg).map((n) => path.join(arg, n))
    .filter((p) => fs.existsSync(path.join(p, 'tileset.json')));
}

// 返回该数据集的违例列表；每项 {depth, axis, over, parentUri, childUri}
function checkDataset(dir) {
  const ts = JSON.parse(fs.readFileSync(path.join(dir, 'tileset.json'), 'utf8'));
  const issues = [];
  let nan = 0, negHalf = 0, badGe = 0;

  function box(t) {
    const b = t.boundingVolume && t.boundingVolume.box;
    if (!Array.isArray(b) || b.length !== 12) return null;
    if (!b.every((v) => Number.isFinite(v))) { nan++; return null; }
    if (b[3] < 0 || b[7] < 0 || b[11] < 0) { negHalf++; }
    return b;
  }
  function covered(par, child) {
    const ax = [[0, 3], [1, 7], [2, 11]];
    const eps = EPS_REL * Math.max(1, par[3], par[7], par[11]);
    for (const [ci, hi] of ax) {
      const over = Math.abs(child[ci] - par[ci]) + child[hi] - par[hi];
      if (over > eps) return { axis: ci, over };
    }
    return null;
  }

  (function walk(t, depth, parent, uri) {
    if (t.geometricError !== undefined &&
        (!Number.isFinite(t.geometricError) || t.geometricError < 0)) { badGe++; }
    const b = box(t);
    const myUri = (t.content && (typeof t.content === 'string' ? t.content
                 : t.content.uri)) || uri;
    if (parent && b) {
      const bad = covered(parent, b);
      if (bad) {
        issues.push({ depth, axis: bad.axis, over: bad.over,
          parentUri: parent.__uri || '(root)', childUri: myUri || '(no content)' });
      }
    }
    if (b) b.__uri = myUri || '(no content)';
    (t.children || []).forEach((c) => walk(c, depth + 1, b, myUri));
  })(ts.root, 0, null, null);

  return { name: path.basename(dir.replace(/[\\/]+$/, '')), issues, nan, negHalf, badGe };
}

let bad = 0;
const rows = [];
for (const arg of process.argv.slice(2)) {
  let dirs;
  try { dirs = datasetDirs(arg); } catch (e) { console.error('SKIP ' + arg + ': ' + e.message); continue; }
  for (const d of dirs) {
    let r;
    try { r = checkDataset(d); } catch (e) { console.error('ERROR ' + d + ': ' + e.message); bad++; continue; }
    rows.push(r);
    if (r.issues.length || r.nan || r.negHalf || r.badGe) bad++;
  }
}

console.log('\n| dataset | 父不覆盖子 | NaN box | 负半轴 | 非法 geometricError | 示例(深度/轴/超出) |');
console.log('|---|---|---|---|---|---|');
for (const r of rows) {
  const ex = r.issues.slice(0, 3).map((i) => 'd' + i.depth + '/ax' + i.axis + '/'
    + (i.over < 0.01 ? i.over.toExponential(2) : i.over.toFixed(3))).join(' ');
  console.log('| ' + [r.name, r.issues.length, r.nan, r.negHalf, r.badGe, ex || '-'].join(' | ') + ' |');
}
const tot = rows.reduce((a, r) => a + r.issues.length, 0);
console.log('\ndatasets=' + rows.length + '  父不覆盖子合计=' + tot + '  有问题的数据集=' + bad);
process.exit(bad ? 1 : 0);
