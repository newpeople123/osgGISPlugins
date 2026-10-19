#!/usr/bin/env bash
# 视觉验收批量截图（替代 scripts/visual_capture.py —— 本机 python 是 Store 存根，
# 见 STATUS.md B-5）。用无头 Edge 打开验收页，页面自己逐帧摆相机、截图、
# 算"几何占比"，再 POST 回 scripts/serve_static.js 的 /va-report 落盘。
#
# 用法: bash scripts/visual_capture.sh <session> <tileset-url> [paths] [maxFrames] [port]
#   session     产物目录名 docs/visual-acceptance/runs/<session>/
#   tileset-url 相对验收页的路径，或 http://127.0.0.1:PORT/... 绝对地址
#   paths       逗号分隔，默认全部 9 条
#   maxFrames   每条路径抽帧数（0=全帧）
#   port        静态服务器端口，默认 8080
set -u

SESSION="${1:?session required}"
TILESET="${2:?tileset url required}"
PATHS="${3:-}"
MAXFRAMES="${4:-0}"
PORT="${5:-8080}"

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EDGE="/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
[ -x "$EDGE" ] || EDGE="/c/Program Files/Microsoft/Edge/Application/msedge.exe"
[ -x "$EDGE" ] || { echo "msedge.exe not found"; exit 3; }

RUNS="$REPO/docs/visual-acceptance/runs/$SESSION"
mkdir -p "$RUNS"

started_server=0
server_pid=""
if ! curl -s -o /dev/null --max-time 2 "http://127.0.0.1:$PORT/docs/visual-acceptance/index.html"; then
  echo "starting static server on $PORT"
  node "$REPO/scripts/serve_static.js" "$PORT" "$REPO" >"$RUNS/server.log" 2>&1 &
  server_pid=$!
  started_server=1
  sleep 1
fi

URL="http://127.0.0.1:$PORT/docs/visual-acceptance/index.html?tileset=${TILESET}&cesiumBase=/vendor/Cesium&all=1&session=${SESSION}&maxFrames=${MAXFRAMES}"
[ -n "$PATHS" ] && URL="$URL&paths=$PATHS"

UD="$(mktemp -d)"
echo "URL: $URL"
# --virtual-time-budget 不是"最多跑多久"，而是"页面跑完后 Edge 还要把虚拟时间
# 烧完才退出"：页面自己 0.6 s 就截完 6 帧，budget 给 300000 时命令却卡了 3 分钟。
# 实测每帧只消耗 ~0.1 s 虚拟时间，15000 足够 70 帧 + 加载，多给纯属白等。
"$EDGE" --headless=new --disable-gpu --enable-unsafe-swiftshader --no-sandbox \
  --disable-extensions --disable-background-timer-throttling \
  --user-data-dir="$UD" --window-size=1280,720 \
  --virtual-time-budget="${VA_BUDGET:-15000}" "$URL" 2>&1 \
  | grep -v -E "vbs_encoder|QQBrowser|fallback_task_provider|Fontconfig" | tail -5
rm -rf "$UD"

N=$(ls -1 "$RUNS"/*.png 2>/dev/null | wc -l)
echo "shots=$N in $RUNS"
[ "$started_server" = "1" ] && kill "$server_pid" 2>/dev/null
exit 0
