// 把若干 b3dm / i3dm 打包成一个 cmpt 复合容器（3D Tiles 1.0 "Composite tile"）。
//
// 生产管线本身不产 cmpt：批量几何与实例分别成树、挂在同一根下作为兄弟子树
// （ProductionPipeline.cpp 的 buildB3dmTree / buildInstanceTree），OGC 校验 0 错误
// 证明该结构合法。本脚本只为给"目标客户端能正确加载 cmpt"这一验收项造一个
// 真实输入，再用官方 validator + CesiumJS 1.119 截图复核。
//
// 用法: node scripts/make_cmpt.js <out.cmpt> <in1.b3dm|in1.i3dm> [in2 ...]
"use strict";
const fs = require("fs");

const [, , out, ...ins] = process.argv;
if (!out || ins.length === 0) {
  console.error("usage: node scripts/make_cmpt.js <out.cmpt> <tile> [tile ...]");
  process.exit(2);
}

const parts = ins.map((p) => {
  const buf = fs.readFileSync(p);
  const magic = buf.toString("latin1", 0, 4);
  if (magic !== "b3dm" && magic !== "i3dm" && magic !== "pntx" && magic !== "cmpt") {
    throw new Error(`${p}: not a 3D Tiles inner tile (magic="${magic}")`);
  }
  // 校验器按内层瓦片自报的 byteLength 走位，这里提前核对，避免产出"能打开但错位"的容器
  const selfLength = buf.readUInt32LE(8);
  if (selfLength !== buf.length) {
    throw new Error(`${p}: byteLength field ${selfLength} != file size ${buf.length}`);
  }
  return buf;
});

// 每个内层瓦片就是它自己那份完整的 b3dm/i3dm 文件：magic(4) + version(4) +
// tileLength(4，即该瓦片自身 byteLength) + 体，起始偏移必须 8 字节对齐。
// 判据来自官方校验器 CmptValidator：magic 读 +0、长度读 +8，再把这一段整份
// 交给 B3dm/I3dmValidator——所以不能额外加 4 字节前缀（加了会报
// "Invalid inner tile magic" + "must be aligned to an 8-byte boundary"）。
const framed = parts.map((buf) => {
  const len = buf.length;
  if (len % 8 !== 0) {
    const padded = Buffer.alloc(Math.ceil(len / 8) * 8);
    buf.copy(padded);
    return padded;
  }
  return buf;
});

const byteLength = 16 + framed.reduce((s, f) => s + f.length, 0);
const header = Buffer.alloc(16);
header.write("cmpt", 0, "latin1");
header.writeUInt32LE(1, 4);
header.writeUInt32LE(byteLength, 8);
header.writeUInt32LE(framed.length, 12);

fs.writeFileSync(out, Buffer.concat([header, ...framed]));
console.log(`${out}: ${framed.length} inner tiles, ${byteLength} bytes`);
