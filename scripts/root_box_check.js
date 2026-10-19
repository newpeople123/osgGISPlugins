// Root bounding-volume sanity check: compare root box against the union of its
// DESCENDANTS only (excluding root itself, which would make containment trivial).
const fs = require("fs");
const dir = process.argv[2];
if (!dir) {
  console.error("usage: node scripts/root_box_check.js <datasetDir>");
  process.exit(2);
}
const ts = JSON.parse(fs.readFileSync(dir + "/tileset.json", "utf8"));

// Conservative OBB -> AABB: extent on axis i is sum of |column_i| of the 3 half-axes.
function aabb(b) {
  const c = [b[0], b[1], b[2]];
  const h = [[b[3], b[4], b[5]], [b[6], b[7], b[8]], [b[9], b[10], b[11]]];
  const ex = [0, 1, 2].map((i) =>
    Math.abs(h[0][i]) + Math.abs(h[1][i]) + Math.abs(h[2][i]));
  return { c, ex };
}

const root = ts.root;
const rA = aabb(root.boundingVolume.box);
const fmt = (a) => a.map((v) => v.toExponential(4)).join(", ");
console.log("root      center        =", fmt(rA.c));
console.log("root      abs half-ext  =", fmt(rA.ex));
console.log("root      aabb mn       =", fmt(rA.c.map((v, i) => v - rA.ex[i])));
console.log("root      aabb mx       =", fmt(rA.c.map((v, i) => v + rA.ex[i])));

const spans = [];
(function walk(n, d) {
  const bv = n.boundingVolume;
  if (bv && bv.box) {
    const a = aabb(bv.box);
    spans.push({ t: (n.content && n.content.uri) || "(group)", d,
      span: 2 * Math.max(...a.ex), c: a.c, ex: a.ex });
  }
  (n.children || []).forEach((c) => walk(c, d + 1));
})(root, 0);
// root itself is spans with d===0; union below is over DESCENDANTS ONLY
const rootSelf = spans.filter((s) => s.d === 0);
const desc = spans.filter((s) => s.d > 0);
const uMn = [0, 1, 2].map((i) => Math.min(...desc.map((s) => s.c[i] - s.ex[i])));
const uMx = [0, 1, 2].map((i) => Math.max(...desc.map((s) => s.c[i] + s.ex[i])));

console.log("nodes with box          =", spans.length, "(root:", rootSelf.length,
  "descendants:", desc.length, ")");
console.log("descendant union mn     =", fmt(uMn));
console.log("descendant union mx     =", fmt(uMx));

const SLACK = 1.0;
let outside = 0;
const firstOut = [];
for (const s of desc) {
  let bad = false;
  for (let i = 0; i < 3; i++) {
    if (s.c[i] - s.ex[i] < rA.c[i] - rA.ex[i] - SLACK ||
        s.c[i] + s.ex[i] > rA.c[i] + rA.ex[i] + SLACK) { bad = true; break; }
  }
  if (bad) { outside++; if (firstOut.length < 5) firstOut.push(s.t); }
}
console.log("descendants outside root=", outside, firstOut.join(" "));

const sorted = desc.map((s) => s.span).sort((a, b) => a - b);
const pct = (p) => sorted[Math.min(sorted.length - 1, Math.floor(p * sorted.length))];
console.log("desc span p50/p90/p99/max =",
  pct(0.5).toExponential(3), pct(0.9).toExponential(3),
  pct(0.99).toExponential(3), sorted[sorted.length - 1].toExponential(3));
desc.sort((a, b) => b.span - a.span);
console.log("largest descendants:");
for (const s of desc.slice(0, 4)) {
  console.log("  ", s.t, "d=" + s.d, "span=" + s.span.toFixed(2),
    "c=[" + s.c.map((v) => v.toFixed(1)).join(",") + "]",
    "ex=[" + s.ex.map((v) => v.toFixed(1)).join(",") + "]");
}
console.log("root max half-axis      =", Math.max(...rA.ex).toExponential(4),
  " ratio root/descMaxSpan =", (2 * Math.max(...rA.ex) / sorted[sorted.length - 1]).toFixed(3));
console.log("geometricError          =", ts.geometricError);
if (root.transform) console.log("root.transform translation =", root.transform.slice(12, 15).join(", "));
