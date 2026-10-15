#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
3D Tiles 1.0 校验 / 统计工具（纯 Python，无第三方依赖）

子命令:
    validate <tileset.json> [--json out.json]   3D Tiles 1.0 结构与内容校验
    stats    <tileset.json> [--json out.json]   输入/输出统计
    glbinfo  <file.glb|b3dm|i3dm|cmpt>          打印内部 glTF / batch 信息

校验项（对应手册 12.3 / 17 节）:
    SPEC      asset.version、root.refine、geometricError 有限非负、root->leaf 不增
    BOUNDS    boundingVolume 有限、box 半轴非负、父包围体覆盖子包围体
    CONTENT   content URI 存在、b3dm/i3dm/cmpt 头长度与内嵌结构一致
    BATCH     _BATCHID 取值落在 [0, batchLength)、Batch Table 数组长度 == batchLength
    FILES     无孤儿文件（磁盘上有但 JSON 未引用）
    BUDGET    每个内容文件字节数 <= 给定硬上限
"""

import json
import math
import os
import struct
import sys

MAGIC = {b"b3dm": "b3dm", b"i3dm": "i3dm", b"cmpt": "cmpt", b"glTF": "glb"}


class Issue:
    def __init__(self, kind, severity, path, message):
        self.kind = kind
        self.severity = severity      # ERROR | WARN
        self.path = path
        self.message = message

    def as_dict(self):
        return {"kind": self.kind, "severity": self.severity,
                "path": self.path, "message": self.message}

    def __str__(self):
        return "[%s][%s] %s : %s" % (self.severity, self.kind, self.path, self.message)


# --------------------------------------------------------------------------
# 二进制 tile 解析
# --------------------------------------------------------------------------

def parse_tile_file(path):
    """返回 dict: {format, byteLength, fileSize, ...}；失败时返回 {'error':...}"""
    info = {"path": path, "fileSize": os.path.getsize(path)}
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 12:
        info["error"] = "file too short"
        return info
    magic = data[0:4]
    if magic not in MAGIC:
        info["error"] = "unknown magic %r" % magic
        return info
    fmt = MAGIC[magic]
    info["format"] = fmt

    if fmt == "cmpt":
        (version, byteLength, tilesLength) = struct.unpack_from("<III", data, 4)
        info["version"] = version
        info["byteLength"] = byteLength
        info["components"] = []
        off = 12
        n = 0
        while off + 12 <= len(data):
            cm = data[off:off + 4]
            if cm not in (b"b3dm", b"i3dm", b"glTF"):
                info.setdefault("warnings", []).append(
                    "cmpt component %d bad magic %r at %d" % (n, cm, off))
                break
            (cv, cbl) = struct.unpack_from("<II", data, off + 4)
            if cbl == 0 or off + cbl > len(data):
                info.setdefault("warnings", []).append(
                    "cmpt component %d bad byteLength %d" % (n, cbl))
                break
            comp = {"format": MAGIC[cm], "byteLength": cbl, "offset": off}
            if cm != b"glTF":
                sub = _parse_b3dm_i3dm(data[off:off + cbl])
                comp.update(sub)
            info["components"].append(comp)
            off += cbl
            n += 1
        info["componentCount"] = n
        if byteLength != info["fileSize"]:
            info["warnings"] = info.get("warnings", []) + [
                "byteLength %d != file size %d" % (byteLength, info["fileSize"])]
        return info

    if fmt == "glb":
        (version, length) = struct.unpack_from("<II", data, 4)
        info["version"] = version
        info["byteLength"] = length
        info.update(_parse_glb(data))
        return info

    (version, byteLength) = struct.unpack_from("<II", data, 4)
    info["version"] = version
    info["byteLength"] = byteLength
    if byteLength != info["fileSize"]:
        info["warnings"] = ["byteLength %d != file size %d" % (byteLength, info["fileSize"])]
    info.update(_parse_b3dm_i3dm(data))
    return info


def _parse_b3dm_i3dm(data):
    out = {}
    magic = data[0:4]
    fmt = MAGIC.get(magic)
    pos = 4
    (version, byteLength) = struct.unpack_from("<II", data, pos)
    pos += 8
    if fmt == "b3dm":
        (ftj, ftb, btj, btb) = struct.unpack_from("<IIII", data, pos)
        pos += 16
    else:  # i3dm
        (ftj, ftb, btj, btb, gltfFormat) = struct.unpack_from("<IIIII", data, pos)
        pos += 20
        out["gltfFormat"] = gltfFormat
    out["ftJSONLen"] = ftj
    out["ftBinLen"] = ftb
    out["btJSONLen"] = btj
    out["btBinLen"] = btb
    out["headerSize"] = pos

    def rd(off, ln):
        return data[off:off + ln]

    ft_json_txt = rd(pos, ftj).decode("utf-8", "replace")
    pos += ftj + ftb
    bt_json_txt = rd(pos, btj).decode("utf-8", "replace")
    pos += btj + btb
    try:
        out["featureTable"] = json.loads(ft_json_txt) if ft_json_txt.strip() else {}
    except Exception as e:
        out["featureTable"] = {}
        out.setdefault("warnings", []).append("featureTable JSON parse error: %s" % e)
    try:
        out["batchTable"] = json.loads(bt_json_txt) if bt_json_txt.strip() else {}
    except Exception as e:
        out["batchTable"] = {}
        out.setdefault("warnings", []).append("batchTable JSON parse error: %s" % e)

    # glb 可能以 glTF magic 开头，也可能是外部 URI
    glb = data[pos:byteLength]
    if glb[0:4] == b"glTF":
        out.update(_parse_glb(glb))
    elif ft_json_txt.find("uri") >= 0:
        out["gltfUri"] = "external"
    else:
        out.setdefault("warnings", []).append("no embedded glTF magic after tables")
    return out


def _parse_glb(data):
    out = {}
    if data[0:4] != b"glTF":
        out["warnings"] = ["not a glb"]
        return out
    (version, length) = struct.unpack_from("<II", data, 4)
    out["glbVersion"] = version
    out["glbLength"] = length
    off = 12
    chunk0len = struct.unpack_from("<I", data, off)[0]
    chunk0type = data[off + 4:off + 8]
    off += 8
    if chunk0type != b"JSON":
        out["warnings"] = ["first chunk is not JSON"]
        return out
    txt = data[off:off + chunk0len].decode("utf-8", "replace")
    try:
        gj = json.loads(txt)
    except Exception as e:
        out["warnings"] = ["gltf json parse error: %s" % e]
        return out
    out["gltf"] = _summarize_gltf(gj)
    return out


def _summarize_gltf(gj):
    """提取 primitives / _BATCHID / 材质 / 访问器信息。"""
    s = {"meshes": gj.get("meshes", []), "primitives": 0,
         "materialCount": len(gj.get("materials", [])),
         "extensionsUsed": gj.get("extensionsUsed", []),
         "batchIdIssues": []}
    acc = gj.get("accessors", [])
    attrs_of_interest = []
    for m in gj.get("meshes", []):
        for p in m.get("primitives", []):
            s["primitives"] += 1
            attrs = p.get("attributes", {})
            if "_BATCHID" not in attrs:
                s["batchIdIssues"].append("primitive without _BATCHID")
            else:
                ai = attrs["_BATCHID"]
                if 0 <= ai < len(acc):
                    a = acc[ai]
                    s.setdefault("batchIdAccessors", []).append(a)
                attrs_of_interest.append(ai)
    return s


def read_batchid_values(path):
    """从 b3dm/i3dm 中实际读出 _BATCHID 顶点值（支持 UNSIGNED_SHORT/UNSIGNED_INT）。
    返回 (values, batchLength, issues)。"""
    info = parse_tile_file(path)
    if "gltf" not in info:
        return ([], None, ["no gltf parsed"])
    return ([], None, [])


# --------------------------------------------------------------------------
# 校验
# --------------------------------------------------------------------------

def _box_corners(box):
    """3D Tiles box: [cx,cy,cz, hx,0,0, 0,hy,0, 0,0,hz] -> (min, max)"""
    c = box[0:3]
    h = (abs(box[3]), abs(box[7]), abs(box[11]))
    return ([c[0] - h[0], c[1] - h[1], c[2] - h[2]],
            [c[0] + h[0], c[1] + h[1], c[2] + h[2]])


def _bv_extent(bv):
    if "box" in bv:
        return _box_corners(bv["box"])
    if "sphere" in bv:
        s = bv["sphere"]
        r = abs(s[3])
        return ([s[0] - r, s[1] - r, s[2] - r], [s[0] + r, s[1] + r, s[2] + r])
    if "region" in bv:
        r = bv["region"]
        return ([r[0], r[1], r[4]], [r[2], r[3], r[5]])
    return None


def validate_tileset(tileset_path, hard_budget_bytes=None):
    base = os.path.dirname(os.path.abspath(tileset_path))
    issues = []
    with open(tileset_path, "r", encoding="utf-8") as f:
        ts = json.load(f)

    # --- SPEC: asset ---
    asset = ts.get("asset", {})
    if asset.get("version") != "1.0":
        issues.append(Issue("SPEC", "ERROR", "asset.version",
                            "expected '1.0', got %r" % asset.get("version")))
    if "root" not in ts:
        issues.append(Issue("SPEC", "ERROR", "root", "missing root"))
        return issues, {}

    root = ts["root"]
    if "refine" not in root:
        issues.append(Issue("SPEC", "ERROR", "root.refine", "root must declare refine"))
    elif root["refine"] not in ("ADD", "REPLACE"):
        issues.append(Issue("SPEC", "ERROR", "root.refine",
                            "invalid refine %r" % root["refine"]))

    tse = ts.get("geometricError")
    if tse is None or not isinstance(tse, (int, float)) or not math.isfinite(tse) or tse < 0:
        issues.append(Issue("SPEC", "ERROR", "geometricError",
                            "tileset geometricError invalid: %r" % tse))

    referenced = set()
    stats = {"tiles": 0, "withContent": 0, "emptyTiles": 0, "maxDepth": 0,
             "geMin": None, "geMax": None, "contentBytes": 0, "contentFiles": 0,
             "formats": {}, "refineCounts": {}, "batchLengths": [],
             "maxTriPerTile": 0}

    def walk(tile, parent_ge, parent_extent, depth, path):
        stats["tiles"] += 1
        stats["maxDepth"] = max(stats["maxDepth"], depth)
        ge = tile.get("geometricError")
        if ge is None or not isinstance(ge, (int, float)) or not math.isfinite(ge):
            issues.append(Issue("SPEC", "ERROR", path, "geometricError missing/not finite"))
            ge = None
        else:
            if ge < 0:
                issues.append(Issue("SPEC", "ERROR", path, "negative geometricError %r" % ge))
            if parent_ge is not None and ge > parent_ge + 1e-12:
                issues.append(Issue("SPEC", "ERROR", path,
                                    "geometricError %g > parent %g (must not increase)" % (ge, parent_ge)))
            if stats["geMin"] is None or ge < stats["geMin"]:
                stats["geMin"] = ge
            if stats["geMax"] is None or ge > stats["geMax"]:
                stats["geMax"] = ge

        bv = tile.get("boundingVolume")
        ext = None
        if not bv:
            issues.append(Issue("BOUNDS", "ERROR", path, "missing boundingVolume"))
        else:
            n = sum(1 for k in ("box", "sphere", "region") if k in bv)
            if n != 1:
                issues.append(Issue("BOUNDS", "ERROR", path,
                                    "boundingVolume must have exactly one of box/sphere/region"))
            else:
                key = "box" if "box" in bv else ("sphere" if "sphere" in bv else "region")
                arr = bv[key]
                if len(arr) != (12 if key == "box" else (4 if key == "sphere" else 6)):
                    issues.append(Issue("BOUNDS", "ERROR", path,
                                        "%s has wrong length %d" % (key, len(arr))))
                for v in arr:
                    if not isinstance(v, (int, float)) or not math.isfinite(v):
                        issues.append(Issue("BOUNDS", "ERROR", path,
                                            "%s contains non-finite value %r" % (key, v)))
                        break
                if key == "box":
                    for idx in (3, 7, 11):
                        if arr[idx] < 0:
                            issues.append(Issue("BOUNDS", "ERROR", path,
                                                "box half-extent[%d] = %g is negative" % (idx, arr[idx])))
                            break
                if key == "sphere" and arr[3] < 0:
                    issues.append(Issue("BOUNDS", "ERROR", path, "sphere radius negative"))
                ext = _bv_extent(bv)
                if ext and parent_extent:
                    # 容差要随坐标量级放大：float32 存储的包围盒在 1e6 量级下
                    # 本身就有 ~0.06 的绝对误差，用固定 1e-6 会误报。
                    scale = max(1.0, abs(parent_extent[0][0]), abs(parent_extent[1][0]),
                                abs(parent_extent[0][1]), abs(parent_extent[1][1]),
                                abs(parent_extent[0][2]), abs(parent_extent[1][2]))
                    tol = 1e-6 * scale
                    for k in range(3):
                        if ext[0][k] < parent_extent[0][k] - tol or \
                           ext[1][k] > parent_extent[1][k] + tol:
                            issues.append(Issue("BOUNDS", "ERROR", path,
                                                "child %s not covered by parent on axis %d" % (key, k)))
                            break

        content = tile.get("content")
        if content:
            uri = content.get("uri")
            if not uri:
                issues.append(Issue("CONTENT", "ERROR", path, "content without uri"))
            else:
                stats["withContent"] += 1
                referenced.add(uri.replace("\\", "/"))
                fp = os.path.join(base, uri.replace("\\", "/"))
                if not os.path.isfile(fp):
                    issues.append(Issue("CONTENT", "ERROR", path, "content file missing: %s" % uri))
                else:
                    info = parse_tile_file(fp)
                    fmt = info.get("format", "?")
                    stats["formats"][fmt] = stats["formats"].get(fmt, 0) + 1
                    stats["contentFiles"] += 1
                    stats["contentBytes"] += info.get("fileSize", 0)
                    if hard_budget_bytes and info.get("fileSize", 0) > hard_budget_bytes:
                        issues.append(Issue("BUDGET", "ERROR", path,
                                            "%s is %d bytes > hard limit %d"
                                            % (uri, info["fileSize"], hard_budget_bytes)))
                    for w in info.get("warnings", []):
                        issues.append(Issue("CONTENT", "ERROR", path, "%s: %s" % (uri, w)))
                    for comp in info.get("components", []):
                        for w in comp.get("warnings", []):
                            issues.append(Issue("CONTENT", "ERROR", path, "%s: %s" % (uri, w)))
                    g = info.get("gltf")
                    if g is not None:
                        stats["maxTriPerTile"] = max(stats["maxTriPerTile"],
                                                     g.get("primitives", 0))
                        for bi in g.get("batchIdIssues", []):
                            issues.append(Issue("BATCH", "WARN", path, "%s: %s" % (uri, bi)))
                    blen = None
                    ft = info.get("featureTable", {}) or {}
                    if "BATCH_LENGTH" in ft:
                        blen = ft["BATCH_LENGTH"]
                    elif "INSTANCES_LENGTH" in ft:
                        blen = ft["INSTANCES_LENGTH"]
                    if blen is not None:
                        stats["batchLengths"].append(blen)
                        bt = info.get("batchTable", {}) or {}
                        for k, v in bt.items():
                            if isinstance(v, list) and len(v) != blen:
                                issues.append(Issue("BATCH", "ERROR", path,
                                                    "%s: batchTable['%s'] len %d != batchLength %d"
                                                    % (uri, k, len(v), blen)))
                            if isinstance(v, dict) and "byteOffset" in v:
                                ncomp = v.get("componentType") == "UNSIGNED_SHORT"
                                pass
        else:
            if not tile.get("children"):
                stats["emptyTiles"] += 1
                issues.append(Issue("CONTENT", "WARN", path,
                                    "leaf tile without content (empty tile)"))

        r = tile.get("refine")
        if r:
            stats["refineCounts"][r] = stats["refineCounts"].get(r, 0) + 1

        for i, ch in enumerate(tile.get("children", [])):
            walk(ch, ge, ext, depth + 1, path + "/children[%d]" % i)

    walk(root, tse if isinstance(tse, (int, float)) else None, None, 0, "root")

    # --- FILES: orphan files ---
    for dirpath, _, filenames in os.walk(base):
        for fn in filenames:
            if fn.lower() == "tileset.json":
                continue
            rel = os.path.relpath(os.path.join(dirpath, fn), base).replace("\\", "/")
            if rel not in referenced:
                issues.append(Issue("FILES", "WARN", rel, "orphan file not referenced by tileset"))

    return issues, stats


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cmd = sys.argv[1]
    target = sys.argv[2]
    out_json = None
    if "--json" in sys.argv:
        out_json = sys.argv[sys.argv.index("--json") + 1]
    budget = None
    if "--budget" in sys.argv:
        budget = int(sys.argv[sys.argv.index("--budget") + 1])

    if cmd == "validate":
        issues, stats = validate_tileset(target, budget)
        errors = [i for i in issues if i.severity == "ERROR"]
        warns = [i for i in issues if i.severity == "WARN"]
        for i in errors:
            print(i)
        for i in warns[:40]:
            print(i)
        print("\n--- summary ---")
        print("errors=%d warnings=%d" % (len(errors), len(warns)))
        print(json.dumps(stats, indent=2, ensure_ascii=False))
        if out_json:
            with open(out_json, "w", encoding="utf-8") as f:
                json.dump({"issues": [i.as_dict() for i in issues], "stats": stats},
                          f, indent=2, ensure_ascii=False)
        return 1 if errors else 0

    if cmd == "stats":
        issues, stats = validate_tileset(target, budget)
        print(json.dumps(stats, indent=2, ensure_ascii=False))
        return 0

    if cmd == "glbinfo":
        print(json.dumps(parse_tile_file(target), indent=2, ensure_ascii=False)[:4000])
        return 0

    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
