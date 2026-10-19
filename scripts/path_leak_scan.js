// 扫描产物里是否泄漏本地绝对路径（A-42）。区分"会被分发出去的瓦片文件"和"旁证 json"。
// 只认 JSON 字符串字面量形式的盘符：裸的 X:/ 正则在二进制 float 数据里大量误报
// （t03 的 7 个瓦片就是这么被误判的），那种命中不能当证据。
// 用法: node scripts/path_leak_scan.js <数据集目录>...
const fs = require('fs');
const path = require('path');

const TILE_EXT = ['.b3dm', '.cmpt', '.i3dm', '.pnts'];
// "D:/…" 或 "E:\…"，且内容像路径（后面至少还有 4 个非引号字符）
const DRIVE = /"[A-Za-z]:[/\\][^"]{4,}"/g;

function scan(dir) {
    let tileTotal = 0, tileLeak = 0, jsonTotal = 0, jsonLeak = 0;
    const samples = [];
    (function walk(x) {
        for (const e of fs.readdirSync(x, { withFileTypes: true })) {
            const f = path.join(x, e.name);
            if (e.isDirectory()) { walk(f); continue; }
            const ext = path.extname(e.name).toLowerCase();
            const isTile = TILE_EXT.includes(ext);
            const isJson = ext === '.json';
            if (!isTile && !isJson) continue;
            const s = fs.readFileSync(f).toString('latin1');
            const m = s.match(DRIVE);
            if (isTile) { ++tileTotal; if (m) ++tileLeak; } else { ++jsonTotal; if (m) ++jsonLeak; }
            if (m && samples.length < 4) samples.push([f.slice(-58), m.length, m[0].slice(0, 96)]);
        }
    })(dir);
    return { tileTotal, tileLeak, jsonTotal, jsonLeak, samples };
}

let bad = 0;
for (const d of process.argv.slice(2)) {
    const r = scan(d);
    if (r.tileLeak || r.jsonLeak) ++bad;
    console.log(`${d}\n  瓦片(分发物) ${r.tileTotal} 个，内含绝对路径 ${r.tileLeak} 个 ${r.tileLeak ? '→ LEAK' : '→ clean'}` +
        ` | 旁证 json ${r.jsonTotal} 个，内含绝对路径 ${r.jsonLeak} 个`);
    for (const [f, n, c] of r.samples) console.log(`    ${f}  (${n} 处)\n       ${c}`);
}
process.exit(bad ? 1 : 0);
