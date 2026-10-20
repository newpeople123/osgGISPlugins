// A-42 判据的阳性对照生成器：把绝对路径注进真实产物 b3dm 的 glbJSON，
// 用来证明 path_leak_structural.js 的"解析 JSON 区再走字符串"不是恒返回 false。
// 同时产出一份只注相对 uri 的阴性对照——触发的必须是盘符，不是"多了一个键"。
// 用法: node scripts/make_path_leak_fixture.js <源.b3dm> <输出前缀>
const fs = require('fs');

const [src, outPrefix] = process.argv.slice(2);
if (!src || !outPrefix) { console.error('用法: node scripts/make_path_leak_fixture.js <源.b3dm> <输出前缀>'); process.exit(2); }

const buf = fs.readFileSync(src);
if (buf.toString('latin1', 0, 4) !== 'b3dm') { console.error('只支持 b3dm 源'); process.exit(2); }
const ftJ = buf.readUInt32LE(12), ftB = buf.readUInt32LE(16), btJ = buf.readUInt32LE(20), btB = buf.readUInt32LE(24);
const glbAt = 28 + ftJ + ftB + btJ + btB;
if (buf.toString('latin1', glbAt, glbAt + 4) !== 'glTF') { console.error('GLB 魔数不符，glbAt=' + glbAt); process.exit(2); }
const c0Len = buf.readUInt32LE(glbAt + 12), c0Type = buf.readUInt32LE(glbAt + 16);
if (c0Type !== 0x4E4F534A) { console.error('chunk0 不是 JSON'); process.exit(2); }
const jsonAt = glbAt + 20;
const rest = buf.subarray(jsonAt + c0Len);           // chunk1 及之后，原样搬

function build(extraKey) {
  const txt = buf.toString('utf8', jsonAt, jsonAt + c0Len).replace(/[\0 ]+$/, '');
  if (!txt.endsWith('}')) { console.error('glbJSON 不以 } 结尾'); process.exit(2); }
  let j = txt.slice(0, -1) + ',' + JSON.stringify(extraKey).slice(1, -1) + '}';
  const pad = (4 - (j.length % 4)) % 4;
  j += ' '.repeat(pad);
  const jsonBuf = Buffer.from(j, 'utf8');
  const glbTotal = 12 + 8 + jsonBuf.length + rest.length;
  const out = Buffer.alloc(glbAt + glbTotal);
  buf.copy(out, 0, 0, glbAt);                 // b3dm 头 + feature/batch table 原样保留
  out.writeUInt32LE(glbAt + glbTotal, 8);     // .b3dm byteLength
  out.write('glTF', glbAt, 'latin1');
  out.writeUInt32LE(2, glbAt + 4);
  out.writeUInt32LE(glbTotal, glbAt + 8);
  out.writeUInt32LE(jsonBuf.length, glbAt + 12);
  out.writeUInt32LE(c0Type, glbAt + 16);
  jsonBuf.copy(out, glbAt + 20);
  rest.copy(out, glbAt + 20 + jsonBuf.length);
  return out;
}

const cases = [
  ['leak', { __a42probe: 'F:/wb_3dtiles_out/CONTROL/ktx2-basecolor.ktx2' }, 1],
  ['clean', { __a42probe: 'textures/ktx2-basecolor.ktx2' }, 0],
];
for (const [name, key, want] of cases) {
  const f = outPrefix + '_' + name + '.b3dm';
  fs.writeFileSync(f, build(key));
  console.log(`${f}  ${fs.statSync(f).size} B  期望 structural 退出码 ${want}`);
}
