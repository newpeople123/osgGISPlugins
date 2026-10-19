// 逐 primitive 对齐 POSITION / NORMAL / 索引，用来判断"荒谬顶点"和"零长度法线"
// 是不是同一个顶点的两种表现（A-38 取证）。validator 只报 accessor 元素下标，
// 看不出它属于哪个图元、是否被索引引用，这里补齐。
//
// 用法: node scripts/b3dm_attr_probe.js <file.b3dm> [--abs 1e6]
const fs = require('fs');

function parseTile(buf, tileOff) {
    const magic = buf.toString('latin1', tileOff, tileOff + 4);
    const hdr = magic === 'i3dm' ? 32 : 28;
    const ftj = buf.readUInt32LE(tileOff + 12);
    const ftb = buf.readUInt32LE(tileOff + 16);
    const btj = buf.readUInt32LE(tileOff + 20);
    const btb = buf.readUInt32LE(tileOff + 24);
    const glbOff = tileOff + hdr + ftj + ftb + btj + btb;
    if (buf.toString('latin1', glbOff, glbOff + 4) !== 'glTF') {
        throw new Error('no GLB at ' + glbOff);
    }
    const jsonLen = buf.readUInt32LE(glbOff + 12);
    const json = JSON.parse(buf.toString('utf8', glbOff + 20, glbOff + 20 + jsonLen));
    // GLB: 12B 头 + (8B 块头 + JSON) + (8B 块头 + BIN)
    return { json, binOff: glbOff + 20 + jsonLen + 8 };
}

function vecAt(buf, binOff, acc, i, out) {
    const bv = acc._bv;
    const stride = bv.byteStride || acc._elemSize;
    const base = binOff + bv.byteOffset + (acc.byteOffset || 0) + i * stride;
    for (let c = 0; c < acc._comps; ++c) { out[c] = buf.readFloatLE(base + c * 4); }
    return out;
}

function readIndex(buf, binOff, acc, i) {
    const bv = acc._bv;
    const base = binOff + bv.byteOffset + (acc.byteOffset || 0) + i * acc._elemSize;
    switch (acc.componentType) {
        case 5121: return buf.readUInt8(base);
        case 5123: return buf.readUInt16LE(base);
        default:   return buf.readUInt32LE(base);
    }
}

function prep(json, binOff, buf) {
    const COMP = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4 };
    const SIZE = { 5121: 1, 5123: 2, 5125: 4, 5126: 4 };
    for (const a of json.accessors || []) {
        a._bv = (json.bufferViews || [])[a.bufferView] || {};
        a._comps = COMP[a.type] || 1;
        // 元素步长 = 分量数 × 分量字节数。这里写成分量字节数会让 VEC3 数组
        // 只扫到前 1/4（i*4 而不是 i*12），"越界顶点读不到"就是这么来的。
        a._elemSize = a._comps * (SIZE[a.componentType] || 4);
        if (a.componentType !== 5126 && a.type !== 'SCALAR') {
            console.log(`  ! accessor ${a.type} is not float; probe only reads float`);
        }
    }
    void binOff; void buf;
}

const absArg = process.argv.indexOf('--abs');
const ABS = absArg > 0 ? Number(process.argv[absArg + 1]) : 1e6;
const VERBOSE = process.argv.indexOf('--all') > 0;
const inputs = process.argv.slice(2).filter(a => a !== '--all' && a !== '--abs');
const files = [];
for (const a of inputs) {
    if (!fs.existsSync(a)) { console.log(`missing: ${a}`); continue; }
    if (fs.statSync(a).isDirectory()) {
        // 数据集目录：优先 tiles/，否则目录本身
        const dir = fs.existsSync(`${a}/tiles`) ? `${a}/tiles` : a;
        for (const f of fs.readdirSync(dir)) {
            if (/\.(b3dm|i3dm|cmpt)$/i.test(f)) { files.push(`${dir}/${f}`); }
        }
    } else { files.push(a); }
}
if (!files.length) {
    console.log('usage: node scripts/b3dm_attr_probe.js <file|datasetDir> [...] [--abs N] [--all]');
    process.exit(2);
}

let badFiles = 0;
for (const file of files) {
    const buf = fs.readFileSync(file);
    let json, binOff;
    try { ({ json, binOff } = parseTile(buf, 0)); } catch (e) { console.log(`${file} PARSE-FAIL ${e.message}`); continue; }
    prep(json, binOff, buf);

    const report = [];
    let nBadPos = 0, nZeroNrm = 0, nBadNrm = 0;
    for (let mi = 0; mi < (json.meshes || []).length; ++mi) {
        const mesh = json.meshes[mi];
        for (let pi = 0; pi < (mesh.primitives || []).length; ++pi) {
            const prim = mesh.primitives[pi];
            const posA = json.accessors[prim.attributes.POSITION];
            const nrmA = prim.attributes.NORMAL ? json.accessors[prim.attributes.NORMAL] : null;
            const idxA = prim.indices != null ? json.accessors[prim.indices] : null;
            if (!posA) { continue; }
            const p = [0, 0, 0], n = [0, 0, 0];
            const badPos = [], badNrm = [], zeroNrm = [];
            for (let i = 0; i < posA.count; ++i) {
                vecAt(buf, binOff, posA, i, p);
                let absurd = false, nonFinite = false;
                for (let c = 0; c < 3; ++c) {
                    if (!Number.isFinite(p[c])) { nonFinite = true; }
                    else if (Math.abs(p[c]) > ABS) { absurd = true; }
                }
                if (absurd || nonFinite) { badPos.push({ i, v: p.slice(), count: posA.count }); }
            }
            if (nrmA) {
                for (let i = 0; i < nrmA.count; ++i) {
                    vecAt(buf, binOff, nrmA, i, n);
                    const len = Math.hypot(n[0], n[1], n[2]);
                    if (!Number.isFinite(len)) { badNrm.push({ i, v: n.slice() }); }
                    else if (len < 1e-6) { zeroNrm.push({ i, v: n.slice(), count: nrmA.count }); }
                }
            }
            const referenced = new Set();
            if (idxA) {
                for (let i = 0; i < idxA.count; ++i) { referenced.add(readIndex(buf, binOff, idxA, i)); }
            }
            // 落盘数据的实测 min/max：declared 与实测不一致时，说明写 accessor 元数据
            // 和写顶点缓冲读到的不是同一份状态（A-38 就是这样露出来的）。
            const cmx = [-Infinity, -Infinity, -Infinity];
            for (let i = 0; i < posA.count; ++i) {
                vecAt(buf, binOff, posA, i, p);
                for (let c = 0; c < 3; ++c) { if (p[c] > cmx[c]) { cmx[c] = p[c]; } }
            }
            nBadPos += badPos.length; nZeroNrm += zeroNrm.length; nBadNrm += badNrm.length;
            // 只有零长度法线时不打：退化面的法线本来就是 0，那是夹具性质不是损坏签名
            if (!badPos.length && !badNrm.length && !VERBOSE) { continue; }
            const fmt3 = (v) => v.map(x => x.toExponential(3)).join(', ');
            const tag = (e) => `v${e.i}`
                + `${e.count !== undefined ? `(@count-1=${e.count - 1 - e.i})` : ''}`
                + `${e.v ? '[' + e.v.map(x => x.toExponential(2)).join(',') + ']' : ''}`
                + `${referenced.has(e.i) ? ' [referenced]' : ' [unreferenced]'}`;
            report.push(`  mesh ${mi} prim ${pi}: pos=${posA.count} nrm=${nrmA ? nrmA.count : '-'} idx=${idxA ? idxA.count : '-'}`);
            report.push(`      pos data max=[${fmt3(cmx)}] declared max=[${(posA.max || [-1,-1,-1]).join(', ')}]`);
            if (badPos.length) { report.push(`      BAD POSITION (|x|>${ABS.toExponential(1)} or non-finite): ${badPos.length} -> ${badPos.slice(0, 6).map(tag).join(' ')}`); }
            if (zeroNrm.length) { report.push(`      ZERO-LENGTH NORMAL: ${zeroNrm.length} -> ${zeroNrm.slice(0, 8).map(tag).join(' ')}`); }
            if (badNrm.length) { report.push(`      NON-FINITE NORMAL: ${badNrm.length} -> ${badNrm.slice(0, 6).map(tag).join(' ')}`); }
            if (nrmA && badPos.length) {
                const same = badPos.filter(e => zeroNrm.some(z => z.i === e.i)).map(e => e.i);
                report.push(`      SAME INDEX BAD POS + ZERO NORMAL: ${same.length ? same.join(',') : 'none'}`);
            }
        }
    }
    if (nBadPos || nZeroNrm || nBadNrm) { ++badFiles; }
    if (report.length) {
        console.log(`${file}  badPos=${nBadPos} zeroNrm=${nZeroNrm} badNrm=${nBadNrm}`);
        console.log(report.join('\n'));
    }
}
console.log(`\nfiles=${files.length} withFindings=${badFiles}`);
