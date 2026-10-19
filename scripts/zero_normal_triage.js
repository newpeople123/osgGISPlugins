// 零长度法线定性：区分"退化几何本来就该是 0"与"法线本该算得出却是 0"。
//
// 官方 3d-tiles-validator 只报 `is not of unit length: 0`，不告诉你那个面是不是退化的，
// 所以单看条数无法判断严重性。这里按三角形面积把每个零长度法线归类：
//   - 只落在退化三角形上     -> 良性（常见来源：简化器留下的退化面）
//   - 落在任一非退化三角形上 -> 真隐患（法线本该算得出）
// "退化"给两个判据：**主判据**是面积 > REL × 该 primitive 自身的中位面积（尺度无关，
// 默认 REL=1e-6）；**对照**是旧的绝对阈值 EPS（默认 1e-9）。两者都打印，因为绝对阈值
// 跨尺度会翻转结论：lugouqiao_v15 那批在 1e-8～1e-9 之间摆动时，"真隐患"一档给过
// 36～55 条、"退化"一档给过 140～159 条（两档合计恒为 195），判别力不足以定级。
// 面积用两条边叉积的模（= 平行四边形面积，2× 三角形面积）。
//
// 用法: node scripts/zero_normal_triage.js <数据集目录|文件> [...] [--eps 1e-9] [--rel 1e-6]
const fs = require('fs');
const path = require('path');

function parseTile(buf, tileOff) {
    const magic = buf.toString('latin1', tileOff, tileOff + 4);
    const hdr = magic === 'i3dm' ? 32 : 28;      // i3dm 多一个 GLTF_FORMAT 字段
    const ftj = buf.readUInt32LE(tileOff + 12);
    const ftb = buf.readUInt32LE(tileOff + 16);
    const btj = buf.readUInt32LE(tileOff + 20);
    const btb = buf.readUInt32LE(tileOff + 24);
    const glbOff = tileOff + hdr + ftj + ftb + btj + btb;
    if (buf.toString('latin1', glbOff, glbOff + 4) !== 'glTF') { return null; }
    const jsonLen = buf.readUInt32LE(glbOff + 12);
    return {
        json: JSON.parse(buf.toString('utf8', glbOff + 20, glbOff + 20 + jsonLen)),
        binOff: glbOff + 20 + jsonLen + 8,
    };
}

function walk(buf, off, fn) {
    const m = buf.toString('latin1', off, off + 4);
    if (m === 'cmpt') {
        const n = buf.readUInt32LE(off + 8);
        let p = off + 12;
        for (let i = 0; i < n; i++) { fn(p); p += buf.readUInt32LE(p + 4); }
    } else if (m === 'b3dm' || m === 'i3dm') { fn(off); }
}

function indexList(buf, binOff, json, acc) {
    const bv = json.bufferViews[acc.bufferView] || {};
    const elem = acc.componentType === 5121 ? 1 : acc.componentType === 5123 ? 2 : 4;
    const stride = bv.byteStride || elem;
    const base = binOff + (bv.byteOffset || 0) + (acc.byteOffset || 0);
    const out = [];
    for (let i = 0; i < acc.count; i++) {
        const b = base + i * stride;
        out.push(acc.componentType === 5121 ? buf.readUInt8(b)
            : acc.componentType === 5123 ? buf.readUInt16LE(b) : buf.readUInt32LE(b));
    }
    return out;
}

const epsArg = process.argv.indexOf('--eps');
const EPS = epsArg > 0 ? Number(process.argv[epsArg + 1]) : 1e-9;
const relArg = process.argv.indexOf('--rel');
const REL = relArg > 0 ? Number(process.argv[relArg + 1]) : 1e-6;
const files = [];
for (const a of process.argv.slice(2)) {
    if (a === '--eps' || a === '--rel') { continue; }
    if (/^[0-9.e-]+$/.test(a)) { continue; }        // 选项的值
    if (!fs.existsSync(a)) { console.log('missing: ' + a); continue; }
    if (fs.statSync(a).isDirectory()) {
        const dir = fs.existsSync(path.join(a, 'tiles')) ? path.join(a, 'tiles') : a;
        for (const f of fs.readdirSync(dir)) {
            if (/\.(b3dm|i3dm|cmpt)$/i.test(f)) { files.push(path.join(dir, f)); }
        }
    } else { files.push(a); }
}
if (!files.length) {
    console.log('usage: node scripts/zero_normal_triage.js <datasetDir|file> [...] [--eps 1e-9] [--rel 1e-6]');
    process.exit(2);
}

let nZero = 0, relDeg = 0, relReal = 0, absDeg = 0, absReal = 0, noIndex = 0;
const byFile = new Map();
const byVertex = new Map();
const byPrim = [];

for (const file of files) {
    const buf = fs.readFileSync(file);
    walk(buf, 0, (off) => {
        const t = parseTile(buf, off);
        if (!t) { return; }
        let meshIdx = 0;
        for (const mesh of (t.json.meshes || [])) {
            for (const prim of (mesh.primitives || [])) {
                const nrmA = prim.attributes.NORMAL != null ? t.json.accessors[prim.attributes.NORMAL] : null;
                const posA = prim.attributes.POSITION != null ? t.json.accessors[prim.attributes.POSITION] : null;
                if (!nrmA || !posA || nrmA.componentType !== 5126 || posA.componentType !== 5126) { continue; }
                const nbv = t.json.bufferViews[nrmA.bufferView] || {};
                const nStride = nbv.byteStride || 12;
                const nBase = t.binOff + (nbv.byteOffset || 0) + (nrmA.byteOffset || 0);
                const zeros = [];
                for (let i = 0; i < nrmA.count; i++) {
                    const b = nBase + i * nStride;
                    const L = Math.hypot(buf.readFloatLE(b), buf.readFloatLE(b + 4), buf.readFloatLE(b + 8));
                    if (L < 1e-6) { zeros.push(i); }
                }
                if (!zeros.length) { continue; }
                const idx = prim.indices != null ? indexList(buf, t.binOff, t.json, t.json.accessors[prim.indices]) : [];
                const pbv = t.json.bufferViews[posA.bufferView] || {};
                const pStride = pbv.byteStride || 12;
                const pBase = t.binOff + (pbv.byteOffset || 0) + (posA.byteOffset || 0);
                const P = (i) => {
                    const b = pBase + i * pStride;
                    return [buf.readFloatLE(b), buf.readFloatLE(b + 4), buf.readFloatLE(b + 8)];
                };
                const tris = [];
                for (let i = 0; i + 2 < idx.length; i += 3) {
                    const v = [idx[i], idx[i + 1], idx[i + 2]];
                    const p0 = P(v[0]), p1 = P(v[1]), p2 = P(v[2]);
                    const u = [p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]];
                    const w = [p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]];
                    const cr = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]];
                    tris.push({ v, area: Math.hypot(cr[0], cr[1], cr[2]) });
                }
                // 面积用叉积模（= 2× 三角形面积）。中位面积给"退化"定一个与本
                // primitive 尺度相符的相对阈值：绝对阈值跨尺度没有判别力，同一批
                // 产物在 1e-8 / 1e-9 之间会给出完全不同的结论（实测过）。
                const areasSorted = tris.map((x) => x.area).sort((x, y) => x - y);
                const medArea = areasSorted.length ? areasSorted[areasSorted.length >> 1] : 0;
                const maxArea = areasSorted.length ? areasSorted[areasSorted.length - 1] : 0;
                const onRealTri = (test) => {
                    const s = new Set();
                    for (const a of tris) { if (test(a.area)) { s.add(a.v[0]); s.add(a.v[1]); s.add(a.v[2]); } }
                    return s;
                };
                const relSet = onRealTri((ar) => ar > REL * medArea);
                const absSet = onRealTri((ar) => ar > EPS);
                for (const z of zeros) {
                    nZero++;
                    if (!idx.length) { noIndex++; continue; }      // DrawArrays：无从归属
                    const rel = relSet.has(z), abs = absSet.has(z);
                    if (rel) { relReal++; } else { relDeg++; }
                    if (abs) { absReal++; } else { absDeg++; }
                    if (rel || abs) {
                        const name = path.basename(file);
                        if (rel) {
                            byFile.set(name, (byFile.get(name) || 0) + 1);
                            const key = 'mesh' + meshIdx + ' v' + z;
                            byVertex.set(key, (byVertex.get(key) || 0) + 1);
                        }
                        byPrim.push({ name, mesh: meshIdx, z, medArea, maxArea,
                                      rel, abs, primTris: tris.length, primVerts: zeros.length });
                    }
                }
            }
            meshIdx++;
        }
    });
}

console.log('files=' + files.length + '  rel=' + REL.toExponential(0)
    + '×本 primitive 中位面积  eps(abs)=' + EPS.toExponential(0));
console.log('零长度 NORMAL 顶点 = ' + nZero);
console.log('  [主判据·相对中位面积] 只落在退化三角形 = ' + relDeg
    + ' | 落在非退化三角形 = ' + relReal + '   -> 后者是真隐患（法线本该算得出）');
console.log('  [对照·绝对面积阈值]   只落在退化三角形 = ' + absDeg + ' | 非退化 = ' + absReal);
console.log('  无索引图元(DrawArrays)无法归属 = ' + noIndex);
if (byPrim.length) {
    const uniq = new Map();
    for (const p of byPrim) {
        const k = p.name + '#mesh' + p.mesh;
        if (!uniq.has(k)) { uniq.set(k, { z: 0, p }); }
        const e = uniq.get(k); e.z++;
    }
    console.log('真隐患涉及文件 ' + byFile.size + ' 个、primitive ' + uniq.size + ' 个（按主判据）:');
    for (const [k, v] of [...uniq].slice(0, 12)) {
        console.log('  ' + k + '  零法线顶点=' + v.z + '  该图元三角形=' + v.p.primTris
            + '  中位面积=' + v.p.medArea.toExponential(2) + '  最大面积=' + v.p.maxArea.toExponential(2)
            + '  绝对阈值下' + (v.p.abs ? '同样命中' : '判为退化'));
    }
    console.log('顶点签名（相对判据，按出现次数）:');
    for (const [k, v] of [...byVertex].sort((a, b) => b[1] - a[1]).slice(0, 15)) { console.log('  ' + k + ' x' + v); }
}
