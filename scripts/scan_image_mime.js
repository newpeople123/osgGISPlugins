// 扫描数据集瓦片 GLB 里 images[].mimeType 的分布。
// 用法: node scripts/scan_image_mime.js <datasetDir> [maxTiles]
const fs = require('fs');
const path = require('path');

const dir = process.argv[2];
const maxTiles = Number(process.argv[3] || 1e9);
if (!dir) { console.error('usage: scan_image_mime.js <datasetDir> [maxTiles]'); process.exit(2); }

const tilesDir = fs.existsSync(path.join(dir, 'tiles')) ? path.join(dir, 'tiles') : dir;
const files = fs.readdirSync(tilesDir).filter((f) => /\.(b3dm|i3dm|cmpt)$/.test(f)).slice(0, maxTiles);

function glbJson(buf) {
  const magic = buf.toString('utf8', 0, 4);
  let off = 0;
  if (magic === 'b3dm') {
    off = 28 + buf.readUInt32LE(12) + buf.readUInt32LE(16) + buf.readUInt32LE(20) + buf.readUInt32LE(24);
  } else if (magic === 'i3dm') {
    off = 28 + buf.readUInt32LE(12) + buf.readUInt32LE(16) + buf.readUInt32LE(20) + buf.readUInt32LE(24) + 4;
  } else {
    return null;
  }
  const g = buf.subarray(off);
  if (g.toString('utf8', 0, 4) !== 'glTF') return null;
  return JSON.parse(g.subarray(20, 20 + g.readUInt32LE(12)).toString('utf8'));
}

const mime = new Map();
let filesWithImages = 0;
for (const f of files) {
  let j = null;
  try { j = glbJson(fs.readFileSync(path.join(tilesDir, f))); } catch (e) { continue; }
  if (!j || !j.images || !j.images.length) continue;
  filesWithImages++;
  for (const im of j.images) {
    const k = im.mimeType || '(none)';
    mime.set(k, (mime.get(k) || 0) + 1);
  }
}
console.log(dir + '  扫描瓦片=' + files.length + '  含 images 的瓦片=' + filesWithImages);
for (const [k, v] of [...mime.entries()].sort((a, b) => b[1] - a[1])) console.log('  ' + k + ' = ' + v);
