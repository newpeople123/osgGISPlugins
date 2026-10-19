// Read the glTF payload out of a .b3dm and report accessor min/max, so a
// tileset bounding box can be checked against the geometry that was actually
// written. Usage: node scripts/b3dm_extents.js <file.b3dm|file.i3dm> [...]
const fs = require('fs');

function glbAt(buf, tileOff) {
  // 外壳头：b3dm 28B，i3dm 32B（多一个 glTFFormat）；GLB 紧跟四张表之后。
  const magic = buf.toString('latin1', tileOff, tileOff + 4);
  const hdr = magic === 'i3dm' ? 32 : 28;
  const ftj = buf.readUInt32LE(tileOff + 12);
  const ftb = buf.readUInt32LE(tileOff + 16);
  const btj = buf.readUInt32LE(tileOff + 20);
  const btb = buf.readUInt32LE(tileOff + 24);
  const glbOff = tileOff + hdr + ftj + ftb + btj + btb;
  if (buf.toString('latin1', glbOff, glbOff + 4) !== 'glTF') {
    throw new Error('no GLB at ' + glbOff + ' (found ' + buf.toString('latin1', glbOff, glbOff + 4) + ')');
  }
  const chunkLen = buf.readUInt32LE(glbOff + 12);
  return JSON.parse(buf.toString('utf8', glbOff + 20, glbOff + 20 + chunkLen));
}

function tilesIn(file) {
  const buf = fs.readFileSync(file);
  const magic = buf.toString('latin1', 0, 4);
  if (magic === 'CMPT') {
    // cmpt: 16B 头 + 若干独立瓦片（magic 在 +0，自己的 byteLength 在 +8，8B 对齐）
    const out = [];
    let off = 16;
    const total = buf.readUInt32LE(8);
    while (off + 12 <= Math.min(total, buf.length)) {
      const len = buf.readUInt32LE(off + 8);
      out.push({ buf, tileOff: off, label: buf.toString('latin1', off, off + 4) });
      off += len + (len % 8 ? 8 - (len % 8) : 0);
    }
    return out;
  }
  return [{ buf, tileOff: 0, label: magic }];
}

for (const f of process.argv.slice(2)) {
  let parts;
  try { parts = tilesIn(f); } catch (e) { console.log(f + ' READ-FAIL ' + e.message); continue; }
  for (const p of parts) {
    let g;
    try { g = glbAt(p.buf, p.tileOff); } catch (e) { console.log(`${f} [${p.label}] PARSE-FAIL ${e.message}`); continue; }
    const mins = [Infinity, Infinity, Infinity], maxs = [-Infinity, -Infinity, -Infinity];
    let accs = 0;
    for (const a of g.accessors || []) {
      if (a.type !== 'VEC3' || !a.min || !a.max) continue;
      ++accs;
      for (let i = 0; i < 3; ++i) {
        mins[i] = Math.min(mins[i], a.min[i]);
        maxs[i] = Math.max(maxs[i], a.max[i]);
      }
    }
    const fmt = (v) => (Number.isFinite(v) ? v.toExponential(3) : 'n/a');
    const absMax = Math.max(...mins.map(Math.abs), ...maxs.map(Math.abs));
    console.log(`${f} [${p.label}] posAccessors=${accs} meshes=${(g.meshes || []).length} maxAbsCoord=${fmt(absMax)}`);
    console.log(`    min=[${mins.map(fmt).join(', ')}] max=[${maxs.map(fmt).join(', ')}] bytes=${p.buf.length}`);
  }
}
