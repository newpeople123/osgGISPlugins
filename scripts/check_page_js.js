// 语法体检：把验收页里所有内联 <script> 抽出来过一遍 new Function()。
// 页面是 HTML，改坏了不会有编译错误，只能这样抓语法问题。
// 用法: node scripts/check_page_js.js docs/visual-acceptance/index.html
"use strict";
const fs = require("fs");
const file = process.argv[2];
if (!file) { console.error("usage: node scripts/check_page_js.js <html>"); process.exit(2); }
const src = fs.readFileSync(file, "utf8");
const re = /<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/g;
let m, n = 0, bad = 0;
while ((m = re.exec(src))) {
  n++;
  try { new Function(m[1]); console.log(`block ${n}: OK (${m[1].length} chars)`); }
  catch (e) { bad++; console.log(`block ${n}: SYNTAX ERROR: ${e.message}`); }
}
console.log(`${file}: ${n} inline script block(s), ${bad} with syntax errors`);
process.exit(bad ? 1 : 0);
