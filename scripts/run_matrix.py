#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试矩阵运行器：对 test_assets 下每个资产跑一遍转换并采集指标。

用法:
    python scripts/run_matrix.py [--tool Release/model23dtiles.exe]
                                 [--out docs/model23dtiles-production/runs/<tag>]
                                 [--assets t01,t03]
                                 [--extra "-t quad -tri 80000"]
                                 [--budget 8388608]

采集: 墙钟时间、峰值内存(Windows PSAPI 采样)、退出码、输入/输出字节、
      tile 数、最大深度、误差范围、validator 结果。
输出: <out>/matrix.json 与 <out>/matrix.md
"""

import argparse
import ctypes
import json
import os
import subprocess
import sys
import time
from ctypes import wintypes

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tileset_tool  # noqa: E402


class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("PageFaultCount", wintypes.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
    ]


_psapi = ctypes.WinDLL("psapi")
_kernel32 = ctypes.WinDLL("kernel32")


def peak_rss_bytes(pid):
    h = _kernel32.OpenProcess(0x1000 | 0x0400, False, pid)  # PROCESS_QUERY_INFORMATION|VM_READ
    if not h:
        return 0
    try:
        pmc = PROCESS_MEMORY_COUNTERS()
        pmc.cb = ctypes.sizeof(pmc)
        if _psapi.GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb):
            return pmc.PeakWorkingSetSize
        return 0
    finally:
        _kernel32.CloseHandle(h)


def dir_bytes(path):
    total = 0
    count = 0
    for dirpath, _, files in os.walk(path):
        for fn in files:
            fp = os.path.join(dirpath, fn)
            try:
                total += os.path.getsize(fp)
                count += 1
            except OSError:
                pass
    return total, count


def run_one(tool, asset, outdir, extra, budget, timeout=1800):
    os.makedirs(outdir, exist_ok=True)
    cmd = [tool, "-sp", "-i", asset, "-o", outdir] + extra.split()
    t0 = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            universal_newlines=True, errors="replace")
    peak = 0
    while proc.poll() is None:
        peak = max(peak, peak_rss_bytes(proc.pid))
        time.sleep(0.05)
    try:
        out, _ = proc.communicate(timeout=30)
    except Exception:
        out = ""
    dt = time.time() - t0
    peak = max(peak, peak_rss_bytes(proc.pid))

    rec = {
        "asset": os.path.basename(asset),
        "cmd": " ".join(cmd),
        "exitCode": proc.returncode,
        "seconds": round(dt, 3),
        "peakRSSMiB": round(peak / (1024.0 * 1024.0), 1),
        "inputBytes": os.path.getsize(asset),
        "stdout": out[-2000:],
    }
    ts = os.path.join(outdir, "tileset.json")
    if os.path.isfile(ts):
        issues, stats = tileset_tool.validate_tileset(ts, budget)
        rec["outBytes"], rec["outFileCount"] = dir_bytes(outdir)
        rec["tileset"] = stats
        rec["errors"] = sum(1 for i in issues if i.severity == "ERROR")
        rec["warnings"] = sum(1 for i in issues if i.severity == "WARN")
        rec["issueKinds"] = {}
        for i in issues:
            rec["issueKinds"][i.kind] = rec["issueKinds"].get(i.kind, 0) + 1
        rec["sampleErrors"] = [str(i) for i in issues if i.severity == "ERROR"][:6]
    else:
        rec["outBytes"] = 0
        rec["outFileCount"] = 0
        rec["errors"] = None
        rec["warning"] = "no tileset.json produced"
    return rec


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser()
    ap.add_argument("--tool", default=os.path.join(root, "Release", "model23dtiles.exe"))
    ap.add_argument("--assets", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--extra", default="")
    ap.add_argument("--budget", type=int, default=8 * 1024 * 1024)
    args = ap.parse_args()

    asset_dir = os.path.join(root, "test_assets")
    names = sorted(os.listdir(asset_dir))
    if args.assets:
        keep = set(args.assets.split(","))
        names = [n for n in names if any(n.startswith(k) for k in keep)]
    outdir = args.out or os.path.join(root, "docs", "model23dtiles-production",
                                      "runs", time.strftime("%Y%m%d_%H%M%S"))
    outdir = os.path.abspath(outdir)
    os.makedirs(outdir, exist_ok=True)

    # model23dtiles 会把 CWD 切到输入文件目录，因此 -i/-o 都必须给绝对路径
    records = []
    for n in names:
        asset = os.path.join(asset_dir, n)
        if not os.path.isfile(asset):
            continue
        if os.path.splitext(n)[1].lower() not in (".obj", ".osgt", ".osg", ".fbx", ".gltf", ".glb"):
            continue
        print("=== %s ===" % n)
        rec = run_one(args.tool, asset, os.path.join(outdir, os.path.splitext(n)[0]),
                      args.extra, args.budget)
        records.append(rec)
        print(json.dumps({k: v for k, v in rec.items() if k != "stdout"},
                         ensure_ascii=False, indent=2))
        sys.stdout.flush()

    with open(os.path.join(outdir, "matrix.json"), "w", encoding="utf-8") as f:
        json.dump(records, f, indent=2, ensure_ascii=False)

    lines = ["| asset | exit | s | peakMiB | tiles | depth | contentFiles | outBytes | errors | kinds |",
             "|---|---|---|---|---|---|---|---|---|---|"]
    for r in records:
        st = r.get("tileset") or {}
        lines.append("| %s | %s | %.2f | %.1f | %s | %s | %s | %d | %s | %s |" % (
            r["asset"], r["exitCode"], r["seconds"], r["peakRSSMiB"],
            st.get("tiles", "-"), st.get("maxDepth", "-"),
            st.get("contentFiles", "-"), r["outBytes"],
            r.get("errors", "-"), ",".join(sorted(r.get("issueKinds", {}).keys())) or "-"))
    with open(os.path.join(outdir, "matrix.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    print("\noutput: %s" % outdir)


if __name__ == "__main__":
    main()
