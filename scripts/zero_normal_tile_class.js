// A-43 取证第二步：把"含零长度 NORMAL 的瓦片"与 conversion-report.json 的瓦片类别
// （isProxy / isLeaf / aggregate / errorStatus）对上，用来回答"这些 0 法线是谁产生的"。
// 只看面积分档回答不了这一条：代理瓦片走过简化器，EXACT_LEAF 叶瓦片没有。
//
// 用法：node scripts/zero_normal_tile_class.js <数据集目录>
//       （数据集目录里要有 tiles/ 与 conversion-report.json）
const fs = require('fs');
const path = require('path');

const root = process.argv[2];
if (!root) { console.log('usage: node scripts/zero_normal_tile_class.js <datasetDir>'); process.exit(2); }
const dir = fs.existsSync(path.join(root, 'tiles')) ? path.join(root, 'tiles') : root;
const repFile = path.join(root, 'conversion-report.json');
if (!fs.existsSync(repFile)) { console.log('missing conversion-report.json in ' + root); process.exit(2); }
const rep = JSON.parse(fs.readFileSync(repFile, 'utf8'));
const byName = new Map((rep.tiles || []).map((t) => [path.basename(t.uri), t]));

function parseTile(buf, off) {
  const magic = buf.toString('latin1', off, off + 4);
  const hdr = magic === 'i3dm' ? 32 : 28;      // i3dm 多一个 GLTF_FORMAT 字段
  const ftj = buf.readUInt32LE(off + 12), ftb = buf.readUInt32LE(off + 16);
  const btj = buf.readUInt32LE(off + 20), btb = buf.readUInt32LE(off + 24);
  const g = off + hdr + ftj + ftb + btj + btb;
  if (buf.toString('latin1', g, g + 4) !== 'glTF') return null;
  const jsonLen = buf.readUInt32LE(g + 12);
  return { json: JSON.parse(buf.toString('utf8', g + 20, g + 20 + jsonLen)), binOff: g + 20 + jsonLen + 8 };
}
function walk(buf, off, fn) {
  const m = buf.toString('latin1', off, off + 4);
  if (m === 'cmpt') {
    const n = buf.readUInt32LE(off + 8);
    let p = off + 12;
    for (let i = 0; i < n; i++) { fn(p); p += buf.readUInt32LE(p + 4); }
  } else if (m === 'b3dm' || m === 'i3dm') { fn(off); }
}

const rows = [];
for (const f of fs.readdirSync(dir)) {
  if (!/\.(b3dm|i3dm|cmpt)$/i.test(f)) continue;
  const buf = fs.readFileSync(path.join(dir, f));
  let zeros = 0;
  walk(buf, 0, (off) => {
    const t = parseTile(buf, off); if (!t) return;
    for (const mesh of (t.json.meshes || [])) for (const prim of (mesh.primitives || [])) {
      const a = prim.attributes.NORMAL != null ? t.json.accessors[prim.attributes.NORMAL] : null;
      if (!a || a.componentType !== 5126) continue;          // 非 float 法线不在本判据范围
      const bv = t.json.bufferViews[a.bufferView] || {};
      const stride = bv.byteStride || 12;
      const base = t.binOff + (bv.byteOffset || 0) + (a.byteOffset || 0);
      for (let i = 0; i < a.count; i++) {
        const b = base + i * stride;
        if (Math.hypot(buf.readFloatLE(b), buf.readFloatLE(b + 4), buf.readFloatLE(b + 8)) < 1e-6) zeros++;
      }
    }
  });
  if (!zeros) continue;
  const r = byName.get(f) || {};
  rows.push({ f, zeros, isProxy: !!r.isProxy, isLeaf: !!r.isLeaf, aggregate: !!r.aggregate,
              tris: r.triangles, status: r.errorStatus });
}
rows.sort((a, b) => b.zeros - a.zeros);
console.log('| file | 零法线 | isProxy | isLeaf | aggregate | triangles | errorStatus |');
for (const r of rows) {
  console.log('| ' + [r.f, r.zeros, r.isProxy, r.isLeaf, r.aggregate, r.tris, r.status || '(不在 report)'].join(' | ') + ' |');
}
const g = (k) => rows.reduce((a, r) => a + (r[k] ? r.zeros : 0), 0);
console.log('\nfiles=' + rows.length + '  zeros=' + rows.reduce((a, r) => a + r.zeros, 0)
  + '  代理瓦片=' + g('isProxy') + '  叶瓦片=' + g('isLeaf') + '  聚合代理=' + g('aggregate')
  + '  report 未标=' + rows.filter((r) => !r.status).length);
