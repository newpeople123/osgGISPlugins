// 直接读**源** FBX 二进制里的法线数组，回答"零长度法线是源里就有的，还是我们写出时
// 变成 0 的"。这个问题原本以为要加一个 WB_DUMP_SOURCE 的构建才能判，其实源文件本身就是
// 答案，不需要动产品代码。
//
// FBX 7.x 二进制属性布局（实测于本仓库样本）：
//   名字长度(1) + 名字 + type(1) + arrayLength(4) + encoding(4) + packedLength(4) + 数据
//   encoding==1 时数据是 zlib，必须先解压再按分量读；不解压直接数会得出完全错误的结论
//   （本轮第一次尝试就是这么"数出"过 24.7 万个零法线，实为压缩字节）。
// 法线以 ByPolygonVertex/Direct 存成三重向量（本样本全是 double；'f' 也支持）；
// 多边形拓扑在**同一 mesh 对象**的 PolygonVertexIndex 里（负数=本多边形最后一个角点，
// 取反得顶点号），属性在 Layers 之前写出，所以"最近的前一个 PVI"就是该 mesh 的。
//
// 三重自检（任何一条不过就说明布局理解错了，别信统计数）：
//   ① 结构：解压后长度必须精确等于 arrayLength×elemSize（本样本 2729/2729 全过）；
//   ② 统计：非零向量里"非单位"的条数——布局错位时读出的是随机字节，这个数会爆
//      （错位那一版实测 16 675 068 条里 16.4 M 非单位；布局对了是 0）；
//   ③ 配对：零法线数组的角点数必须等于其 PVI 元素数（按角点数=向量数的前提）。
// 还有一条归零自检（防"恒阳"）：拿一个非 FBX 文件跑（如 test_assets/t01_plane_curved.obj）
// 会得到"命中 0 / 角点 0 / 零 0 / 末角点 0"并打印"源里没有零法线"、rc=0；
// 真源 FBX 上是 rc=1 + 63 个零。**没有这一条，"某个干净源得 0"这个结果没有意义。**
//
// 同一个 mesh 的法线在 FBX 里可能有多份**逐字节相同**的拷贝（本样本实测如此），所以
// 除了原始计数还要给按内容摘要去重后的独立计数——否则"含零法线的数组 = 36"会被读成
// "源里有 36 个不同的 mesh 带零法线"，那是夸大。
//
// 用法: node scripts/fbx_source_normals_probe.js <模型.fbx> [--detail N]
//        --detail N  额外打印前 N 个含零法线数组的明细（默认 3）
// 退出码：源里存在零法线 = 1（这是"源侧缺陷"的信号，不是产品缺陷）。
const fs = require('fs');
const zlib = require('zlib');
const crypto = require('crypto');

const detailArg = process.argv.indexOf('--detail');
const DETAIL = detailArg > 0 ? Number(process.argv[detailArg + 1]) : 3;
const file = process.argv[2];
if (!file || !fs.existsSync(file)) {
  console.error('用法: ' + process.argv[1] + ' <模型.fbx> [--detail N]');
  process.exit(2);
}

const fd = fs.openSync(file, 'r');
const size = fs.fstatSync(fd).size;
const ELEM = { 0x64: 8, 0x66: 4, 0x69: 4, 0x6c: 8 };   // 'd' 'f' 'i' 'l'

// 一次流式扫描同时收集多个属性名的位置（前一字节是名字长度）
function findNames(specs) {
  const CH = 16 << 20, OV = 40;
  const buf = Buffer.alloc(CH + OV);
  let pos = 0, tail = Buffer.alloc(0);
  const out = specs.map(() => []);
  while (true) {
    if (tail.length) tail.copy(buf, 0);
    const n = fs.readSync(fd, buf, tail.length, CH, pos);
    if (n <= 0) break;
    const used = tail.length + n;
    const win = buf.subarray(0, Math.max(0, used - OV));
    for (let i = 0; i < win.length; i++) {
      specs.forEach(([len, str], k) => {
        if (i + 1 + len <= win.length && win[i] === len &&
            win.toString('latin1', i + 1, i + 1 + len) === str) out[k].push(pos - tail.length + i + 1);
      });
    }
    tail = Buffer.from(buf.subarray(Math.max(0, used - OV), used));
    pos += n;
    if (n < CH) break;
  }
  return out.map((a) => [...new Set(a)].sort((x, y) => x - y));
}

// 解析 off 处（名字已跳过）的属性：返回 {n, vs, tc, raw} 或 null
function readArray(off, nameLen) {
  const h = Buffer.alloc(13);
  if (fs.readSync(fd, h, 0, 13, off + nameLen) !== 13) return null;
  const vs = ELEM[h[0]];
  if (!vs) return null;
  const n = h.readUInt32LE(1), enc = h.readUInt32LE(5), packed = h.readUInt32LE(9);
  if (!Number.isInteger(n) || n <= 0 || packed <= 0 || off + nameLen + 13 + packed > size) return null;
  const data = Buffer.alloc(packed);
  if (fs.readSync(fd, data, 0, packed, off + nameLen + 13) !== packed) return null;
  let raw;
  try { raw = enc === 1 ? zlib.inflateSync(data) : data; } catch (e) { return null; }
  if (raw.length !== n * vs) return null;              // 自检 ①
  return { n, vs, tc: h[0], raw };
}

const [normalsOff, pviOff] = findNames([[7, 'Normals'], [18, 'PolygonVertexIndex']]);

let arrays = 0, bad = 0, corners = 0, zero = 0, nonUnit = 0;
let uniqArrays = 0, uniqCorners = 0, uniqZero = 0;
const digests = new Set(), zeroDigests = new Set();
const perArray = [];

for (const off of normalsOff) {
  const a = readArray(off, 7);
  if (!a) { bad++; continue; }
  if (a.n % 3 !== 0) { bad++; continue; }              // 法线必须是三元向量
  arrays++;
  const dg = crypto.createHash('sha1').update(a.raw).digest('hex');
  const isNew = !digests.has(dg);
  digests.add(dg);
  const nv = a.n / 3;
  corners += nv;
  if (isNew) { uniqArrays++; uniqCorners += nv; }
  const zs = [];
  const rd = (b) => (a.vs === 8 ? a.raw.readDoubleLE(b) : a.raw.readFloatLE(b));
  for (let k = 0; k < nv; k++) {
    const b0 = k * a.vs * 3;
    const x = rd(b0), y = rd(b0 + a.vs), z = rd(b0 + 2 * a.vs);
    const l2 = x * x + y * y + z * z;
    if (l2 === 0) { zero++; zs.push(k); if (isNew) uniqZero++; }
    else if (!(l2 > 0.9801 && l2 < 1.0201)) nonUnit++; // 自检 ②
  }
  if (!zs.length) continue;
  if (isNew) zeroDigests.add(dg);

  // 配对同一 mesh 的 PolygonVertexIndex：取最近的前一个，且元素数必须等于角点数（自检 ③）
  let p = 0;
  for (const o of pviOff) { if (o < off) p = o; else break; }
  const pv = p ? readArray(p, 18) : null;
  const idx = [];
  if (pv && pv.n === nv) {
    for (let i = 0; i < pv.n; i++)
      idx.push(pv.vs === 8 ? Number(pv.raw.readBigInt64LE(i * 8)) : pv.raw.readInt32LE(i * 4));
  }
  let verts = null, polys = null, whole = null, polyEnd = null;
  if (idx.length) {
    verts = new Set(zs.map((k) => (idx[k] >= 0 ? idx[k] : ~idx[k]))).size;
    let pid = 0; const cornerPoly = [];
    for (const v of idx) { cornerPoly.push(pid); if (v < 0) pid++; }
    const touched = new Set(zs.map((k) => cornerPoly[k]));
    polys = touched.size;
    // 真实多边形边界（负数下标）下的"整面全零"
    const byPoly = new Map();
    for (let k = 0; k < idx.length; k++) {
      const q = cornerPoly[k];
      if (!touched.has(q)) continue;
      if (!byPoly.has(q)) byPoly.set(q, []);
      byPoly.get(q).push(k);
    }
    const zset = new Set(zs);
    whole = [...byPoly.values()].filter((cs) => cs.every((k) => zset.has(k))).length;
    polyEnd = zs.filter((k) => idx[k] < 0).length;
  }
  perArray.push({ off, nv, zero: zs.length, verts, polys, whole, polyEnd, uniq: isNew,
                  first: zs.slice(0, 9), paired: !!idx.length });
}
fs.closeSync(fd);

const uniqArr = perArray.filter((r) => r.uniq);
const sum = (f) => uniqArr.reduce((a, r) => a + (f(r) || 0), 0);
const uPct = uniqCorners ? (100 * uniqZero / uniqCorners).toFixed(7) : '0';
console.log('源文件 ' + file + '  ' + size + ' B');
console.log('Normals 属性命中 ' + normalsOff.length + '，PolygonVertexIndex 命中 ' + pviOff.length
  + '；法线数组解析成功 ' + arrays + ' / 失败 ' + bad);
console.log('法线角点 ' + corners + '（去重 ' + uniqCorners + '）  零长度 ' + zero
  + '（去重 ' + uniqZero + '，占去重角点 ' + uPct + '%）  非单位（自检②）= ' + nonUnit);
console.log('含零法线的数组 ' + perArray.length + ' 个 / 按内容去重 ' + zeroDigests.size
  + ' 个；其中能配对到同 mesh PVI 的 ' + uniqArr.filter((r) => r.paired).length
  + '/' + uniqArr.length + ' 个（去重后）');
console.log('零角点涉及的多边形（按真实负下标边界，非"3 角点/面"假设）= ' + sum((r) => r.polys)
  + '，其中整面全零 = ' + sum((r) => r.whole)
  + '，未整面 = ' + (sum((r) => r.polys) - sum((r) => r.whole)));
console.log('零角点里落在"多边形末角点"上的 = ' + sum((r) => r.polyEnd)
  + '；零角点唯一顶点数合计 = ' + sum((r) => r.verts)
  + (zero === 0 ? '  -> 源里没有零法线（本文件 Normals 命中 ' + normalsOff.length + '）'
    : sum((r) => r.polys) === sum((r) => r.whole)
      ? '  -> 源里就是"整个多边形全零"，与产物里连续 3 顶点成组的签名同构'
      : '  -> 源里有散落的单个零角点，需要另判'));
uniqArr.sort((a, b) => b.zero - a.zero);
for (const r of uniqArr.slice(0, DETAIL)) {
  console.log('  @file ' + r.off + ' 角点=' + r.nv + ' 零=' + r.zero + ' 唯一顶点=' + r.verts
    + ' 涉及多边形=' + r.polys + ' 整面全零=' + r.whole + ' 前 9 个零下标=' + JSON.stringify(r.first));
}
process.exit(zero ? 1 : 0);
