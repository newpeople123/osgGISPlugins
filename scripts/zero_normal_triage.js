// 零长度法线定性：区分"退化几何本来就该是 0"与"法线本该算得出却是 0"。
//
// 官方 3d-tiles-validator 只报 `is not of unit length: 0`，不告诉你那个面是不是退化的，
// 所以单看条数无法判断严重性。这里按三角形面积把每个零长度法线归类：
//   - 只出现在面积≈0 的三角形上 -> DEGENERATE（良性，简化器留下的退化面）
//   - 出现在任一面积>0 的三角形上 -> REAL-AREA（真隐患，法线本该算得出）
// 面积用两条边叉积的模（= 平行四边形面积，2× 三角形面积），阈值 1e-9。
//
// 用法: node scripts/zero_normal_triage.js <数据集目录|文件> [--eps 1e-9]
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
const files = [];
for (const a of process.argv.slice(2)) {
    if (a === '--eps') { continue; }
    if (/^[0-9.e-]+$/.test(a)) { continue; }        // --eps 的值
    if (!fs.existsSync(a)) { console.log('missing: ' + a); continue; }
    if (fs.statSync(a).isDirectory()) {
        const dir = fs.existsSync(path.join(a, 'tiles')) ? path.join(a, 'tiles') : a;
        for (const f of fs.readdirSync(dir)) {
            if (/\.(b3dm|i3dm|cmpt)$/i.test(f)) { files.push(path.join(dir, f)); }
        }
    } else { files.push(a); }
}
if (!files.length) {
    console.log('usage: node scripts/zero_normal_triage.js <datasetDir|file> [...] [--eps 1e-9]');
    process.exit(2);
}

let nZero = 0, degenerate = 0, realArea = 0, noIndex = 0;
const byFile = new Map();
const byVertex = new Map();

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
                const onRealAreaTri = new Set();
                for (let i = 0; i + 2 < idx.length; i += 3) {
                    const a = P(idx[i]), c = P(idx[i + 1]), e = P(idx[i + 2]);
                    const u = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
                    const v = [e[0] - a[0], e[1] - a[1], e[2] - a[2]];
                    const cr = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
                    if (Math.hypot(cr[0], cr[1], cr[2]) > EPS) {
                        onRealAreaTri.add(idx[i]); onRealAreaTri.add(idx[i + 1]); onRealAreaTri.add(idx[i + 2]);
                    }
                }
                for (const z of zeros) {
                    nZero++;
                    if (!idx.length) { noIndex++; continue; }      // DrawArrays：无从归属
                    if (onRealAreaTri.has(z)) {
                        realArea++;
                        const rel = path.basename(file);
                        byFile.set(rel, (byFile.get(rel) || 0) + 1);
                        const key = 'mesh' + meshIdx + ' v' + z;
                        byVertex.set(key, (byVertex.get(key) || 0) + 1);
                    } else {
                        degenerate++;
                    }
                }
            }
            meshIdx++;
        }
    });
}

console.log('files=' + files.length + '  eps=' + EPS.toExponential(0));
console.log('零长度 NORMAL 顶点 = ' + nZero);
console.log('  只出现在退化(面积≈0)三角形上 = ' + degenerate + '   -> 良性');
console.log('  出现在某个非退化三角形上     = ' + realArea + '   -> 真隐患（法线本该算得出）');
console.log('  无索引图元(DrawArrays)无法归属 = ' + noIndex);
if (byFile.size) {
    console.log('受影响文件 ' + byFile.size + ' 个:');
    for (const [k, v] of [...byFile].sort((a, b) => b[1] - a[1]).slice(0, 20)) { console.log('  ' + k + ': ' + v); }
    console.log('顶点签名（按出现次数）:');
    for (const [k, v] of [...byVertex].sort((a, b) => b[1] - a[1]).slice(0, 15)) { console.log('  ' + k + ' x' + v); }
}
