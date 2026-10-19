#!/usr/bin/env bash
# OGC 3D Tiles 1.0 官方校验（CesiumGS 3d-tiles-validator）批量跑。
#
# 本机 python 是 Store 存根（STATUS.md B-5），npm 侧此前误判为"未安装"：
# 实际 registry.npmjs.org 可达，只是 github.com 不可达，导致 sharp 0.32 的
# prebuild-install（从 GitHub  Releases 拉 libvips）超时。装法见本脚本第 1 步。
#
# 用法: bash scripts/run_validator.sh <tilesetsDir> [reportDir]
#   tilesetsDir  下面每个子目录里要有 tileset.json
#   reportDir    默认 docs/model23dtiles-production/validation
set -u

SRC="${1:?tilesets dir required}"
OUT="${2:-docs/model23dtiles-production/validation}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VDIR="$REPO/vendor/3dt-validator"
VMAIN="$VDIR/node_modules/3d-tiles-validator/build/main.js"

if [ ! -f "$VMAIN" ]; then
  echo "# 安装校验器（--ignore-scripts：github.com 不可达，sharp 走 npm 预编译包）"
  npm install --prefix "$VDIR" --no-audit --no-fund --ignore-scripts 3d-tiles-validator || exit 3
  npm install --prefix "$VDIR" --no-audit --no-fund --ignore-scripts sharp@0.33.5 || exit 3
  # 三个依赖各自内嵌 sharp 0.32（无原生二进制），统一换成 0.33.5
  for d in "$VDIR"/node_modules/*/node_modules/sharp "$VDIR"/node_modules/*/node_modules/*/node_modules/sharp; do
    [ -d "$d" ] && { rm -rf "$d"; cp -r "$VDIR/node_modules/sharp" "$d"; }
  done
fi
[ -f "$VMAIN" ] || { echo "validator not installed: $VMAIN"; exit 3; }

mkdir -p "$OUT"
# 两种形态都收：容器目录（下面每个子目录一个数据集）或数据集目录本身
if [ -f "$SRC/tileset.json" ]; then
  LIST="$SRC"
else
  LIST="$(printf '%s\n' "$SRC"/*/)"
fi
printf '%-28s %8s %8s %8s  %s\n' tileset errors warnings infos report
fail=0
for d in $LIST; do
  ts="${d%/}/tileset.json"
  [ -f "$ts" ] || continue
  name=$(basename "${d%/}")
  rep="$OUT/$name.validator.json"
  node "$VMAIN" -t "$(cygpath -w "$ts" 2>/dev/null || echo "$ts")" \
       -r "$(cygpath -w "$rep" 2>/dev/null || echo "$rep")" >"$OUT/$name.validator.log" 2>&1
  read -r e w i <<<"$(node -e '
    var r = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
    console.log(r.numErrors, r.numWarnings, r.numInfos);
  ' "$rep" 2>/dev/null || echo "解析失败 - -")"
  printf '%-28s %8s %8s %8s  %s\n' "$name" "$e" "$w" "$i" "$rep"
  [ "$e" = "0" ] || fail=1
done
exit $fail
