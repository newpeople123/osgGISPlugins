// 阳性对照 fixture：一个最小 b3dm，两个图元 ——
//   mesh0/primitive0：1 个面积 100 的三角形，3 个法线全零  -> 应判"真隐患" 3 条
//   primitive1：面积 10000 的正常三角形 + 面积 0 的退化三角形，零法线只落在退化面上
//               -> 应判"退化" 2 条
// 用法：node scripts/make_zero_norm_fixture.js <out.b3dm>
//       node scripts/zero_normal_triage.js <out.b3dm>   # 应报 零法线 5 / 真隐患 3 / 退化 2
const fs = require('fs');

function f32(arr) { const b = Buffer.alloc(arr.length * 4); arr.forEach((v, i) => b.writeFloatLE(v, i * 4)); return b; }
function u32(arr) { const b = Buffer.alloc(arr.length * 4); arr.forEach((v, i) => b.writeUInt32LE(v, i * 4)); return b; }

const posA = f32([0, 0, 0, 10, 0, 0, 0, 10, 0]);
const nrmA = f32([0, 0, 0, 0, 0, 0, 0, 0, 0]);
const idxA = u32([0, 1, 2]);

const posB = f32([0, 0, 0, 100, 0, 0, 0, 100, 0, 5, 5, 5, 5, 5, 5, 9, 9, 9]);
const nrmB = f32([1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]);
const idxB = u32([0, 1, 2, 3, 4, 5]);

const bin = Buffer.concat([posA, nrmA, posB, nrmB, idxA, idxB]);
const off = { posA: 0, nrmA: 36, posB: 72, nrmB: 144, idxA: 216, idxB: 228 };

const gltf = {
  asset: { version: '2.0' },
  scene: 0,
  scenes: [{ nodes: [0] }],
  nodes: [{ mesh: 0 }],
  meshes: [{
    name: 'fixture_mesh',
    primitives: [
      { attributes: { POSITION: 0, NORMAL: 1 }, indices: 2 },
      { attributes: { POSITION: 3, NORMAL: 4 }, indices: 5 },
    ],
  }],
  accessors: [
    { bufferView: 0, componentType: 5126, count: 3, type: 'VEC3', min: [0, 0, 0], max: [10, 10, 0] },
    { bufferView: 1, componentType: 5126, count: 3, type: 'VEC3', min: [0, 0, 0], max: [0, 0, 0] },
    { bufferView: 2, componentType: 5125, count: 3, type: 'SCALAR', min: [0], max: [2] },
    { bufferView: 3, componentType: 5126, count: 6, type: 'VEC3', min: [0, 0, 0], max: [100, 100, 9] },
    { bufferView: 4, componentType: 5126, count: 6, type: 'VEC3', min: [0, 0, 0], max: [1, 0, 1] },
    { bufferView: 5, componentType: 5125, count: 6, type: 'SCALAR', min: [0], max: [5] },
  ],
  bufferViews: [
    { buffer: 0, byteOffset: off.posA, byteLength: 36, target: 34962 },
    { buffer: 0, byteOffset: off.nrmA, byteLength: 36, target: 34962 },
    { buffer: 0, byteOffset: off.idxA, byteLength: 12, target: 34963 },
    { buffer: 0, byteOffset: off.posB, byteLength: 72, target: 34962 },
    { buffer: 0, byteOffset: off.nrmB, byteLength: 72, target: 34962 },
    { buffer: 0, byteOffset: off.idxB, byteLength: 24, target: 34963 },
  ],
  buffers: [{ byteLength: bin.length }],
};

let json = JSON.stringify(gltf);
while (json.length % 4) { json += ' '; }
const jsonBuf = Buffer.from(json, 'utf8');
const binBuf = Buffer.concat([bin, Buffer.alloc((4 - (bin.length % 4)) % 4)]);

const glb = Buffer.concat([
  Buffer.from('glTF', 'latin1'), (b => { b.writeUInt32LE(2, 0); return b; })(Buffer.alloc(4)),
  (b => { b.writeUInt32LE(12 + 8 + jsonBuf.length + 8 + binBuf.length, 0); return b; })(Buffer.alloc(4)),
  (b => { b.writeUInt32LE(jsonBuf.length, 0); b.writeUInt32LE(0x4E4F534A, 4); return b; })(Buffer.alloc(8)),
  jsonBuf,
  (b => { b.writeUInt32LE(binBuf.length, 0); b.writeUInt32LE(0x004E4942, 4); return b; })(Buffer.alloc(8)),
  binBuf,
]);

let ft = JSON.stringify({ BATCH_LENGTH: 0 });
while (ft.length % 8) { ft += ' '; }
const ftBuf = Buffer.from(ft, 'latin1');
const hdr = Buffer.alloc(28);
hdr.write('b3dm', 0, 'latin1');
hdr.writeUInt32LE(1, 4);
hdr.writeUInt32LE(28 + ftBuf.length + glb.length, 8);
hdr.writeUInt32LE(ftBuf.length, 12);
hdr.writeUInt32LE(0, 16);        // featureTableBinary
hdr.writeUInt32LE(0, 20);        // batchTableJSON
hdr.writeUInt32LE(0, 24);        // batchTableBinary

const out = process.argv[2];
fs.writeFileSync(out, Buffer.concat([hdr, ftBuf, glb]));
console.log('wrote ' + out + ' bytes=' + (28 + ftBuf.length + glb.length)
  + '  expected: 零法线 5 = 真隐患 3 (primitive0) + 退化 2 (primitive1)');
