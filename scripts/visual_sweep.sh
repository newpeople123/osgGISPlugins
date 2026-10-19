#!/usr/bin/env bash
# 固定相机路径批量截图，一次跑完全部数据集（手册 15 / 17 节）。
# 逐资产调用 scripts/visual_capture.sh，产物写
# docs/visual-acceptance/runs/fa_<name>/，最后打印 scripts/va_summary.js 汇总表。
#
# 用法: bash scripts/visual_sweep.sh [maxFramesPerPath] [name ...]
#   maxFramesPerPath  每条相机路径抽几帧，0 = 全帧（默认 0）
#   name ...          只跑指定数据集（默认 = 12 资产矩阵 + 真实 FBX + cmpt 探针）
#
# 环境变量：
#   WB_OUT_ROOT    F 盘产物根，默认 /f/wb_3dtiles_out（8081 服务的目录）
#   WB_MATRIX_URL  矩阵产物在 8081 上的路径前缀，
#                  默认 http://127.0.0.1:8081/matrix_texless
#   WB_PAGE_PORT   验收页端口，默认 8080（仓库根）
#   WB_OUT_PORT    产物端口，默认 8081
#
# 两个静态服务缺哪个脚本自己起哪个。页面里的 Cesium 走 /vendor/Cesium（1.119），
# 安装步骤见 docs/visual-acceptance/README.md。
#
# 不要在和 335 MB 级 FBX 转换并行的机器上跑：每帧从 ~6 s 涨到 ~60 s，
# 更要紧的是瓦片请求会在 settle 的 12 轮上限内回不来，被记成 HOLE ——
# 那是测量污染，不是产品缺陷。
set -u

MAXF="${1:-0}"
if [ $# -gt 0 ]; then shift; fi

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_ROOT="${WB_OUT_ROOT:-/f/wb_3dtiles_out}"
MATRIX_URL="${WB_MATRIX_URL:-http://127.0.0.1:8081/matrix_texless}"
PAGE_PORT="${WB_PAGE_PORT:-8080}"
OUT_PORT="${WB_OUT_PORT:-8081}"

MATRIX_ASSETS="t01_plane_curved t02_thin_features t03_giant_single_mesh \
t04_many_objects t05_instances t06_nonuniform t07_zero_axis t08_big_coords \
t09_cross_tile_object t10_material_heavy t11_sparse_outlier t12_big_texture"
EXTRA_ASSETS="longxiang_covfix cmpt_probe"

reachable() { curl -s -o /dev/null --max-time 2 "$1"; }

start_server() {  # $1=port $2=dir
  if ! reachable "http://127.0.0.1:$1/"; then
    echo "starting static server :$1 -> $2"
    node "$REPO/scripts/serve_static.js" "$1" "$2" >/tmp/va_serve_$1.log 2>&1 &
    sleep 1
  fi
}

start_server "$PAGE_PORT" "$REPO"
start_server "$OUT_PORT" "$OUT_ROOT"

is_matrix() { for a in $MATRIX_ASSETS; do [ "$a" = "$1" ] && return 0; done; return 1; }

NAMES=("$@")
if [ ${#NAMES[@]} -eq 0 ]; then
  NAMES=($MATRIX_ASSETS $EXTRA_ASSETS)
fi

fail=0
for n in "${NAMES[@]}"; do
  if is_matrix "$n"; then
    URL="$MATRIX_URL/$n/tileset.json"
  else
    URL="http://127.0.0.1:$OUT_PORT/$n/tileset.json"
  fi
  if ! reachable "$URL"; then
    echo "### $n SKIP: tileset not reachable at $URL" >&2
    fail=1
    continue
  fi
  echo "### $n $(date +%H:%M:%S)  $URL"
  if ! bash "$REPO/scripts/visual_capture.sh" "fa_$n" "$URL" "" "$MAXF" "$PAGE_PORT"; then
    echo "### $n capture failed (rc=$?)" >&2
    fail=1
  fi
done

echo
node "$REPO/scripts/va_summary.js" fa_
exit $fail
