// Coordinate-frame scan: are tile bounding volumes / content vertices in one
// consistent frame? Buckets every node box and every accessor extent by
// magnitude and by tile kind (b3dm vs i3dm), so a mixed-frame output is visible
// at a glance instead of inferred from two hand-picked tiles.
//   node scripts/coord_frame_scan.js <datasetDir> [...]
const fs = require("fs");
const path = require("path");

const B3DM_HDR = 28, I3DM_HDR = 32;

// i3dm: ftj@12 ftb@16 btj@20 btb@24, gltfFormat@28 -> GLB after 32+len
// b3dm: ftj@12 ftb@16 btj@20 btb@24              -> GLB after 28+len
function glbOf(buf, hdr) {
  const ftj = buf.readUInt32LE(12), ftb = buf.readUInt32LE(16);
  const btj = buf.readUInt32LE(20), btb = buf.readUInt32LE(24);
  const off = hdr + ftj + ftb + btj + btb;
  if (buf.toString("ascii", off, off + 4) !== "glTF") return null;
  const jsonLen = buf.readUInt32LE(off + 12);
  return JSON.parse(buf.toString("utf8", off + 20, off + 20 + jsonLen));
}

function accessorExtent(gj, binOff, buf, idx) {
  const a = gj.accessors[idx];
  if (!a || !a.min || a.type !== "VEC3") return null;
  return { min: a.min, max: a.max, count: a.count };
}

function bucket(v) {
  const m = Math.max(...v.map(Math.abs));
  if (!isFinite(m)) return "nonfinite";
  if (m < 1e3) return "<1e3";
  if (m < 1e6) return "<1e6";
  if (m < 1e7) return "<1e7";
  if (m < 1e8) return "<1e8";
  return ">=1e8";
}

for (const dir of process.argv.slice(2)) {
  const tsPath = path.join(dir, "tileset.json");
  if (!fs.existsSync(tsPath)) { console.log("skip (no tileset.json):", dir); continue; }
  const ts = JSON.parse(fs.readFileSync(tsPath, "utf8"));
  const boxes = [];
  (function walk(n) {
    const bv = n.boundingVolume || {};
    if (bv.box) boxes.push({ uri: (n.content && n.content.uri) || "(group)", b: bv.box });
    (n.children || []).forEach(walk);
  })(ts.root);

  const byKind = {};
  for (const e of boxes) {
    const kind = e.uri === "(group)" ? "group" : path.extname(e.uri).slice(1);
    const c = [e.b[0], e.b[1], e.b[2]];
    const h = [[e.b[3], e.b[4], e.b[5]], [e.b[6], e.b[7], e.b[8]], [e.b[9], e.b[10], e.b[11]]];
    const ex = [0, 1, 2].map((i) => Math.abs(h[0][i]) + Math.abs(h[1][i]) + Math.abs(h[2][i]));
    const mag = Math.max(...c.map(Math.abs)) + Math.max(...ex);
    byKind[kind] = byKind[kind] || {};
    byKind[kind][bucket([mag])] = (byKind[kind][bucket([mag])] || 0) + 1;
  }
  console.log("### " + dir);
  console.log("  node boxes by kind/magnitude(center+extent):");
  for (const k of Object.keys(byKind)) console.log("   ", k, JSON.stringify(byKind[k]));

  // content vertices, sampled from every tile file that exists
  const tilesDir = path.join(dir, "tiles");
  const vert = {};
  const worst = { b3dm: null, i3dm: null };
  let files = 0;
  if (fs.existsSync(tilesDir)) {
    for (const f of fs.readdirSync(tilesDir)) {
      const ext = path.extname(f).slice(1);
      if (ext !== "b3dm" && ext !== "i3dm") continue;
      files++;
      let gj = null;
      try { gj = glbOf(fs.readFileSync(path.join(tilesDir, f)), ext === "i3dm" ? I3DM_HDR : B3DM_HDR); }
      catch (e) { continue; }
      if (!gj || !gj.meshes) continue;
      for (const m of gj.meshes) {
        for (const p of m.primitives || []) {
          const e = accessorExtent(gj, 0, null, p.attributes && p.attributes.POSITION);
          if (!e) continue;
          const v = [Math.max(Math.abs(e.min[0]), Math.abs(e.max[0])),
                     Math.max(Math.abs(e.min[1]), Math.abs(e.max[1])),
                     Math.max(Math.abs(e.min[2]), Math.abs(e.max[2]))];
          vert[ext] = vert[ext] || {};
          const bk = bucket(v);
          vert[ext][bk] = (vert[ext][bk] || 0) + 1;
          const mg = Math.max(...v);
          if (!worst[ext] || mg > worst[ext].mg) worst[ext] = { f, mg, min: e.min, max: e.max };
        }
      }
    }
  }
  console.log("  POSITION extents across " + files + " tiles:");
  for (const k of Object.keys(vert)) {
    console.log("   ", k, JSON.stringify(vert[k]), "worst:", worst[k] ? worst[k].f + " |v|max=" + worst[k].mg.toExponential(3) : "-");
  }
}
