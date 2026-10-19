// i3dm 每实例位姿验收（A-41）。历史上 S3 只验过容器 magic，"位姿有没有落盘"从未验收。
// 逐 i3dm 瓦片核对三件事：
//   1) 特征表 POSITION 有没有非零值（A-41 签名：全 0）
//   2) GLB 节点是否还带 TRS（带 = flatten 没屏蔽干净，会与特征表双变换）
//   3) POSITION 集合能否对上 feature-manifest.json 的实例平移（glTF Y-up 轴序 (x,-z,y)）
// 用法: node scripts/i3dm_pose_check.js <runDir|tile.i3dm> [--top 12]
const fs = require('fs');
const path = require('path');

const TILE_HDR = { i3dm: 32, b3dm: 28, pnts: 28 };

function innerTiles(buf, off, end, out) {
    let p = off;
    while (p + 12 <= end) {
        const magic = buf.toString('latin1', p, p + 4);
        const len = buf.readUInt32LE(p + 8);
        if (!TILE_HDR[magic] && magic !== 'cmpt') break;
        if (len <= 0 || p + len > end) break;
        if (magic === 'cmpt') innerTiles(buf, p + 16, p + len, out);
        else out.push({ magic, off: p, len });
        p += len;
    }
    return out;
}

// i3dm 头：magic@0 ver@4 tileLength@8 ftj@12 ftb@16 btj@20 btb@24 gltfFormat@28，头 32 B
function parseI3dm(buf, off) {
    const ftj = buf.readUInt32LE(off + 12), ftb = buf.readUInt32LE(off + 16);
    const btj = buf.readUInt32LE(off + 20), btb = buf.readUInt32LE(off + 24);
    const ftJson = JSON.parse(buf.toString('utf8', off + 32, off + 32 + ftj));
    const glbOff = off + 32 + ftj + ftb + btj + btb;
    if (buf.toString('latin1', glbOff, glbOff + 4) !== 'glTF') throw new Error('no GLB');
    const jsonLen = buf.readUInt32LE(glbOff + 12);
    const json = JSON.parse(buf.toString('utf8', glbOff + 20, glbOff + 20 + jsonLen));
    return { ftJson, ftBin: off + 32 + ftj, json, glbBin: glbOff + 12 + 8 + jsonLen + 8 };
}

function positions(r, buf) {
    const n = r.ftJson.INSTANCES_LENGTH;
    const po = r.ftJson.POSITION && r.ftJson.POSITION.byteOffset;
    if (n === undefined || po === undefined) return null;
    const out = new Array(n);
    for (let i = 0; i < n; ++i) {
        const o = r.ftBin + po + i * 12;
        out[i] = [buf.readFloatLE(o), buf.readFloatLE(o + 4), buf.readFloatLE(o + 8)];
    }
    return out;
}

// GLB 里原型 POSITION 的 accessor min/max —— A-41 的另一半症状是它被同组实例矩阵累乘
function protoExtent(r, buf) {
    for (const mesh of r.json.meshes || [])
        for (const prim of mesh.primitives || []) {
            const a = r.json.accessors[prim.attributes.POSITION];
            if (!a) continue;
            const bv = r.json.bufferViews[a.bufferView];
            const base = r.glbBin + (bv.byteOffset || 0) + (a.byteOffset || 0);
            const mn = [1e300, 1e300, 1e300], mx = [-1e300, -1e300, -1e300];
            for (let i = 0; i < a.count; ++i)
                for (let c = 0; c < 3; ++c) {
                    const v = buf.readFloatLE(base + i * 12 + c * 4);
                    mn[c] = Math.min(mn[c], v); mx[c] = Math.max(mx[c], v);
                }
            return { count: a.count, declaredMin: a.min, declaredMax: a.max, min: mn, max: mx };
        }
    return null;
}

function nodesWithTrs(json) {
    return (json.nodes || []).filter(nd => nd.translation || nd.rotation || nd.scale || nd.matrix).length;
}

function collect(target) {
    if (fs.statSync(target).isFile()) return [target];
    const out = [];
    (function walk(d) {
        for (const e of fs.readdirSync(d, { withFileTypes: true })) {
            const p = path.join(d, e.name);
            if (e.isDirectory()) walk(p);
            else if (e.name.endsWith('.i3dm')) out.push(p);
        }
    })(target);
    return out.sort();
}

function manifestTranslations(dir) {
    const f = path.join(dir, 'feature-manifest.json');
    if (!fs.existsSync(f)) return null;
    const m = JSON.parse(fs.readFileSync(f, 'utf8'));
    if (!Array.isArray(m.instances)) return null;
    // OSG 行向量约定的列主 4x4：平移在 [12..14]；glTF Y-up 置换 (x,y,z) -> (x,-z,y)
    return m.instances.map(i => { const t = i.transform || i.matrix; return [t[12], -t[14], t[13]]; });
}

const argv = process.argv.slice(2);
const target = argv.find(a => !a.startsWith('--'));
if (!target) { console.error('用法: node scripts/i3dm_pose_check.js <runDir|tile.i3dm>'); process.exit(2); }
const ti = argv.indexOf('--top');
const top = ti < 0 ? 12 : (Number(argv[ti + 1]) || 12);
const files = collect(target);
if (!files.length) { console.error(`${target} 下没有 .i3dm`); process.exit(2); }

const rows = [];
let fail = 0, trsTotal = 0;
for (const f of files) {
    const buf = fs.readFileSync(f);
    const list = buf.toString('latin1', 0, 4) === 'cmpt'
        ? innerTiles(buf, 16, buf.length, []).filter(t => t.magic === 'i3dm')
        : [{ off: 0 }];
    for (const t of list) {
        let r;
        try { r = parseI3dm(buf, t.off); }
        catch (e) { console.log(`FAIL ${f}: ${e.message}`); ++fail; continue; }
        const pts = positions(r, buf);
        if (!pts) { console.log(`FAIL ${f}: 特征表缺 INSTANCES_LENGTH / POSITION`); ++fail; continue; }
        const nz = pts.filter(v => v.some(x => x !== 0)).length;
        const span = protoExtent(r, buf);
        const trs = nodesWithTrs(r.json);
        trsTotal += trs;
        const bad = nz === 0;
        if (bad) ++fail;
        rows.push({ f, n: pts.length, nz, trs, span, pts, bad });
    }
}

const box = (mn, mx) => `[${mn.map(v => v.toFixed(2))}]..[${mx.map(v => v.toFixed(2))}]`;

const tr = manifestTranslations(fs.statSync(target).isDirectory() ? target : path.dirname(target));
if (tr) {
    let matched = 0, total = 0, worst = 0;
    for (const w of rows) for (const v of w.pts) {
        ++total;
        if (!v.some(x => x !== 0)) continue;
        let best = 1e300;
        for (const e of tr) best = Math.min(best, Math.hypot(v[0] - e[0], v[1] - e[1], v[2] - e[2]));
        worst = Math.max(worst, best);
        if (best <= 1e-3 + 1e-4 * Math.max(...v.map(Math.abs))) ++matched;
    }
    const ok = total > 0 && matched === total;
    console.log(`manifest 实例平移 ${tr.length} | 特征表 POSITION ${total} 条对上 ${matched} (${total ? (100 * matched / total).toFixed(1) : '-'}%) 最近邻残差最大 ${worst.toExponential(2)} → ${ok ? 'PASS' : 'FAIL'}`);
    if (!ok) ++fail;
} else {
    console.log('（没有 feature-manifest.json，位姿只验"非全 0"，无法与真值对集合）');
}

console.log(`\ni3dm 瓦片 ${rows.length} 个：POSITION 全 0 = ${rows.filter(w => w.bad).length} 个；GLB 节点带 TRS 合计 ${trsTotal}`);
const spanMax = rows.reduce((a, w) => w.span ? Math.max(a, ...w.span.max.map((v, i) => Math.max(Math.abs(v), Math.abs(w.span.min[i])))) : a, 0);
console.log(`原型几何 |v| 最大 ${spanMax.toFixed(2)} m（A-41 污染时这里会出现 1e4 以上）`);
rows.sort((a, b) => (a.bad === b.bad ? a.f.localeCompare(b.f) : a.bad ? -1 : 1));
for (const w of rows.slice(0, top)) {
    const mn = [1e300, 1e300, 1e300], mx = [-1e300, -1e300, -1e300];
    for (const v of w.pts) for (let k = 0; k < 3; ++k) { mn[k] = Math.min(mn[k], v[k]); mx[k] = Math.max(mx[k], v[k]); }
    const s = w.span ? ` 原型span=${(w.span.max[0] - w.span.min[0]).toFixed(2)}x${(w.span.max[1] - w.span.min[1]).toFixed(2)}x${(w.span.max[2] - w.span.min[2]).toFixed(2)}` : '';
    console.log(`  ${w.bad ? 'FAIL' : 'ok  '} ${path.basename(w.f)} IL=${w.n} 非零=${w.nz} TRS=${w.trs} pos${box(mn, mx)}${s}`);
}
if (rows.length > top) console.log(`  ... 其余 ${rows.length - top} 个`);
console.log(fail ? `\nFAIL (${fail} 项)` : '\nPASS');
process.exit(fail ? 1 : 0);
