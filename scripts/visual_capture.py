"""
视觉验收自动截图（手册第 15 节）
================================

用 Playwright 驱动 docs/visual-acceptance/index.html，对指定 tileset 跑完
9 条固定相机路径并逐帧截图，同时记录：

- 浏览器 UA / GPU 渲染器 / WebGL 版本（截图元数据必须带）
- 视口 1280x720、FOV 60deg、SSE 4 px（固定）
- 每帧的可见 tile URI、相机位置
- 页面 console 错误（HOLE / 内容丢失的直接信号）

用法：
    python scripts/visual_capture.py --tileset out_test/longxiang/tileset.json \
        --out docs/visual-acceptance/runs/longxiang [--paths orbit,lod-threshold] \
        [--serve-root .] [--headed]

说明：
- 需要一个本地静态服务器（脚本会自己起一个，http.server）。
- Playwright 的 Chromium 默认用 SwiftShader 软件渲染，无独显的机器也能跑，
  但 ENV.gpuRenderer 会显示 SwiftShader，报告里要如实写。
"""

import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import threading
import time
import http.server
import functools
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PAGE = REPO_ROOT / "docs" / "visual-acceptance" / "index.html"

ALL_PATHS = ["far-to-near", "near-to-far", "orbit", "oblique-high", "oblique-low",
             "seam-pan", "feature-approach", "lod-threshold", "texture-zoom"]


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def start_server(root: Path):
    port = free_port()
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(root))
    # 静音
    handler.log_message = lambda *a, **k: None
    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)
    t = threading.Thread(target=httpd.serve_forever, daemon=True)
    t.start()
    return httpd, port


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tileset", required=True, help="tileset.json 路径")
    ap.add_argument("--out", required=True, help="输出目录")
    ap.add_argument("--paths", default=",".join(ALL_PATHS))
    ap.add_argument("--serve-root", default=str(REPO_ROOT),
                    help="静态服务器根目录，必须能覆盖 tileset 与页面")
    ap.add_argument("--headed", action="store_true")
    ap.add_argument("--width", type=int, default=1280)
    ap.add_argument("--height", type=int, default=720)
    ap.add_argument("--settle-ms", type=int, default=250)
    args = ap.parse_args()

    tileset_abs = Path(args.tileset).resolve()
    serve_root = Path(args.serve_root).resolve()
    try:
        rel = tileset_abs.relative_to(serve_root).as_posix()
    except ValueError:
        print("ERROR: --serve-root 必须是 tileset 路径的父目录", file=sys.stderr)
        print("  tileset     =", tileset_abs, file=sys.stderr)
        print("  serve-root  =", serve_root, file=sys.stderr)
        return 2

    out_dir = Path(args.out)
    if out_dir.exists():
        shutil.rmtree(out_dir)
    (out_dir / "shots").mkdir(parents=True)

    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("ERROR: 需要 playwright。安装：", file=sys.stderr)
        print("  pip install playwright && playwright install chromium", file=sys.stderr)
        return 2

    httpd, port = start_server(serve_root)
    page_url = f"http://127.0.0.1:{port}/docs/visual-acceptance/index.html"
    tileset_url = f"http://127.0.0.1:{port}/{rel}"
    print(f"page     : {page_url}")
    print(f"tileset  : {tileset_url}")

    result = {
        "tileset": str(tileset_abs),
        "tilesetUrl": tileset_url,
        "pageUrl": page_url,
        "startedAt": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "paths": [],
        "consoleErrors": [],
        "pageErrors": [],
    }

    with sync_playwright() as pw:
        browser = pw.chromium.launch(headless=not args.headed, args=[
            # 无 GPU 机器上打开软件 WebGL；有 GPU 也保留，行为可复现
            "--use-gl=angle",
            "--use-angle=swiftshader",
            "--enable-unsafe-swiftshader",
            "--ignore-gpu-blocklist",
        ])
        ctx = browser.new_context(viewport={"width": args.width, "height": args.height},
                                  device_scale_factor=1)
        page = ctx.new_page()
        page.on("console", lambda m: result["consoleErrors"].append(
            {"type": m.type, "text": m.text[:2000]}) if m.type == "error" else None)
        page.on("pageerror", lambda e: result["pageErrors"].append(str(e)[:2000]))

        page.goto(page_url, wait_until="load", timeout=120000)
        page.wait_for_timeout(1500)
        env = page.evaluate("() => window.__VA__.info()")
        result["env"] = env
        print("env:", json.dumps(env, ensure_ascii=False))

        page.evaluate(f"() => window.__VA__.load({json.dumps(tileset_url)})")
        page.wait_for_function("() => window.__VA__.ready()", timeout=600000)
        page.wait_for_timeout(1000)

        tiles = page.evaluate("() => window.__VA__.tilePaths()")
        result["tilePaths"] = tiles
        print(f"tileset loaded: {len(tiles)} tiles")

        paths = [p.strip() for p in args.paths.split(",") if p.strip()]
        for path_name in paths:
            frames = page.evaluate(f"() => window.__VA__.pathFrames({json.dumps(path_name)})")
            if isinstance(frames, dict) or not frames:
                result["paths"].append({"path": path_name, "error": "no frames"})
                continue
            pdir = out_dir / "shots" / path_name
            pdir.mkdir(parents=True, exist_ok=True)
            frames_out = []
            print(f"\n=== {path_name}: {len(frames)} frames ===")
            for i, fname in enumerate(frames):
                page.evaluate(
                    f"() => window.__VA__.gotoFrame({json.dumps(path_name)}, {json.dumps(fname)})")
                page.wait_for_timeout(args.settle_ms)
                vis = page.evaluate("() => window.__VA__.visibleTiles()")
                cam = page.evaluate("() => window.__VA__.camera()")
                data_url = page.evaluate("() => window.__VA__.screenshot()")
                png = (data_url.split(",", 1)[1] if "," in data_url else data_url)
                import base64
                png_path = pdir / f"{i:03d}_{fname}.png"
                png_path.write_bytes(base64.b64decode(png))
                rec = {"frame": fname, "index": i, "camera": cam,
                       "visibleTiles": vis, "screenshot": str(png_path.relative_to(out_dir))}
                frames_out.append(rec)
                print(f"  [{i+1}/{len(frames)}] {fname}  vis={len(vis)}  "
                      f"cam=({cam['lon']:.4f},{cam['lat']:.4f},{cam['height']:.1f})")
            result["paths"].append({"path": path_name, "frames": frames_out})

        browser.close()
    httpd.shutdown()

    result["finishedAt"] = time.strftime("%Y-%m-%dT%H:%M:%S%z")
    (out_dir / "capture.json").write_text(json.dumps(result, ensure_ascii=False, indent=2),
                                          encoding="utf-8")

    n_frames = sum(len(p.get("frames", [])) for p in result["paths"])
    n_png = len(list((out_dir / "shots").rglob("*.png")))
    print(f"\n完成：{len(result['paths'])} 条路径 / {n_frames} 帧 / {n_png} 张截图")
    print(f"console errors: {len(result['consoleErrors'])}")
    print(f"page errors   : {len(result['pageErrors'])}")
    print(f"结果          : {out_dir / 'capture.json'}")
    if result["pageErrors"]:
        for e in result["pageErrors"][:10]:
            print("  PAGEERROR:", e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
