// A-43 写出侧修复的可行性分档：每个零长度 NORMAL 顶点，能不能从它自己所在的
// 非退化三角形拿到面法线？拿不到时，同一图元里还有没有别的有效面可以兜底？
//
// 官方 validator 只看向量长度，不看三角形退化与否，所以 195 条 ERROR 需要分开看：
//   A 档 = 自身至少有一个非退化入射面  -> 直接用入射面法线（面积加权平均）
//   B 档 = 自身入射面全退化，但同图元存在非退化面 -> 需要图元级兜底
//   C 档 = 整个图元没有任何非退化面（或无索引）-> 无法从几何推出法线
// "非退化"沿用 zero_normal_triage.js 的主判据：面积 > REL × 本图元中位面积。
//
// 用法: node scripts/zero_normal_repair_plan.js <数据集目录|文件> [...] [--rel 1e-6]
const fs = require('fs');
const path = require('path');

function parseTile(buf, tileOff) {
    const magic = buf.toString('latin1', tileOff, tileOff + 4);
    const hdr = magic === 'i3dm' ? 32 : 28;
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

const relArg = process.argv.indexOf('--rel');
const REL = relArg > 0 ? Number(process.argv[relArg + 1]) : 1e-6;
const files = [];
for (const a of process.argv.slice(2)) {
    if (a === '--rel') { continue; }
    if (/^[0-9.e-]+$/.test(a)) { continue; }
    if (!fs.existsSync(a)) { console.log('missing: ' + a); continue; }
    if (fs.statSync(a).isDirectory()) {
        const dir = fs.existsSync(path.join(a, 'tiles')) ? path.join(a, 'tiles') : a;
        for (const f of fs.readdirSync(dir)) {
            if (/\.(b3dm|i3dm|cmpt)$/i.test(f)) { files.push(path.join(dir, f)); }
        }
    } else { files.push(a); }
}
if (!files.length) {
    console.log('usage: node scripts/zero_normal_repair_plan.js <datasetDir|file> [...] [--rel 1e-6]');
    process.exit(2);
}

let A = 0, B = 0, C = 0, noIdx = 0, total = 0;
const filesWithZero = new Set();
const bucketByFile = new Map();

for (const file of files) {
    const buf = fs.readFileSync(file);
    const name = path.basename(file);
    walk(buf, 0, (off) => {
        const t = parseTile(buf, off);
        if (!t) { return; }
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
                    if (!(L > 1e-6)) { zeros.push(i); }     // 含 NaN：长度算不出即不可用
                }
                if (!zeros.length) { continue; }
                filesWithZero.add(name);
                const idx = prim.indices != null
                    ? indexList(buf, t.binOff, t.json, t.json.accessors[prim.indices]) : [];
                if (!idx.length) { noIdx += zeros.length; total += zeros.length; continue; }
                const pbv = t.json.bufferViews[posA.bufferView] || {};
                const pStride = pbv.byteStride || 12;
                const pBase = t.binOff + (pbv.byteOffset || 0) + (posA.byteOffset || 0);
                const P = (i) => {
                    const b = pBase + i * pStride;
                    return [buf.readFloatLE(b), buf.readFloatLE(b + 4), buf.readFloatLE(b + 8)];
                };
                const areaOf = (v) => {
                    const p0 = P(v[0]), p1 = P(v[1]), p2 = P(v[2]);
                    const u = [p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]];
                    const w = [p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]];
                    const cr = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]];
                    const ar = Math.hypot(cr[0], cr[1], cr[2]);
                    return Number.isFinite(ar) ? ar : 0;
                };
                const tris = [];
                for (let i = 0; i + 2 < idx.length; i += 3) {
                    const v = [idx[i], idx[i + 1], idx[i + 2]];
                    tris.push({ v, area: areaOf(v) });
                }
                const sorted = tris.map((x) => x.area).sort((x, y) => x - y);
                const med = sorted.length ? sorted[sorted.length >> 1] : 0;
                const good = (ar) => ar > REL * med && ar > 0;
                const inc = new Map();       // 顶点 -> 入射非退化面数
                let primGood = 0;
                for (const a of tris) {
                    if (!good(a.area)) { continue; }
                    primGood++;
                    for (const v of a.v) { inc.set(v, (inc.get(v) || 0) + 1); }
                }
                const z = new Set(zeros);
                for (const i of zeros) {
                    total++;
                    let bucket;
                    if ((inc.get(i) || 0) > 0) { bucket = 'A'; A++; }
                    else if (primGood > 0) { bucket = 'B'; B++; }
                    else { bucket = 'C'; C++; }
                    const k = name + '/' + bucket;
                    bucketByFile.set(k, (bucketByFile.get(k) || 0) + 1);
                    if (z.size && process.env.ZGRP) {
                        console.log('  ' + name + ' v' + i + ' -> ' + bucket
                            + ' 图元非退化面=' + primGood + ' 三角形=' + tris.length);
                    }
                }
            }
        }
    });
}

console.log('files=' + files.length + '  rel=' + REL.toExponential(0) + '×本图元中位面积');
console.log('零长度 NORMAL 顶点 = ' + total + '（涉及 ' + filesWithZero.size + ' 个内容文件）');
console.log('  A 档 自身入射面可算出面法线   = ' + A);
console.log('  B 档 自身全退化、同图元有-good = ' + B + '   -> 需要图元级兜底');
console.log('  C 档 整图元无非退化面         = ' + C + '   -> 几何推不出法线');
console.log('  无索引图元（归属不了）        = ' + noIdx);
const byB = new Map();
for (const [k, v] of bucketByFile) {
    const b = k.split('/')[1];
    byB.set(b, (byB.get(b) || 0) + v);
}
console.log('逐档涉及文件数: ' + [...byB].map(([b, n]) => b + '=' + n).join(' '));
