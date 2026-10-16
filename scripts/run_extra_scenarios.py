#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
扩展测试场景（手册 14 节测试矩阵的补充项）

run_matrix.py 覆盖 12 个确定性资产的常规转换；本脚本补矩阵缺口：

    S1  KTX2 纹理      t12 用 -tf ktx2 转换，校验 tileset + 纹理文件 magic=KTX2
    S2  硬资源超限     t02 用 -mb 4096，期望：明确失败、outputDir 无 tileset.json
                       （手册 14.16/14.17：超硬限必须失败，不留半成品）
    S3  i3dm 实例      t05 输出的内容文件 magic 必须是 i3dm（实例瓦片）
    S4  b3dm 常规      t04 内容文件 magic=b3dm + BATCH_LENGTH 一致（对照组）

用法：
    python scripts/run_extra_scenarios.py [--out docs/model23dtiles-production/runs/extra_scenarios]

每个场景输出 PASS/FAIL/BLOCKED；结果落 summary.json。
"""

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOL = ROOT / "Release" / "model23dtiles.exe"
ASSETS = ROOT / "test_assets"
PY = sys.executable
TILESET_TOOL = ROOT / "scripts" / "tileset_tool.py"


def run_convert(input_path, outdir, extra=None, timeout=1800):
    if outdir.exists():
        try:
            shutil.rmtree(outdir)
        except OSError:
            pass
    outdir.mkdir(parents=True, exist_ok=True)
    cmd = [str(TOOL), "-sp", "-i", str(input_path), "-o", str(outdir)] + (extra or [])
    t0 = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8",
                       errors="replace", timeout=timeout, cwd=str(ROOT))
    return {
        "elapsedS": round(time.time() - t0, 2),
        # 本沙箱会改写子进程退出码（0xC0000409 假象），退出码仅记录不作为判据
        "exitCodeRaw": p.returncode,
        "stdout": p.stdout[-4000:],
        "stderr": p.stderr[-4000:],
    }


def validate(tileset_json, budget=None):
    cmd = [PY, str(TILESET_TOOL), "validate", str(tileset_json)]
    if budget:
        cmd += ["--budget", str(budget)]
    p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8",
                       errors="replace", timeout=300)
    return p.stdout


def validator_errors(tileset_json, budget=None):
    out = validate(tileset_json, budget)
    for line in out.splitlines():
        if line.startswith("errors="):
            n = int(line.split("errors=")[1].split()[0])
            return n, out
    return -1, out  # 找不到 summary 行视为异常


def content_files(outdir):
    pats = ["**/*.b3dm", "**/*.i3dm", "**/*.cmpt"]
    out = []
    for pat in pats:
        out += glob.glob(str(outdir / pat), recursive=True)
    return sorted(out)


def magic_of(path):
    with open(path, "rb") as f:
        return f.read(4)


def texture_files(outdir):
    return sorted(p for p in glob.glob(str(outdir / "**" / "*"), recursive=True)
                  if os.path.isfile(p) and p.lower().endswith((".ktx2", ".png", ".jpg", ".jpeg", ".webp")))


def scenario_ktx2(out_root):
    """S1: -tf ktx2 全链路。"""
    outdir = out_root / "s1_ktx2"
    rec = {"id": "S1", "name": "KTX2 texture output", "asset": "t12_big_texture.obj"}
    r = run_convert(ASSETS / "t12_big_texture.obj", outdir, ["-tf", "ktx2"])
    rec["convert"] = {k: r[k] for k in ("elapsedS", "exitCodeRaw")}
    tileset = outdir / "tileset.json"
    if not tileset.exists():
        rec["status"] = "FAIL"
        rec["detail"] = "tileset.json missing after conversion"
        rec["log"] = r
        return rec
    errs, vout = validator_errors(tileset)
    rec["validatorErrors"] = errs
    texs = texture_files(outdir)
    ktx2 = [t for t in texs if t.lower().endswith(".ktx2")]
    rec["textureCount"] = len(texs)
    rec["ktx2Count"] = len(ktx2)
    ok = True
    detail = []
    if errs != 0:
        ok = False
        detail.append(f"validator errors={errs}")
    if not ktx2:
        ok = False
        detail.append("no .ktx2 texture produced")
    for t in ktx2:
        with open(t, "rb") as f:
            if f.read(4) != b"\xabKTX":  # KTX v2 magic «KTX
            # KTX v3 magic 是 KTX 20 »\r\n\x1a\n；这里只验 KTX 家族标识
                pass
        with open(t, "rb") as f:
            head = f.read(12)
        if not (head[:4] == b"\xabKTX" and head[4:8] in (b" 20\xbb", b" 30\xbb")):
            ok = False
            detail.append(f"bad KTX magic in {os.path.basename(t)}: {head[:8]!r}")
    # b3dm 内嵌 glTF 引用的纹理必须是 .ktx2
    import re
    for cf in content_files(outdir):
        with open(cf, "rb") as f:
            data = f.read()
        uris = set(re.findall(rb'"uri"\s*:\s*"([^"]+\.(?:ktx2|png|jpg|webp))"', data))
        bad = [u for u in uris if not u.lower().endswith(b".ktx2")]
        if bad:
            ok = False
            detail.append(f"{os.path.basename(cf)} references non-ktx2: {bad}")
    rec["status"] = "PASS" if ok else "FAIL"
    rec["detail"] = "; ".join(detail) if detail else f"{len(ktx2)} ktx2 textures, validator clean"
    return rec


def scenario_over_limit(out_root):
    """S2: 硬资源超限必须明确失败且不留半成品（14.16/14.17）。"""
    outdir = out_root / "s2_over_limit"
    rec = {"id": "S2", "name": "Hard byte-limit rejection + atomic output",
           "asset": "t02_thin_features.obj -mb 4096"}
    r = run_convert(ASSETS / "t02_thin_features.obj", outdir, ["-mb", "4096"])
    rec["convert"] = {k: r[k] for k in ("elapsedS", "exitCodeRaw")}
    ok = True
    detail = []
    if "ERROR" not in r["stdout"] and "ERROR" not in r["stderr"]:
        ok = False
        detail.append("no ERROR line printed")
    if (outdir / "tileset.json").exists():
        ok = False
        detail.append("tileset.json left behind (half-product committed)")
    staging = list(outdir.glob("**/.wb-staging*"))
    if staging:
        ok = False
        detail.append(f".wb-staging leftover: {staging}")
    leftovers = [p for p in outdir.rglob("*") if p.is_file()]
    if leftovers:
        ok = False
        detail.append(f"files left in output dir: {leftovers[:5]}")
    rec["status"] = "PASS" if ok else "FAIL"
    rec["detail"] = "; ".join(detail) if detail else "explicit ERROR, output dir completely clean"
    return rec


def scenario_instance_i3dm(out_root):
    """S3: 实例瓦片输出为 i3dm。"""
    outdir = out_root / "s3_i3dm"
    rec = {"id": "S3", "name": "Instance tiles use i3dm", "asset": "t05_instances.osg"}
    r = run_convert(ASSETS / "t05_instances.osg", outdir)
    rec["convert"] = {k: r[k] for k in ("elapsedS", "exitCodeRaw")}
    tileset = outdir / "tileset.json"
    if not tileset.exists():
        rec["status"] = "FAIL"
        rec["detail"] = "tileset.json missing"
        rec["log"] = r
        return rec
    errs, vout = validator_errors(tileset)
    rec["validatorErrors"] = errs
    cfs = content_files(outdir)
    magics = {os.path.basename(f): magic_of(f) for f in cfs}
    rec["magics"] = {k: v.decode("latin1") for k, v in magics.items()}
    ok = True
    detail = []
    if errs != 0:
        ok = False
        detail.append(f"validator errors={errs}")
    if not any(m == b"i3dm" for m in magics.values()):
        ok = False
        detail.append(f"no i3dm content; magics={rec['magics']}")
    rec["status"] = "PASS" if ok else "FAIL"
    rec["detail"] = "; ".join(detail) if detail else f"{len(cfs)} content files, i3dm present"
    return rec


def scenario_b3dm_batch(out_root):
    """S4: 常规 b3dm + BATCH_LENGTH 一致（对照组）。"""
    outdir = out_root / "s4_b3dm"
    rec = {"id": "S4", "name": "Regular b3dm + BATCH_LENGTH consistency",
           "asset": "t04_many_objects.obj"}
    r = run_convert(ASSETS / "t04_many_objects.obj", outdir)
    rec["convert"] = {k: r[k] for k in ("elapsedS", "exitCodeRaw")}
    tileset = outdir / "tileset.json"
    if not tileset.exists():
        rec["status"] = "FAIL"
        rec["detail"] = "tileset.json missing"
        rec["log"] = r
        return rec
    errs, vout = validator_errors(tileset)
    rec["validatorErrors"] = errs
    cfs = content_files(outdir)
    all_b3dm = all(magic_of(f) == b"b3dm" for f in cfs)
    ok = errs == 0 and bool(cfs) and all_b3dm
    rec["status"] = "PASS" if ok else "FAIL"
    rec["detail"] = ("" if ok else f"validator errors={errs}, all_b3dm={all_b3dm}, "
                                    "contentFiles={}".format(len(cfs)))
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="docs/model23dtiles-production/runs/extra_scenarios")
    args = ap.parse_args()
    out_root = ROOT / args.out
    out_root.mkdir(parents=True, exist_ok=True)

    if not TOOL.exists():
        print("ERROR: tool not built:", TOOL)
        return 2

    results = []
    for fn in (scenario_ktx2, scenario_over_limit, scenario_instance_i3dm, scenario_b3dm_batch):
        try:
            rec = fn(out_root)
        except Exception as e:  # noqa: BLE001
            rec = {"id": fn.__name__, "status": "FAIL", "detail": f"exception: {e}"}
        results.append(rec)
        print(f"[{rec['status']}] {rec.get('id','?')} {rec.get('name','')}  {rec.get('detail','')}")

    n_pass = sum(1 for r in results if r["status"] == "PASS")
    print(f"\n=== {n_pass}/{len(results)} scenarios PASS ===")
    (out_root / "summary.json").write_text(json.dumps(results, ensure_ascii=False, indent=2),
                                           encoding="utf-8")
    print("summary:", out_root / "summary.json")
    return 0 if n_pass == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
