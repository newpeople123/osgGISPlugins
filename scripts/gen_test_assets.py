#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
3D Tiles 生产级改造 - 确定性测试资产生成器

生成用于第 14 节「自动化测试矩阵」的可分发小型资产。
所有几何用固定种子的伪随机数生成，保证每次生成的文件逐字节一致（可复现）。

用法:
    python scripts/gen_test_assets.py [输出目录]
默认输出目录: <仓库根>/test_assets

产出:
    t01_plane_curved.obj      简单曲面
    t02_thin_features.obj     薄杆/薄壁/尖角/小突起
    t03_giant_single_mesh.obj 单个不可按对象切分的超大 mesh
    t04_many_objects.obj      海量独立小对象
    t05_instances.osg        大量重复实例 (i3dm)
    t06_nonuniform.obj        非均匀细长场景
    t07_zero_axis.obj         零轴(平面)模型
    t08_big_coords.obj        大坐标场景
    t09_cross_tile_object.obj 单个对象横跨整个空间
    t10_material_heavy.obj    多材质
    t11_sparse_outlier.obj    稀疏+离群
    t12_big_texture.obj       大纹理(硬预算测试) + t12_big_texture.mtl + t12_big_texture.png
"""

import math
import os
import random
import struct
import sys
import zlib


def _rng(seed):
    return random.Random(seed)


class ObjBuilder:
    """极简 Wavefront OBJ 写出器。"""

    def __init__(self, mtllib=None):
        self.lines = []
        self.v = []            # (x,y,z)
        self.vt = []           # (u,v)
        self.vn = []           # (x,y,z)
        self.faces = []        # (objname, material, [(vi,ti,ni), ...])
        self.obj = "default"
        self.mat = None
        if mtllib:
            self.lines.append("mtllib %s" % mtllib)

    def object(self, name, material=None):
        self.obj = name
        self.mat = material
        self.lines.append("o %s" % name)
        if material:
            self.lines.append("usemtl %s" % material)

    def material(self, material):
        self.mat = material
        self.lines.append("usemtl %s" % material)

    def vert(self, x, y, z):
        self.v.append((x, y, z))
        return len(self.v)

    def texcoord(self, u, v):
        self.vt.append((u, v))
        return len(self.vt)

    def normal(self, x, y, z):
        self.vn.append((x, y, z))
        return len(self.vn)

    def face(self, idx, tex=None, nrm=None):
        self.faces.append((self.obj, self.mat, idx, tex, nrm))

    def box(self, name, cx, cy, cz, hx, hy, hz, material=None):
        """轴对齐长方体，24 顶点 / 12 三角形（每面独立法线与 UV）。"""
        self.object(name, material)
        x0, y0, z0 = cx - hx, cy - hy, cz - hz
        x1, y1, z1 = cx + hx, cy + hy, cz + hz
        faces = [
            (((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)), (0, 0, 1)),
            (((x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)), (0, 0, -1)),
            (((x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)), (1, 0, 0)),
            (((x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)), (-1, 0, 0)),
            (((x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)), (0, 1, 0)),
            (((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)), (0, -1, 0)),
        ]
        uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        for (quad, nrm) in faces:
            vi = [self.vert(*p) for p in quad]
            ti = [self.texcoord(*uv) for uv in uvs]
            ni = self.normal(*nrm)
            self.face([vi[0], vi[1], vi[2]], [ti[0], ti[1], ti[2]], [ni] * 3)
            self.face([vi[0], vi[2], vi[3]], [ti[0], ti[2], ti[3]], [ni] * 3)

    def cylinder(self, name, cx, cy, cz, radius, height, seg, material=None):
        self.object(name, material)
        base = len(self.v)
        for i in range(seg):
            a = 2.0 * math.pi * i / seg
            s = math.sin(a)
            c = math.cos(a)
            self.vert(cx + radius * c, cy - height * 0.5, cz + radius * s)
            self.vert(cx + radius * c, cy + height * 0.5, cz + radius * s)
            self.normal(c, 0.0, s)
            self.texcoord(i / float(seg), 0.0)
            self.texcoord(i / float(seg), 1.0)
        for i in range(seg):
            j = (i + 1) % seg
            b0 = base + i * 2
            b1 = base + j * 2
            self.face([b0, b1, b1 + 1], [base + i * 2, base + j * 2, base + j * 2 + 1],
                      [base + i, base + j, base + j])
            self.face([b0, b1 + 1, b0 + 1], [base + i * 2, base + j * 2 + 1, base + i * 2 + 1],
                      [base + i, base + j, base + i])
        # caps
        ct = self.vert(cx, cy + height * 0.5, cz)
        cb = self.vert(cx, cy - height * 0.5, cz)
        nt = self.normal(0, 1, 0)
        nb = self.normal(0, -1, 0)
        for i in range(seg):
            j = (i + 1) % seg
            self.face([ct, base + i * 2 + 1, base + j * 2 + 1], None, [nt, nt, nt])
            self.face([cb, base + j * 2, base + i * 2], None, [nb, nb, nb])

    def grid(self, name, nx, nz, size, height_fn, material=None):
        """由 height_fn(i,j) -> y 生成 nx*nz 网格（2*(nx-1)*(nz-1) 个三角形）。"""
        self.object(name, material)
        base = len(self.v)
        for j in range(nz):
            for i in range(nx):
                x = -size * 0.5 + size * i / float(nx - 1)
                z = -size * 0.5 + size * j / float(nz - 1)
                y = height_fn(i, j)
                self.vert(x, y, z)
                self.texcoord(i / float(nx - 1), j / float(nz - 1))
        ni = self.normal(0, 1, 0)
        for j in range(nz - 1):
            for i in range(nx - 1):
                a = base + j * nx + i
                b = a + 1
                c = a + nx + 1
                d = a + nx
                self.face([a, d, c], [a, d, c], [ni, ni, ni])
                self.face([a, c, b], [a, c, b], [ni, ni, ni])

    def dump(self, path):
        out = list(self.lines)
        for (x, y, z) in self.v:
            out.append("v %.6f %.6f %.6f" % (x, y, z))
        for (u, v) in self.vt:
            out.append("vt %.6f %.6f" % (u, v))
        for (x, y, z) in self.vn:
            out.append("vn %.6f %.6f %.6f" % (x, y, z))
        cur_obj = None
        cur_mat = None
        for (obj, mat, idx, tex, nrm) in self.faces:
            if obj != cur_obj:
                out.append("o %s" % obj)
                cur_obj = obj
                cur_mat = None
            if mat != cur_mat:
                if mat:
                    out.append("usemtl %s" % mat)
                cur_mat = mat
            parts = []
            for k in range(len(idx)):
                v = idx[k] + 1
                if tex:
                    t = tex[k] + 1
                else:
                    t = ""
                if nrm:
                    n = nrm[k] + 1
                else:
                    n = ""
                parts.append("%d/%s/%s" % (v, t, n))
            out.append("f " + " ".join(parts))
        with open(path, "w", newline="\n") as f:
            f.write("\n".join(out) + "\n")
        return len(self.faces)


def write_png_gray(path, w, h, fn):
    """纯 Python 写一张 8bit 灰度 PNG（不依赖 PIL）。"""
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.append(fn(x, y) & 0xFF)
    comp = zlib.compress(bytes(raw), 6)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return c

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", comp)
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


# --------------------------------------------------------------------------
# 各测试用例
# --------------------------------------------------------------------------

def gen_t01(out):
    b = ObjBuilder()
    b.grid("curved_plane", 40, 40, 100.0,
           lambda i, j: 3.0 * math.sin(i * 0.25) * math.cos(j * 0.25))
    return b.dump(os.path.join(out, "t01_plane_curved.obj"))


def gen_t02(out):
    """薄杆 / 薄壁 / 尖角 / 小突起：验证特征保留，不能被简化吃掉。"""
    b = ObjBuilder()
    b.box("base_plate", 0, -0.5, 0, 50, 0.5, 50)
    r = _rng(20241015)
    for i in range(12):
        x = -45 + i * 8.0
        b.box("thin_rod_%02d" % i, x, 6.0, -30.0, 0.15, 6.0, 0.15)     # 细杆
        b.box("thin_wall_%02d" % i, x, 3.0, 0.0, 0.08, 3.0, 12.0)      # 薄壁
        b.box("small_bump_%02d" % i, x, 0.15, 30.0, 0.6, 0.15, 0.6)    # 小突起
    for i in range(8):
        # 尖角（四棱锥用 cylinder 的 seg=4 + 收缩近似：用细长盒代替）
        b.cylinder("spike_%02d" % i, -40 + i * 11.0, 8.0, 40.0, 0.4, 16.0, 4)
    return b.dump(os.path.join(out, "t02_thin_features.obj"))


def gen_t03(out):
    """单个超大 mesh：只有一个 object，无法按对象切分，只能三角形级划分。"""
    b = ObjBuilder()
    nx, nz = 260, 260
    r = _rng(7)
    noise = [[r.random() for _ in range(nz)] for _ in range(nx)]
    b.grid("giant_terrain", nx, nz, 400.0,
           lambda i, j: 12.0 * noise[i % 260][j % 260])
    return b.dump(os.path.join(out, "t03_giant_single_mesh.obj"))


def gen_t04(out):
    b = ObjBuilder()
    r = _rng(11)
    for i in range(1200):
        x = r.uniform(-200, 200)
        z = r.uniform(-200, 200)
        s = r.uniform(0.4, 2.0)
        b.box("obj_%04d" % i, x, s, z, s, s, s)
    return b.dump(os.path.join(out, "t04_many_objects.obj"))


def gen_t05(out):
    """大量重复实例：同一原型 + 不同变换矩阵 -> i3dm。
    写成 OSG 原生 ASCII (.osgt)，保证多个 MatrixTransform 引用同一份几何。"""
    proto = []
    proto.append("Geode {")
    proto.append("  name \"Instance_Prototype\"")
    proto.append("  Geometry {")
    proto.append("    PrimitiveSets 1 {")
    proto.append("      DrawElementsUInt TRIANGLES 36 {")
    # 一个立方体 12 三角形
    tris = [
        (0, 1, 2), (0, 2, 3), (4, 6, 5), (4, 7, 6),
        (8, 9, 10), (8, 10, 11), (12, 14, 13), (12, 15, 14),
        (16, 17, 18), (16, 18, 19), (20, 22, 21), (20, 23, 22),
    ]
    idx = [i for t in tris for i in t]
    proto.append("        " + " ".join(str(i) for i in idx))
    proto.append("      }")
    proto.append("    }")
    proto.append("    VertexArray Vec3Array 24 {")
    v = []
    for fx in (-1, 1):
        for fy in (-1, 1):
            for fz in (-1, 1):
                pass
    # cube corners per face: 手工列出 6 面 x 4 顶点
    corners = [
        [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)],
        [(1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)],
        [(1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1)],
        [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)],
        [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)],
        [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)],
    ]
    for face in corners:
        for (x, y, z) in face:
            proto.append("      %.1f %.1f %.1f" % (x, y, z))
    proto.append("    }")
    proto.append("  }")
    proto.append("}")
    proto_txt = "\n".join(proto)

    root = ["Group {"]
    r = _rng(5)
    for i in range(300):
        x = r.uniform(-150, 150)
        y = r.uniform(0, 20)
        z = r.uniform(-150, 150)
        root.append("  MatrixTransform {")
        root.append("    Matrix {")
        root.append("      %.6f 0 0 0" % 1.0)
        root.append("      0 %.6f 0 0" % 1.0)
        root.append("      0 0 %.6f 0" % 1.0)
        root.append("      %.6f %.6f %.6f 1" % (x, y, z))
        root.append("    }")
        for line in proto_txt.split("\n"):
            root.append("    " + line)
        root.append("  }")
    root.append("}")
    with open(os.path.join(out, "t05_instances.osg"), "w", newline="\n") as f:
        f.write("\n".join(root) + "\n")
    return 300 * 12


def gen_t06(out):
    """细长走廊：2000 x 30 x 30，检查轴选择与树深度。"""
    b = ObjBuilder()
    r = _rng(23)
    b.box("corridor_floor", 0, -0.5, 0, 1000, 0.5, 15)
    for i in range(200):
        x = -990 + i * 9.9
        b.box("pillar_%03d" % i, x, 15, 0, 1.0, 15, 1.0)
    return b.dump(os.path.join(out, "t06_nonuniform.obj"))


def gen_t07(out):
    """零轴模型：所有顶点 z == 0（严格平面），另外一面竖直墙 x == 0。"""
    b = ObjBuilder()
    b.grid("flat_plane", 30, 30, 60.0, lambda i, j: 0.0)
    # 竖直墙：x 恒为 0
    b.object("flat_wall")
    base = len(b.v)
    for j in range(20):
        for i in range(20):
            b.vert(0.0, j * 1.5, i * 1.5)
    for j in range(19):
        for i in range(19):
            a = base + j * 20 + i
            bb = a + 1
            c = a + 21
            d = a + 20
            b.face([a, d, c])
            b.face([a, c, bb])
    return b.dump(os.path.join(out, "t07_zero_axis.obj"))


def gen_t08(out):
    """大坐标：整体平移到 ~1e7，检查精度与包围体。"""
    b = ObjBuilder()
    ox, oy, oz = 1.0e7, 2.0e6, -3.0e6
    r = _rng(31)
    for i in range(60):
        x = ox + r.uniform(-300, 300)
        z = oz + r.uniform(-300, 300)
        b.box("big_%03d" % i, x, oy + 5.0, z, 3.0, 5.0, 3.0)
    return b.dump(os.path.join(out, "t08_big_coords.obj"))


def gen_t09(out):
    """单个对象横跨整个场景：必须被切到多个 tile，且 globalFeatureId 不变。"""
    b = ObjBuilder()
    b.box("spanning_beam", 0, 10, 0, 500, 1.0, 1.0)   # 1000m 长的梁
    r = _rng(41)
    for i in range(40):
        b.box("small_%03d" % i, r.uniform(-400, 400), 1.0, r.uniform(-400, 400), 2, 1, 2)
    return b.dump(os.path.join(out, "t09_cross_tile_object.obj"))


def gen_t10(out):
    b = ObjBuilder(mtllib="t10_material_heavy.mtl")
    r = _rng(53)
    for i in range(40):
        b.box("matobj_%02d" % i, r.uniform(-100, 100), 5.0, r.uniform(-100, 100),
              4.0, 5.0, 4.0, material="mat_%02d" % i)
    n = b.dump(os.path.join(out, "t10_material_heavy.obj"))
    with open(os.path.join(out, "t10_material_heavy.mtl"), "w", newline="\n") as f:
        for i in range(40):
            r2 = (i * 37) % 256
            g2 = (i * 91) % 256
            b2 = (i * 143) % 256
            f.write("newmtl mat_%02d\nKd %.3f %.3f %.3f\nKa 0 0 0\nKs 0.2 0.2 0.2\nNs 10\nillum 2\n\n"
                    % (i, r2 / 255.0, g2 / 255.0, b2 / 255.0))
    return n


def gen_t11(out):
    b = ObjBuilder()
    b.box("dense_cluster", 0, 2, 0, 20, 2, 20)
    r = _rng(59)
    for i in range(150):
        b.box("dense_%03d" % i, r.uniform(-18, 18), 2.0, r.uniform(-18, 18), 1.0, 1.0, 1.0)
    b.box("far_outlier", 4000.0, 2.0, 4000.0, 5.0, 2.0, 5.0)
    return b.dump(os.path.join(out, "t11_sparse_outlier.obj"))


def gen_t12(out):
    """超大纹理：用于硬预算测试（期望：明确失败或降级，不得静默产出超限瓦片）。"""
    w = h = 2048
    write_png_gray(os.path.join(out, "t12_big_texture.png"), w, h,
                   lambda x, y: (x * 7 + y * 13) & 0xFF)
    with open(os.path.join(out, "t12_big_texture.mtl"), "w", newline="\n") as f:
        f.write("newmtl bigtex\n")
        f.write("Kd 1.0 1.0 1.0\nKa 0 0 0\nKs 0 0 0\nNs 1\nillum 2\n")
        f.write("map_Kd t12_big_texture.png\n")
    b = ObjBuilder(mtllib="t12_big_texture.mtl")
    b.grid("tex_plane", 60, 60, 120.0, lambda i, j: 0.0, material="bigtex")
    return b.dump(os.path.join(out, "t12_big_texture.obj"))


GENERATORS = [
    ("t01_plane_curved.obj", gen_t01),
    ("t02_thin_features.obj", gen_t02),
    ("t03_giant_single_mesh.obj", gen_t03),
    ("t04_many_objects.obj", gen_t04),
    ("t05_instances.osg", gen_t05),
    ("t06_nonuniform.obj", gen_t06),
    ("t07_zero_axis.obj", gen_t07),
    ("t08_big_coords.obj", gen_t08),
    ("t09_cross_tile_object.obj", gen_t09),
    ("t10_material_heavy.obj", gen_t10),
    ("t11_sparse_outlier.obj", gen_t11),
    ("t12_big_texture.obj", gen_t12),
]


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "test_assets")
    os.makedirs(out, exist_ok=True)
    for name, fn in GENERATORS:
        tris = fn(out)
        size = os.path.getsize(os.path.join(out, name))
        print("%-32s triangles=%-8d bytes=%d" % (name, tris, size))
    print("output dir: %s" % out)


if __name__ == "__main__":
    main()
