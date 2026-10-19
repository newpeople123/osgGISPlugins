// i3dm 每实例位姿验收（A-41）。历史上 S3 只验过容器 magic，"位姿有没有落盘"从未验收。
// 逐 i3dm 瓦片核对三件事：
//   1) 特征表 POSITION 有没有非零值（A-41 签名：全 0）
//   2) GLB 节点是否还带 TRS（带 = flatten 没屏蔽干净，会与特征表双变换）
//   3) POSITION 集合能否对上 feature-manifest.json 的 **inverse(原型世界)·实例世界**
//      平移（glTF Y-up 轴序 (x,-z,y)）
// 第 3 条的口径本身踩过坑：原型几何已按世界矩阵烘好，特征表若写实例的**绝对**世界
// 平移就是双变换，而"逐条命中绝对平移"当时被判成 PASS。所以这里把绝对口径只作为
// 诊断信息打印，判定一律按相对帧。
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

// OSG 行向量约定、内存按行主序铺开：平移在 [12..14]，最后一列是 [3],[7],[11],[15]
function mat4Mul(a, b) {
    const o = new Array(16);
    for (let r = 0; r < 4; ++r)
        for (let c = 0; c < 4; ++c) {
            let s = 0;
            for (let k = 0; k < 4; ++k) s += a[r * 4 + k] * b[k * 4 + c];
            o[r * 4 + c] = s;
        }
    return o;
}

// 高斯-约当 + 部分主元；奇异矩阵返回 null（相对帧无法定义，必须显式失败）
function mat4Inverse(m) {
    const a = new Array(32).fill(0);
    for (let r = 0; r < 4; ++r) {
        for (let c = 0; c < 4; ++c) a[r * 8 + c] = m[r * 4 + c];
        a[r * 8 + 4 + r] = 1;
    }
    for (let col = 0; col < 4; ++col) {
        let piv = col;
        for (let r = col + 1; r < 4; ++r)
            if (Math.abs(a[r * 8 + col]) > Math.abs(a[piv * 8 + col])) piv = r;
        if (Math.abs(a[piv * 8 + col]) < 1e-14) return null;
        if (piv !== col)
            for (let c = 0; c < 8; ++c) {
                const t = a[col * 8 + c]; a[col * 8 + c] = a[piv * 8 + c]; a[piv * 8 + c] = t;
            }
        const d = a[col * 8 + col];
        for (let c = 0; c < 8; ++c) a[col * 8 + c] /= d;
        for (let r = 0; r < 4; ++r) {
            if (r === col) continue;
            const f = a[r * 8 + col];
            if (f === 0) continue;
            for (let c = 0; c < 8; ++c) a[r * 8 + c] -= f * a[col * 8 + c];
        }
    }
    const o = new Array(16);
    for (let r = 0; r < 4; ++r) for (let c = 0; c < 4; ++c) o[r * 4 + c] = a[r * 8 + 4 + c];
    return o;
}

// OSG → glTF Y-up：(x, y, z) -> (x, -z, y)
const perm = t => [t[12], -t[14], t[13]];

// 期望集合：rel = inverse(原型世界) · 实例世界（特征表里真正该写的矩阵）
// 同时保留绝对口径，只用作诊断——命中绝对平移正是双变换的特征。
function manifestExpectations(dir) {
    const f = path.join(dir, 'feature-manifest.json');
    if (!fs.existsSync(f)) return null;
    const m = JSON.parse(fs.readFileSync(f, 'utf8'));
    if (!Array.isArray(m.instances) || !m.instances.length) return null;
    const rel = [], abs = [];
    let missingProto = 0, singular = 0;
    for (const it of m.instances) {
        const inst = it.transform || it.matrix;
        if (!Array.isArray(inst) || inst.length !== 16) continue;
        abs.push(perm(inst));
        const proto = it.prototypeTransform;
        if (!Array.isArray(proto) || proto.length !== 16) { ++missingProto; continue; }
        const inv = mat4Inverse(proto);
        if (!inv) { ++singular; continue; }
        rel.push(perm(mat4Mul(inv, inst)));
    }
    return { rel, abs, n: m.instances.length, missingProto, singular,
             schema: m.schema };
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

const exp = manifestExpectations(fs.statSync(target).isDirectory() ? target : path.dirname(target));
if (exp) {
    const nn = (v, set) => {
        let best = 1e300;
        for (const e of set) best = Math.min(best, Math.hypot(v[0] - e[0], v[1] - e[1], v[2] - e[2]));
        return best;
    };
    const score = (set) => {
        let matched = 0, total = 0, worst = 0;
        for (const w of rows) for (const v of w.pts) {
            ++total;
            if (!v.some(x => x !== 0)) continue;
            const d = nn(v, set);
            worst = Math.max(worst, d);
            if (d <= 1e-3 + 1e-4 * Math.max(...v.map(Math.abs))) ++matched;
        }
        return { matched, total, worst };
    };
    const s = score(exp.rel);
    const a = score(exp.abs);
    console.log(`manifest ${exp.schema || '?'} | 实例 ${exp.n} 条：缺 prototypeTransform ${exp.missingProto}、奇异原型 ${exp.singular}`);
    console.log(`按相对帧 inverse(原型)·实例 比对：${s.total} 条 POSITION 对上 ${s.matched} (${s.total ? (100 * s.matched / s.total).toFixed(1) : '-'}%) 最近邻残差最大 ${s.worst.toExponential(2)} → ${s.total > 0 && s.matched === s.total ? 'PASS' : 'FAIL'}`);
    console.log(`（诊断）按绝对平移比对：${a.total} 条对上 ${a.matched} —— 旧版本误把这一条当验收，命中绝对平移恰恰是双变换）`);
    if (!(s.total > 0 && s.matched === s.total)) ++fail;
    if (exp.missingProto || exp.singular) {
        console.log('FAIL: manifest 缺少可判相对帧的原型矩阵（v1 产物 / 原型不可逆）——无法验收，必须用修正后的二进制重跑');
        ++fail;
    }
} else {
    console.log('（没有可用的 feature-manifest.json，位姿只验"非全 0"，无法与真值对集合）');
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
