// 表格列数一致性检查：按分隔行（|---|---|）的列数比对表头与每个数据行。
// 只处理 GFM 管道表；转义成 \| 的管道不计入。用法：node scripts/table_pipe_check.js <文件...>
const fs = require('fs');
const BS = String.fromCharCode(92);
const pipes = (s) => {
  let n = 0;
  for (let i = 0; i < s.length; i++) if (s[i] === '|' && s[i - 1] !== BS) n++;
  return n;
};
let bad = 0;
for (const f of process.argv.slice(2)) {
  const L = fs.readFileSync(f, 'utf8').split(/\r?\n/);
  for (let i = 0; i < L.length; i++) {
    if (!/^\|[-: |]+\|$/.test(L[i])) continue;
    const cols = pipes(L[i]);
    const rows = [];
    if (i > 0 && L[i - 1].startsWith('|')) rows.push([i, L[i - 1]]);
    for (let j = i + 1; j < L.length && L[j].startsWith('|'); j++) rows.push([j + 1, L[j]]);
    for (const [ln, s] of rows) {
      const c = pipes(s);
      if (c !== cols) {
        console.log(`${f}:${ln} 列数=${c - 1}，分隔行 ${i + 1} 列数=${cols - 1}`);
        console.log(`    ${s.slice(0, 70)}…`);
        bad++;
      }
    }
  }
}
console.log(bad ? `违例 ${bad} 行` : '全部表格列数与分隔行一致');
process.exit(bad ? 1 : 0);
