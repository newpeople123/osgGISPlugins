// 生产级管线单元测试（手册 5/6/7/9 节）
//
// 运行：test_production.exe          跑全部用例
//       test_production.exe <name>   只跑名字含 <name> 的用例
//
// 退出码：0 全通过；1 有用例失败。
// 注意：不依赖任何外部资产，全部用例在内存里程序化构造几何。
#include "3dtiles/production/ProductionPipeline.h"
#include "3dtiles/production/GeometryErrorEvaluator.h"
#include "3dtiles/production/ProxyMeshBuilder.h"
#include "3dtiles/production/SourceModel.h"
#include "3dtiles/production/SpatialPartitioner.h"
#include "3dtiles/production/ProductionProfile.h"
#include "3dtiles/production/ObjIntegrityPrecheck.h"
#include "utils/GltfOptimizer.h"
#include "utils/Simplifier.h"
#include "utils/Utils.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include "osgdb_gltf/material/GltfPbrMRMaterial.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Array>
#include <osg/PrimitiveSet>
#include <osg/ComputeBoundsVisitor>
#include <osg/Texture2D>
#include <osg/StateSet>
#include <osg/Image>
#include <osgDB/ReadFile>
#include <iostream>
#include <fstream>
#include <sstream>
#include <functional>
#include <cmath>
#include <exception>
#include <cstdlib>
#include <limits>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace osgGISPlugins;
using namespace osgGISPlugins::production;

namespace {

int g_failed = 0;
int g_passed = 0;

void check(bool ok, const std::string& name, const std::string& detail = std::string())
{
    if (ok) { ++g_passed; }
    else
    {
        ++g_failed;
        std::cout << "  FAIL: " << name;
        if (!detail.empty()) { std::cout << "  (" << detail << ")"; }
        std::cout << std::endl;
    }
}

void checkNear(double got, double want, double tol, const std::string& name)
{
    const bool ok = (got >= want - tol) && (got <= want + tol);
    check(ok, name, "got=" + std::to_string(got) + " want=" + std::to_string(want)
                    + " tol=" + std::to_string(tol));
}

// 失败/警告清单压成一行，便于在 check() 的 detail 里直接看到原因
std::string joinMessages(const std::vector<std::string>& msgs)
{
    std::string s;
    for (std::size_t i = 0; i < msgs.size() && i < 4; ++i)
    {
        s += (i ? " | " : "") + msgs[i];
    }
    if (msgs.size() > 4) { s += " | ...(" + std::to_string(msgs.size()) + " total)"; }
    return s;
}

// 程序化构造：gridX * gridY 个四边形（每格 2 三角形），整体铺在 XZ 平面
osg::ref_ptr<osg::Node> makeGrid(int nx, int ny, double cell, int quadsPerObject = 1)
{
    osg::ref_ptr<osg::Group> root = new osg::Group;
    int objId = 0;
    for (int gy = 0; gy < ny; ++gy)
    {
        for (int gx = 0; gx < nx; ++gx)
        {
            osg::ref_ptr<osg::Geode> geode = new osg::Geode;
            geode->setName("grid_" + std::to_string(objId++));
            osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
            osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
            osg::ref_ptr<osg::DrawElementsUInt> idx =
                new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
            const double x0 = gx * cell, y0 = gy * cell;
            for (int q = 0; q < quadsPerObject; ++q)
            {
                const double t = static_cast<double>(q) / static_cast<double>(quadsPerObject);
                const double xa = x0, xb = x0 + cell * (0.5 + 0.5 * t);
                const double ya = y0 + cell * t, yb = y0 + cell * (t + 1.0 / quadsPerObject);
                pos->push_back(osg::Vec3(xa, ya, 0.0f));
                pos->push_back(osg::Vec3(xb, ya, 0.0f));
                pos->push_back(osg::Vec3(xb, yb, 0.0f));
                pos->push_back(osg::Vec3(xa, yb, 0.0f));
                const unsigned b = static_cast<unsigned>(pos->size()) - 4;
                idx->push_back(b); idx->push_back(b + 1); idx->push_back(b + 2);
                idx->push_back(b); idx->push_back(b + 2); idx->push_back(b + 3);
            }
            geom->setVertexArray(pos.get());
            geom->addPrimitiveSet(idx.get());
            geode->addDrawable(geom.get());
            root->addChild(geode.get());
        }
    }
    return root;
}

// ---------------------------------------------------------------------------
void testProfileValidation()
{
    std::cout << "[profile] validate" << std::endl;
    ProductionProfile ok;
    bool threw = false;
    try { ok.validate(); } catch (const std::exception&) { threw = true; }
    check(!threw, "default profile is valid");

    ProductionProfile bad = ok;
    bad.budget.maxTrianglesPerTile = 0;
    threw = false;
    try { bad.validate(); } catch (const std::exception&) { threw = true; }
    check(threw, "maxTrianglesPerTile=0 must throw (not silently clamp)");

    ProductionProfile bad2 = ok;
    bad2.budget.targetTrianglesPerTile = bad2.budget.maxTrianglesPerTile + 1;
    threw = false;
    try { bad2.validate(); } catch (const std::exception&) { threw = true; }
    check(threw, "target > max must throw");

    ProductionProfile bad3 = ok;
    bad3.budget.proxyTextureMaxEdgePx = proxyTextureFloorEdgePx - 1u;
    threw = false;
    try { bad3.validate(); } catch (const std::exception&) { threw = true; }
    check(threw, "proxy texture cap below the floor must throw (not silently clamp)");
    check(ok.budget.proxyTextureMaxEdgePx >= proxyTextureFloorEdgePx,
          "default profile keeps a usable proxy texture cap");
}

void testSourceModelBaking()
{
    std::cout << "[sourceModel] world transform baking" << std::endl;
    osg::ref_ptr<osg::Group> root = new osg::Group;
    osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
    mt->setMatrix(osg::Matrix::translate(osg::Vec3(100.0, 0.0, 0.0)));
    osg::ref_ptr<osg::Geode> g = new osg::Geode;
    g->setName("moved");
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
    pos->push_back(osg::Vec3(0, 0, 0));
    pos->push_back(osg::Vec3(1, 0, 0));
    pos->push_back(osg::Vec3(0, 1, 0));
    geom->setVertexArray(pos.get());
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3));
    g->addDrawable(geom.get());
    mt->addChild(g.get());
    root->addChild(mt.get());

    SourceModel m;
    check(m.build(root.get()), "build succeeds");
    check(m.featureCount() == 1, "one feature", "got " + std::to_string(m.featureCount()));
    if (!m.prims.empty())
    {
        const osg::BoundingBox& bb = m.bounds;
        checkNear(bb.xMin(), 100.0, 1e-4, "world transform baked into bounds.xMin");
        checkNear(bb.xMax(), 101.0, 1e-4, "world transform baked into bounds.xMax");
    }
}

// A-56：越界顶点索引必须"丢图元 + 显式记账 + 计入报告"（用户裁定的语义）。
// 夹具侧的 7 个 OBJ 有 `f` 行引用不存在的 v（t06 的 4825 越出 4824），旧代码在
// PrimRef 指标的 (*pos)[idx[k]] 处直接解引用，读到数组外的垃圾顶点，污染
// pr.aabb / centroid，切分树随进程堆布局漂移（t06 31→33 瓦片、t11 半轴漂移）。
void testIndexValidationDropsOffendingTriangles()
{
    std::cout << "[indexGuard] out-of-range vertex index: drop + account" << std::endl;

    auto geomOf = [](osg::Vec3Array* pos, osg::PrimitiveSet* ps) {
        osg::ref_ptr<osg::Geometry> g = new osg::Geometry;
        g->setVertexArray(pos);
        g->addPrimitiveSet(ps);
        return g;
    };
    auto quadPos = []() {
        osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
        pos->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
        pos->push_back(osg::Vec3(4.0f, 0.0f, 0.0f));
        pos->push_back(osg::Vec3(0.0f, 3.0f, 0.0f));
        pos->push_back(osg::Vec3(9.0f, 9.0f, 0.0f));
        return pos;
    };
    auto sceneOf = [](const std::string& name, osg::Geometry* g) {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->setName(name);
        geode->addDrawable(g);
        root->addChild(geode.get());
        return root;
    };

    // 1) 全部合法：一条账都不许多记，一个三角形都不许多丢
    {
        osg::ref_ptr<osg::Vec3Array> pos = quadPos();
        osg::ref_ptr<osg::DrawElementsUInt> idx =
            new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
        idx->push_back(0); idx->push_back(1); idx->push_back(2);
        idx->push_back(2); idx->push_back(3); idx->push_back(0);
        SourceModel m;
        check(m.build(sceneOf("legal", geomOf(pos.get(), idx.get())).get()), "legal scene builds");
        check(m.indexViolations.empty(), "legal scene records no violation",
              "got " + std::to_string(m.indexViolations.size()));
        check(m.totalTriangles() == 2, "legal scene keeps both triangles",
              "got " + std::to_string(m.totalTriangles()));
    }

    // 2) 一个越界三角形：丢它、留合法的、记账字段齐、顶点数组一个字节不动
    {
        osg::ref_ptr<osg::Vec3Array> pos = quadPos();
        osg::ref_ptr<osg::DrawElementsUInt> idx =
            new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
        idx->push_back(0); idx->push_back(1); idx->push_back(2);
        idx->push_back(2); idx->push_back(3); idx->push_back(4);   // 4 >= vertexCount(4)
        SourceModel m;
        check(m.build(sceneOf("bad_one", geomOf(pos.get(), idx.get())).get()),
              "scene with one offending triangle builds");
        check(m.indexViolations.size() == 1, "one violation accounted",
              "got " + std::to_string(m.indexViolations.size()));
        if (m.indexViolations.size() == 1)
        {
            const IndexViolation& v = m.indexViolations[0];
            check(v.droppedTriangles == 1, "one triangle dropped",
                  "got " + std::to_string(v.droppedTriangles));
            check(v.vertexCount == 4, "accounted vertex bound",
                  "got " + std::to_string(v.vertexCount));
            check(v.maxOffendingIndex == 4, "accounted offending index",
                  "got " + std::to_string(v.maxOffendingIndex));
            check(v.nodeName == "bad_one", "accounted node name", "got '" + v.nodeName + "'");
            const bool sameFeature = !m.objects.empty()
                && v.globalFeatureId == m.objects[0].globalFeatureId;
            check(sameFeature, "accounted feature id points at the offending object");
        }
        check(m.totalTriangles() == 1, "only the valid triangle survives",
              "got " + std::to_string(m.totalTriangles()));
        if (!m.meshes.empty() && !m.meshes[0].primSets.empty())
        {
            const std::vector<unsigned>& tris = m.meshes[0].primSets[0].tris;
            bool allInRange = true;
            for (unsigned i : tris) { if (i >= 4u) { allInRange = false; } }
            check(allInRange, "no stored index reaches past the vertex array");
            check(tris.size() == 3, "kept primitive set is exactly one triangle",
                  "got " + std::to_string(tris.size()));
        }
        check(!m.meshes.empty() && m.meshes[0].positions->size() == 4,
              "vertex array itself is neither truncated nor padded");
        // 旧缺陷的产品级症状：垃圾顶点被算进 PrimRef 指标
        if (!m.prims.empty())
        {
            check(m.prims[0].triCount == 1, "prim metric counts only kept triangles",
                  "got " + std::to_string(m.prims[0].triCount));
            const osg::BoundingBox& bb = m.prims[0].aabb;
            check(bb.xMin() == 0.0f && bb.xMax() == 4.0f
                    && bb.yMin() == 0.0f && bb.yMax() == 3.0f
                    && bb.zMin() == 0.0f && bb.zMax() == 0.0f,
                  "prim aabb holds no out-of-array vertex",
                  "got [" + std::to_string(bb.xMin()) + "," + std::to_string(bb.xMax()) + ","
                  + std::to_string(bb.yMin()) + "," + std::to_string(bb.yMax()) + "]");
        }
    }

    // 3) 整图元全越界：几何没了、对象也没有几何，但账必须还在（不许静默丢弃）
    {
        osg::ref_ptr<osg::Vec3Array> pos = quadPos();
        osg::ref_ptr<osg::DrawElementsUInt> idx =
            new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
        idx->push_back(4); idx->push_back(5); idx->push_back(6);
        SourceModel m;
        m.build(sceneOf("bad_all", geomOf(pos.get(), idx.get())).get());
        check(m.indexViolations.size() == 1, "fully offending primitive set still accounted",
              "got " + std::to_string(m.indexViolations.size()));
        check(m.indexViolations.empty() || m.indexViolations[0].droppedTriangles == 1,
              "offending triangle count accounted");
        check(m.totalTriangles() == 0, "no fabricated geometry kept");
        check(m.prims.empty(), "no prim refers to an emptied primitive set");
    }

    // 4) QUADS：过滤发生在展开后的三角形上，一个越界角点废掉整个四边形的两个面。
    //    UShort 索引 9 在 ushort 范围内、但越出顶点数组——正是 osgdb_obj 的实际形态。
    {
        osg::ref_ptr<osg::Vec3Array> pos = quadPos();
        osg::ref_ptr<osg::DrawElementsUShort> idx =
            new osg::DrawElementsUShort(osg::PrimitiveSet::QUADS);
        idx->push_back(0); idx->push_back(1); idx->push_back(2); idx->push_back(3);
        idx->push_back(1); idx->push_back(2); idx->push_back(9); idx->push_back(0);
        SourceModel m;
        m.build(sceneOf("bad_quad", geomOf(pos.get(), idx.get())).get());
        check(m.totalTriangles() == 2, "good quad kept, offending quad dropped",
              "got " + std::to_string(m.totalTriangles()));
        const unsigned dropped = m.indexViolations.empty()
            ? 0u : m.indexViolations[0].droppedTriangles;
        check(dropped == 2, "both derived triangles of the bad quad accounted",
              "got " + std::to_string(dropped));
        check(m.indexViolations.empty() || m.indexViolations[0].maxOffendingIndex == 9,
              "ushort-sourced offending index accounted");
    }

    // 5) 记账顺序确定：同一场景两次 build 得到逐字段相同的清单
    {
        auto twice = [&](SourceModel& m) {
            osg::ref_ptr<osg::Vec3Array> pos = quadPos();
            osg::ref_ptr<osg::DrawElementsUInt> idx =
                new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
            idx->push_back(0); idx->push_back(1); idx->push_back(2);
            idx->push_back(2); idx->push_back(3); idx->push_back(7);
            m.build(sceneOf("det", geomOf(pos.get(), idx.get())).get());
        };
        SourceModel a, b;
        twice(a);
        twice(b);
        check(!a.indexViolations.empty()
                && a.indexViolations.size() == b.indexViolations.size(),
              "both runs account the same number of sets");
        bool same = a.indexViolations.size() == b.indexViolations.size();
        for (std::size_t i = 0; same && i < a.indexViolations.size(); ++i)
        {
            same = a.indexViolations[i].globalFeatureId == b.indexViolations[i].globalFeatureId
                && a.indexViolations[i].nodeName == b.indexViolations[i].nodeName
                && a.indexViolations[i].droppedTriangles == b.indexViolations[i].droppedTriangles
                && a.indexViolations[i].vertexCount == b.indexViolations[i].vertexCount
                && a.indexViolations[i].maxOffendingIndex == b.indexViolations[i].maxOffendingIndex;
        }
        check(same, "accounting is a pure function of the input, not of the heap");
    }

    // 6) 静态入口 repairZeroNormals 同样不许被越界索引带出数组外
    {
        osg::ref_ptr<osg::Vec3Array> pos = quadPos();
        osg::ref_ptr<osg::Vec3Array> nrm = new osg::Vec3Array;
        for (unsigned i = 0; i < 4; ++i) { nrm->push_back(osg::Vec3(0.0f, 0.0f, 0.0f)); }
        std::vector<unsigned> tris = {0, 1, 2, 2, 3, 40};
        const auto rc = SourceModel::repairZeroNormals(pos.get(), nrm.get(), tris);
        check(rc.fromFaces == 3 && rc.fromPrimitive == 1 && rc.leftZero == 0,
              "offending triangle treated as degenerate, not read",
              "fromFaces=" + std::to_string(rc.fromFaces)
              + " fromPrimitive=" + std::to_string(rc.fromPrimitive)
              + " leftZero=" + std::to_string(rc.leftZero));
        for (unsigned v = 0; v < 4; ++v)
        {
            checkNear((*nrm)[v].length(), 1.0, 1e-5, "vertex v" + std::to_string(v) + " repaired");
        }
    }

    // 7) 记账要一路走到 conversion-report 的 warnings + counters（"计入报告"）
    {
        Utils::registerFileAliases();
        ProductionProfile profile;
        osg::ref_ptr<osg::Group> root = new osg::Group;
        root->addChild(makeGrid(1, 1, 1.0).get());
        osg::ref_ptr<osg::Vec3Array> pos = quadPos();
        osg::ref_ptr<osg::DrawElementsUInt> idx =
            new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
        idx->push_back(0); idx->push_back(1); idx->push_back(2);
        idx->push_back(2); idx->push_back(3); idx->push_back(4);
        root->addChild(sceneOf("reported_bad", geomOf(pos.get(), idx.get())).get());

        const std::filesystem::path out =
            std::filesystem::temp_directory_path() / "wb_pipeline_indexguard";
        std::error_code ec;
        std::filesystem::remove_all(out, ec);
        ProductionPipeline pipeline(profile);
        ProductionPipeline::Options opt;
        opt.outputDir = out.string();
        pipeline.run(root.get(), opt);

        bool hasSummary = false;
        for (const std::string& w : pipeline.warnings())
        {
            if (w.find("index validation: dropped 1 triangle") != std::string::npos) { hasSummary = true; }
        }
        check(hasSummary, "report carries the index-validation warning",
              joinMessages(pipeline.warnings()));
        std::uint64_t reported = 0;
        for (const StageCount& c : pipeline.report().counters)
        {
            if (c.name == "outOfRangeIndexTriangles") { reported = c.value; }
        }
        check(reported == 1, "report counter says one triangle dropped",
              "got " + std::to_string(reported));
    }
}

// ---------------------------------------------------------------------------
// A-57：读模**之前**的原始 OBJ 面索引预检。
//
// 为什么读模后的守卫不够：osgdb_obj 不交付文件里的 `f` 索引，而是按引用把 `v` 坐标
// 逐个拷进几何自己的数组，越界引用因此被"吸收"成本地 0..N-1 的合法下标（Q2/A-56 那道
// 守卫结构性失明，见 ObjIntegrityPrecheck.h 的头注释与 A56_ROOT_CAUSE.md）。
//
// 定位：这些是**格式专项单元测试**，只验预检器自己的判定语义（三个索引空间、正/负/
// 0/非整数/前向引用/续行/噪声）。它们不构成、也不替代真实 FBX 端到端回归，更不能据此
// 声称任何真实资产干净——本轮没有任何真实 .obj 资产可跑（详见 STATUS.md 的"未执行"栏）。
void testObjIntegrityPrecheck()
{
    std::cout << "[objPrecheck] A-57 raw OBJ face-index precheck" << std::endl;
    namespace fs = std::filesystem;

    std::error_code ec;
    const std::string dir = (fs::temp_directory_path() / "wb_obj_precheck").string();
    fs::remove_all(Utils::utf8ToPath(dir), ec);
    fs::create_directories(Utils::utf8ToPath(dir), ec);

    // 夹具路径一律用 std::string 拼接再交 Utils::utf8ToPath：窄字符 path 正是 A-59 里
    // 会被 C locale 代码页解错的那条入口，测试不能踩在同一条路上冒充覆盖。
    auto write = [&dir](const std::string& name, const std::string& text) {
        const std::string p = dir + "/" + name;
        std::ofstream f(Utils::utf8ToPath(p), std::ios::binary | std::ios::trunc);
        f << text;
        f.close();
        return p;
    };
    auto fileSize = [&ec](const std::string& p) {
        return static_cast<std::uint64_t>(fs::file_size(Utils::utf8ToPath(p), ec));
    };
    auto firstWhy = [](const ObjPrecheckResult& r) {
        return r.violations.empty() ? std::string() : r.violations.front().why;
    };
    auto msgBlob = [](const ObjPrecheckResult& r) {
        std::string s;
        for (const std::string& m : r.messages()) { s += m + " ;; "; }
        return s;
    };

    // 1) 阳性对照：四种面形态全合法，0 违规。
    //    "0 违规"只有在解析**真的发生**时才算证据，所以同时钉住解析计数：
    //    24 条引用（3 + 6 + 6 + 9）和三个空间各自的 maxRef 都非零。
    const std::string cleanForms = write("clean_four_forms.obj",
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvn 0 0 1\n"
        "f 1 2 3\nf 1/1 2/2 3/3\nf 1//1 2//1 3//1\nf 1/1/1 2/2/1 3/3/1\n");
    {
        const ObjPrecheckResult r = precheckObjFile(cleanForms);
        check(r.ran && r.readable, "clean fixture opened and scanned");
        check(r.hardViolations == 0 && r.badFaces == 0 && r.forwardRefs == 0
                  && r.violations.empty() && r.clean(),
              "positive control: four legal face forms give zero violations", msgBlob(r));
        check(r.faces == 4 && r.numV == 4 && r.numVT == 3 && r.numVN == 1,
              "counts: 4 faces / 4 v / 3 vt / 1 vn",
              "faces=" + std::to_string(r.faces) + " v=" + std::to_string(r.numV)
              + " vt=" + std::to_string(r.numVT) + " vn=" + std::to_string(r.numVN));
        check(r.checkedRefs == 24, "all 24 references were checked",
              "got " + std::to_string(r.checkedRefs));
        check(r.maxRefV == 3 && r.maxRefVT == 3 && r.maxRefVN == 1,
              "per-space max reference",
              "v=" + std::to_string(r.maxRefV) + " t=" + std::to_string(r.maxRefVT)
              + " n=" + std::to_string(r.maxRefVN));
        check(r.bytes == fileSize(cleanForms) && r.lines == 12,
              "buffered reader accounts every byte and line",
              "bytes=" + std::to_string(r.bytes) + "/" + std::to_string(fileSize(cleanForms))
              + " lines=" + std::to_string(r.lines));
    }

    // 2) A-56/A-57 的确切形态：文件声明 3 条 v，面引用第 4 条。
    const std::string beyondTotal = write("beyond_total.obj",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n");
    {
        const ObjPrecheckResult r = precheckObjFile(beyondTotal);
        check(!r.clean() && r.hardViolations == 1, "beyond-total is a hard violation", msgBlob(r));
        check(r.badFaces == 1 && r.faces == 1, "one face flagged, not three triangles",
              "badFaces=" + std::to_string(r.badFaces) + " faces=" + std::to_string(r.faces));
        check(firstWhy(r) == "beyond-total", "why code is beyond-total", firstWhy(r));
        if (!r.violations.empty())
        {
            const ObjRefViolation& v = r.violations.front();
            check(v.line == 4 && v.space == ObjRefSpace::Vertex && v.resolved == 4
                      && v.capSoFar == 3 && v.capTotal == 3 && v.token == "4" && v.hard,
                  "violation carries line 4 / v-space / resolved 4 / capTotal 3",
                  "line=" + std::to_string(v.line) + " resolved=" + std::to_string(v.resolved)
                  + " capTotal=" + std::to_string(v.capTotal) + " token=" + v.token);
        }
        const std::string m = msgBlob(r);
        check(m.find("line 4") != std::string::npos && m.find("beyond-total") != std::string::npos
                  && m.find("only 3") != std::string::npos && m.find("f 1 2 4") != std::string::npos,
              "message names the line, the space, the total and the offending text", m);
    }

    // 3) 索引 0：OBJ 是 1 基，0 不是合法引用。
    {
        const std::string p = write("index_zero.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 0\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(firstWhy(r) == "index-zero" && r.hardViolations == 1 && r.badFaces == 1
                  && !r.clean(), "index 0 is rejected as index-zero", msgBlob(r));
    }

    // 4) 负数（相对）引用合法：resolved = 到此为止的条数 + raw + 1。阳性对照。
    {
        const std::string p = write("negative_ok.obj",
            "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nf -1 -2 -3\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(r.clean() && r.hardViolations == 0 && r.forwardRefs == 0,
              "legal relative references are not violations", msgBlob(r));
        check(r.checkedRefs == 3 && r.maxRefV == 4,
              "relative refs resolve against the 4 declared vertices",
              "checked=" + std::to_string(r.checkedRefs) + " maxRefV=" + std::to_string(r.maxRefV));
    }

    // 5) 负得过头：只有 2 条 v 时 -3 回溯到第 1 条之前。
    {
        const std::string p = write("negative_underflow.obj", "v 0 0 0\nv 1 0 0\nf -3 -2 -1\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(firstWhy(r) == "negative-underflow" && r.hardViolations == 1 && r.badFaces == 1
                  && !r.clean(), "underflowing relative reference is rejected", msgBlob(r));
        if (!r.violations.empty())
        {
            const ObjRefViolation& v = r.violations.front();
            check(v.raw == -3 && v.line == 3 && v.capSoFar == 2,
                  "underflow records raw -3 on line 3 with 2 declared",
                  "raw=" + std::to_string(v.raw) + " line=" + std::to_string(v.line)
                  + " capSoFar=" + std::to_string(v.capSoFar));
        }
    }

    // 6) 三个索引空间互相独立：vt 越界不连坐 v。
    {
        const std::string p = write("space_independent.obj",
            "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nf 1/1 2/2 3/9\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(r.hardViolations == 1 && !r.violations.empty()
                  && r.violations.front().space == ObjRefSpace::Texture,
              "only the vt reference is flagged", msgBlob(r));
        check(r.numVT == 2 && r.maxRefVT == 9 && r.maxRefVT > r.numVT,
              "vt max reference exceeds declared vt count (the only signal the reader hides)",
              "numVT=" + std::to_string(r.numVT) + " maxRefVT=" + std::to_string(r.maxRefVT));
        check(r.maxRefV == 3 && r.numV == 3, "the v space stays untouched",
              "maxRefV=" + std::to_string(r.maxRefV) + " numV=" + std::to_string(r.numV));
        check(r.checkedRefs == 6 && r.badFaces == 1, "6 refs / 1 bad face",
              "checked=" + std::to_string(r.checkedRefs) + " badFaces=" + std::to_string(r.badFaces));
    }

    // 7) 合法的前向引用（f 在 v 之前）只警告，不硬拒：导出器常见写法。
    {
        const std::string p = write("forward_ref.obj",
            "f 1 2 3\nv 0 0 0\nv 1 0 0\nv 0 1 0\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(r.forwardRefs == 3 && r.hardViolations == 0 && r.badFaces == 0,
              "forward references are counted, not rejected", msgBlob(r));
        check(r.clean(), "a file with only forward references stays clean");
        check(r.violations.size() == 3 && !r.violations.empty()
                  && r.violations.front().why == "forward-reference"
                  && !r.violations.front().hard,
              "forward refs are recorded as soft violations",
              "n=" + std::to_string(r.violations.size()));
    }

    // 8) n 边面（多边形）：一个面 5 个引用，只有引用号问题算一条违规、一个坏面。
    {
        const std::string p = write("ngon.obj",
            "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4 5\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(r.faces == 1 && r.checkedRefs == 5, "one polygon, five references",
              "faces=" + std::to_string(r.faces) + " checked=" + std::to_string(r.checkedRefs));
        check(r.hardViolations == 1 && r.badFaces == 1 && firstWhy(r) == "beyond-total",
              "the polygon is one bad face, not three triangles", msgBlob(r));
    }

    // 9) 噪声、CRLF、行内注释、非计数行与反斜杠续行：不能污染任何计数。阳性对照。
    {
        const std::string p = write("noisy.obj",
            "# header comment\r\n"
            "\r\n"
            "mtllib mat.mtl\r\n"
            "o object\r\n"
            "g group\r\n"
            "s off\r\n"
            "vp 0.5 0.5\r\n"
            "v 0 0 0\r\n"
            "v 1 0 0     # inline comment\r\n"
            "v 0 1 0\r\n"
            "usemtl mat\r\n"
            "f 1 2 \\\r\n"
            "    3\r\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(r.clean() && r.hardViolations == 0, "noise-only fixture with continuation is clean",
              msgBlob(r));
        check(r.lines == 13 && r.logicalLines == 12, "13 physical lines fold into 12 logical",
              "lines=" + std::to_string(r.lines) + " logical=" + std::to_string(r.logicalLines));
        check(r.faces == 1 && r.checkedRefs == 3 && r.numV == 3,
              "the continued face still yields exactly one face with 3 refs",
              "faces=" + std::to_string(r.faces) + " checked=" + std::to_string(r.checkedRefs)
              + " numV=" + std::to_string(r.numV));
        check(r.numVT == 0 && r.numVN == 0, "mtllib/o/g/s/vp/usemtl do not enter the index spaces",
              "vt=" + std::to_string(r.numVT) + " vn=" + std::to_string(r.numVN));
        check(r.bytes == fileSize(p), "CRLF bytes are all counted",
              "bytes=" + std::to_string(r.bytes) + " file=" + std::to_string(fileSize(p)));
    }

    // 10) 非整数 token："3abc" 是缺陷，不是"3"。
    {
        const std::string p = write("not_integer.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 a 3\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(firstWhy(r) == "not-integer" && r.hardViolations == 1 && r.badFaces == 1,
              "a non-integer element is a hard violation", msgBlob(r));
        check(r.checkedRefs == 2 && !r.violations.empty() && r.violations.front().token == "a",
              "the bad token is the reported evidence",
              "checked=" + std::to_string(r.checkedRefs)
              + " token=" + (r.violations.empty() ? std::string() : r.violations.front().token));
    }

    // 11) 缺顶点索引（`f /2 ...`）：连 v 槽位都是空的。
    {
        const std::string p = write("index_missing.obj",
            "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 1\nf /2 2 3\n");
        const ObjPrecheckResult r = precheckObjFile(p);
        check(firstWhy(r) == "index-missing" && r.hardViolations == 1 && r.badFaces == 1,
              "a leading slash with no vertex index is reported", msgBlob(r));
        check(r.checkedRefs == 3 && !r.violations.empty() && r.violations.front().token == "/2",
              "the whole element token is kept for reading",
              "checked=" + std::to_string(r.checkedRefs));
    }

    // 12) 不可读：绝不伪装成"扫过且干净"。
    {
        const ObjPrecheckResult r = precheckObjFile(dir + "/does_not_exist.obj");
        check(r.ran && !r.readable, "a missing file reports ran-but-unreadable");
        check(!r.clean(), "unreadable is never reported as clean");
        check(r.bytes == 0 && r.lines == 0 && r.hardViolations == 0,
              "unreadable carries no invented counts");
        const std::vector<std::string> m = r.messages();
        check(m.size() == 1 && m[0].find("could not be opened") != std::string::npos
                  && m[0].find("unknown") != std::string::npos,
              "the message says coverage is unknown, not zero",
              m.empty() ? std::string("no message") : m[0]);
    }

    // 13) A-59：非 ASCII 目录下的 OBJ 必须能读到，且字节数与文件大小一致。
    //     这条同时是给 Utils::utf8ToPath 上的锁：改回窄字符 path 就必然变成
    //     readable=false（"文件不存在"），而不是静默通过。
    {
        const std::string cjkDir = dir + "/文章中的附件";
        fs::create_directories(Utils::utf8ToPath(cjkDir), ec);
        const std::string p = cjkDir + "/中文资产.obj";
        {
            std::ofstream f(Utils::utf8ToPath(p), std::ios::binary | std::ios::trunc);
            f << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
            f.close();
        }
        check(fs::exists(Utils::utf8ToPath(p), ec), "CJK fixture was actually written", p);
        const ObjPrecheckResult r = precheckObjFile(p);
        check(r.ran && r.readable, "A-59: UTF-8 path reaches the precheck scanner", p);
        check(r.clean() && r.faces == 1 && r.checkedRefs == 3,
              "CJK-path fixture parsed identically", msgBlob(r));
        check(r.bytes == fileSize(p) && r.bytes > 0, "byte count survives the CJK path",
              "bytes=" + std::to_string(r.bytes) + " file=" + std::to_string(fileSize(p)));
    }

    // 14) 记账：预检结果必须由 CLI 一路走进 conversion-report.json 的
    //     stages/counters/warnings，否则"warn 模式放行"等于什么都没记（手册 4.1）。
    {
        auto counterOf = [](const ProductionPipeline& pl, const char* name) {
            for (const StageCount& c : pl.report().counters)
            {
                if (c.name == name) { return c.value; }
            }
            return std::uint64_t(0);
        };
        auto stageOf = [](const ProductionPipeline& pl, const char* name) {
            for (const StageTiming& s : pl.report().stages)
            {
                if (s.name == name) { return true; }
            }
            return false;
        };
        auto warningWith = [](const ProductionPipeline& pl, const std::string& needle) {
            for (const std::string& w : pl.warnings())
            {
                if (w.find(needle) != std::string::npos) { return true; }
            }
            return false;
        };
        auto runWith = [&](const std::string& tag, const ObjPrecheckResult& pre)
            -> std::unique_ptr<ProductionPipeline> {
            ProductionProfile profile;
            std::unique_ptr<ProductionPipeline> pipeline(new ProductionPipeline(profile));
            ProductionPipeline::Options opt;
            opt.outputDir = dir + "/report_" + tag;
            opt.sourceFile = beyondTotal;
            opt.objPrecheck = pre;
            (void)pipeline->run(makeGrid(2, 2, 1.0).get(), opt);
            return pipeline;
        };
        Utils::registerFileAliases();

        // 14a) 脏结果：counter/stage/warning 三处都要出现。
        {
            const auto dirty = runWith("dirty", precheckObjFile(beyondTotal));
            check(stageOf(*dirty, "obj_precheck"), "obj_precheck stage is reported");
            check(counterOf(*dirty, "objPrecheckViolations") == 1,
                  "violation counter reaches the report",
                  std::to_string(counterOf(*dirty, "objPrecheckViolations")));
            check(counterOf(*dirty, "objPrecheckBadFaces") == 1
                      && counterOf(*dirty, "objPrecheckFaces") == 1
                      && counterOf(*dirty, "objPrecheckCheckedRefs") == 3
                      && counterOf(*dirty, "objPrecheckForwardRefs") == 0,
                  "face/ref counters reach the report",
                  "bad=" + std::to_string(counterOf(*dirty, "objPrecheckBadFaces"))
                  + " faces=" + std::to_string(counterOf(*dirty, "objPrecheckFaces"))
                  + " refs=" + std::to_string(counterOf(*dirty, "objPrecheckCheckedRefs")));
            check(warningWith(*dirty, "obj precheck: 1 invalid face reference"),
                  "the summary warning names the count", joinMessages(dirty->warnings()));
            check(warningWith(*dirty, "beyond-total"),
                  "the per-violation detail is kept in warnings",
                  joinMessages(dirty->warnings()));
        }

        // 14b) 干净结果：照样要有 counters（"0" 必须看起来是量出来的，不是没跑）。
        {
            const auto clean = runWith("clean", precheckObjFile(cleanForms));
            check(stageOf(*clean, "obj_precheck"), "clean scan is still reported");
            check(counterOf(*clean, "objPrecheckFaces") == 4
                      && counterOf(*clean, "objPrecheckCheckedRefs") == 24
                      && counterOf(*clean, "objPrecheckViolations") == 0,
                  "clean scan reports non-zero work with zero violations",
                  "faces=" + std::to_string(counterOf(*clean, "objPrecheckFaces"))
                  + " refs=" + std::to_string(counterOf(*clean, "objPrecheckCheckedRefs")));
            check(!warningWith(*clean, "obj precheck:"),
                  "a clean scan produces no precheck warning", joinMessages(clean->warnings()));
        }

        // 14c) 没跑（非 OBJ 输入 / -objcheck off）：不许出现任何 obj_precheck 痕迹。
        {
            const auto skipped = runWith("skipped", ObjPrecheckResult());
            check(!stageOf(*skipped, "obj_precheck"), "no obj_precheck stage when it did not run");
            check(counterOf(*skipped, "objPrecheckFaces") == 0,
                  "no objPrecheck counters when it did not run");
            check(!warningWith(*skipped, "obj precheck"),
                  "no precheck warning when it did not run");
        }

        // 14d) 不可读：warning 要说"覆盖面未知"，而不是"没有违规"。
        {
            const auto unread = runWith("unread", precheckObjFile(dir + "/gone.obj"));
            check(stageOf(*unread, "obj_precheck"), "an unreadable scan is still reported");
            check(warningWith(*unread, "could not be opened") && warningWith(*unread, "unknown"),
                  "unreadable is reported as unknown coverage",
                  joinMessages(unread->warnings()));
        }
    }
}

// ---------------------------------------------------------------------------
// 暂存目录卫生（本节新增）：强杀残留的暂存目录必须能被下次运行识别并清掉；
// 已存在的旧数据集在新版本就位前不能被破坏（失败时回滚）。
// 这两条都只在"异常/重复运行"时才会踩到，平时跑不出来，所以必须显式构造。
// ---------------------------------------------------------------------------
void testStagingHygiene()
{
    std::cout << "[staging] stale staging sweep + previous-dataset protection" << std::endl;
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wb_staging_hygiene";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    Utils::registerFileAliases();   // 没有它，b3dm 写出插件不注册，管线写盘必失败

    // 1) 造一个"属主已死"的旧暂存目录（owner.pid = 一个不可能存在的 pid）
    const fs::path stale = root / ".wb-staging-4294967294";
    fs::create_directories(stale / "tiles", ec);
    {
        std::ofstream f(stale / "owner.pid");
        f << "4294967294";
        std::ofstream t(stale / "tiles" / "orphan.b3dm", std::ios::binary);
        t << "stale tile that must never reach the published dataset";
    }

    // 2) 造一个上一版数据集（tileset.json + tiles/）
    fs::create_directories(root / "tiles", ec);
    {
        std::ofstream f(root / "tileset.json"); f << "{\"asset\":{\"version\":\"1.0\"},\"old\":true}";
        std::ofstream t(root / "tiles" / "old.b3dm", std::ios::binary); t << "old";
    }

    // 3) 跑一次真实管线（小场景）
    ProductionProfile profile;
    ProductionPipeline pipeline(profile);
    ProductionPipeline::Options opt;
    opt.outputDir = root.u8string();
    const bool ok = pipeline.run(makeGrid(2, 2, 1.0).get(), opt);
    check(ok, "pipeline runs on an output dir that already holds a dataset");
    // 提交（搬盘）发生在 finalize()，不调它根本走不到"替换旧数据集"这段
    const bool committed = pipeline.finalize();
    check(committed, "finalize commits into an output dir that already holds a dataset",
          joinMessages(pipeline.failures()));

    check(!fs::exists(stale, ec), "a staging dir whose owner process is gone is swept",
          stale.u8string());
    check(!fs::exists(root / ".wb-previous", ec),
          "the .wb-previous backup is removed after a successful commit",
          (root / ".wb-previous").u8string());
    // 旧 tiles/ 里的 old.b3dm 不该混进新数据集（新内容整体替换 tiles/）
    {
        std::string listing;
        for (const auto& e : fs::directory_iterator(root / "tiles", ec))
        {
            listing += e.path().filename().string() + ",";
        }
        check(!fs::exists(root / "tiles" / "old.b3dm", ec),
              "the previous tiles/ is replaced, not merged", "tiles/ = " + listing);
    }
    check(fs::exists(root / "tileset.json", ec), "a new tileset.json is in place");

    // 4) 收尾：commit 之后再跑一次（重复执行）必须同样成功
    ProductionPipeline again(profile);
    ProductionPipeline::Options opt2;
    opt2.outputDir = root.u8string();
    check(again.run(makeGrid(2, 2, 1.2).get(), opt2), "re-running into the same dir still works");
    check(!fs::exists(root / ".wb-previous", ec), "no backup left behind after the second commit");

    fs::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// UTF-8 路径的**失败清理**（A-59 遗留项）：CLI 的 return 3 分支会调
// discardStaging()（remove_all）和空纹理目录的 remove()。这两处只在失败时走，
// 成功路径的验证覆盖不到。这里用 CJK 目录做同等动作：造一个暂存目录 + 空纹理
// 目录，跑一次注定失败（无几何）的管线，再按 CLI 的顺序清理，断言都没了。
// 口径：这是**单元级**证据（同一个函数、同一个 Utils::utf8ToPath 入口），
// 不是 "CLI 端到端强行失败" 的证据。
// ---------------------------------------------------------------------------
void testCjkFailureCleanup()
{
    std::cout << "[cjkCleanup] UTF-8 staging cleanup on the failure path" << std::endl;
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / fs::u8path(u8"测试_失败清理_中文目录");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / fs::u8path(u8"暂存") / fs::u8path(u8"瓦片"), ec);
    fs::create_directories(root / fs::u8path(u8"纹理"), ec);
    {
        std::ofstream f(root / fs::u8path(u8"暂存") / fs::u8path(u8"瓦片") / "t0.b3dm", std::ios::binary);
        f << "not a real tile";
    }
    check(fs::exists(root / fs::u8path(u8"暂存")), "CJK staging tree was created",
          root.u8string());

    // 空几何 -> run() 失败（与 CLI 里 pipeline.run() 返回 false 同一分支）
    osg::ref_ptr<osg::Group> empty = new osg::Group;
    ProductionProfile profile;
    ProductionPipeline pipeline(profile);
    ProductionPipeline::Options opt;
    opt.outputDir = root.u8string();
    opt.stagingDir = (root / fs::u8path(u8"暂存")).u8string();
    const bool ok = pipeline.run(empty.get(), opt);
    check(!ok, "a scene without geometry fails the pipeline (positive control)");

    pipeline.discardStaging();
    check(!fs::exists(Utils::utf8ToPath(opt.stagingDir), ec),
          "CJK staging dir is gone after discardStaging()",
          opt.stagingDir + " ec=" + ec.message());
    check(!fs::exists(Utils::utf8ToPath(opt.stagingDir + "/瓦片/t0.b3dm"), ec),
          "the b3dm written into the CJK staging dir was removed too");

    // CLI 的第二个清理点：空纹理目录
    const std::string texDir = (root / fs::u8path(u8"纹理")).u8string();
    check(fs::is_directory(Utils::utf8ToPath(texDir), ec), "CJK texture dir exists before cleanup");
    if (fs::is_directory(Utils::utf8ToPath(texDir), ec)
        && fs::directory_iterator(Utils::utf8ToPath(texDir), ec) == fs::directory_iterator())
    {
        fs::remove(Utils::utf8ToPath(texDir), ec);
    }
    check(!fs::exists(Utils::utf8ToPath(texDir), ec),
          "empty CJK texture dir is removed by the failure-path cleanup",
          texDir + " ec=" + ec.message());

    fs::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// 坐标有效性（手册 §4.1/§4.2）：世界变换后的 double 有限不等于 float 有限。
// 1e38f 顶点 × 缩放 10 = 1e39，double 侧 isfinite 为真，转 float 就是 inf；
// 旧代码只查了第一级，那个 inf 会一路进 aabb / 质心 / 切分器 / BVH / POSITION。
// 语义要求：整条 drawable 丢弃 + 逐条记账 + 进报告，不裁剪、不搬移、不伪造。
// ---------------------------------------------------------------------------
void testCoordinateValidity()
{
    std::cout << "[coordValidity] float overflow after world transform must be caught" << std::endl;

    auto triGeom = [](const osg::Vec3& a, const osg::Vec3& b, const osg::Vec3& c) {
        osg::ref_ptr<osg::Geometry> g = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
        pos->push_back(a); pos->push_back(b); pos->push_back(c);
        g->setVertexArray(pos.get());
        g->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3));
        return g;
    };
    auto sceneOf = [](std::initializer_list<std::pair<const char*, osg::Geometry*>> items) {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        for (const auto& it : items)
        {
            osg::ref_ptr<osg::Geode> geode = new osg::Geode;
            geode->setName(it.first);
            geode->addDrawable(it.second);
            root->addChild(geode.get());
        }
        return root;
    };

    // 1) double 有限、float 溢出：必须丢弃并记成 floatOverflows
    {
        osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
        mt->setMatrix(osg::Matrix::scale(10.0, 10.0, 10.0));
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->setName("overflow");
        geode->addDrawable(triGeom(osg::Vec3(1.0e38f, 0.0f, 0.0f),
                                   osg::Vec3(0.0f, 1.0f, 0.0f),
                                   osg::Vec3(1.0f, 0.0f, 0.0f)).get());
        mt->addChild(geode.get());

        SourceModel m;
        check(!m.build(mt.get()), "a scene whose only geometry overflows reports no geometry");
        check(m.nonFiniteGeometry.size() == 1, "the overflowing drawable is recorded",
              "records=" + std::to_string(m.nonFiniteGeometry.size()));
        if (!m.nonFiniteGeometry.empty())
        {
            const NonFiniteGeometry& g = m.nonFiniteGeometry[0];
            check(g.floatOverflows == 1 && g.nonFiniteDoubles == 0,
                  "counted as float overflow, not as a non-finite double",
                  "ff=" + std::to_string(g.floatOverflows)
                  + " nf=" + std::to_string(g.nonFiniteDoubles));
            check(g.droppedTriangles == 1 && g.vertexCount == 3,
                  "the lost triangles are accounted for",
                  "tris=" + std::to_string(g.droppedTriangles));
        }
        check(m.totalTriangles() == 0, "no triangle survives from the dropped drawable",
              "tris=" + std::to_string(m.totalTriangles()));
    }

    // 2) 源数据里就有 NaN：记成 nonFiniteDoubles（与溢出分开统计）
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        SourceModel m;
        check(!m.build(sceneOf({{"nan_geom", triGeom(osg::Vec3(nan, 0.0f, 0.0f),
                                                    osg::Vec3(0.0f, 1.0f, 0.0f),
                                                    osg::Vec3(1.0f, 0.0f, 0.0f)).get()}}).get()),
              "a scene whose only drawable is NaN reports no geometry");
        check(m.nonFiniteGeometry.size() == 1 && m.nonFiniteGeometry[0].nonFiniteDoubles == 1,
              "NaN is recorded as a non-finite double",
              m.nonFiniteGeometry.empty() ? "no record"
                  : "nf=" + std::to_string(m.nonFiniteGeometry[0].nonFiniteDoubles));
    }

    // 3) 合法的大坐标（投影坐标系级别的 1e7）不得被当成异常丢掉
    {
        SourceModel m;
        check(m.build(sceneOf({{"georef", triGeom(osg::Vec3(4540000.0f, 0.0f, 0.0f),
                                                  osg::Vec3(4540001.0f, 0.0f, 0.0f),
                                                  osg::Vec3(4540000.0f, 1.0f, 0.0f)).get()}}).get()),
              "large but legal coordinates build");
        check(m.nonFiniteGeometry.empty(), "legal large coordinates are not flagged",
              "records=" + std::to_string(m.nonFiniteGeometry.size()));
        check(m.totalTriangles() == 1, "legal large-coordinate triangle is kept",
              "tris=" + std::to_string(m.totalTriangles()));
        checkNear(m.bounds.xMin(), 4540000.0, 0.5, "large coordinate lands in bounds");
    }

    // 4) 边界：接近 float 上限但可表示的坐标必须留下（不能"看着大就丢"）
    {
        const float nearMax = 3.0e38f;
        SourceModel m;
        check(m.build(sceneOf({{"near_max", triGeom(osg::Vec3(nearMax, 0.0f, 0.0f),
                                                    osg::Vec3(nearMax, 1.0f, 0.0f),
                                                    osg::Vec3(nearMax, 0.0f, 1.0f)).get()}}).get()),
              "coordinates near FLT_MAX are representable and build");
        check(m.nonFiniteGeometry.empty(), "near-FLT_MAX is not flagged",
              "records=" + std::to_string(m.nonFiniteGeometry.size()));
        check(m.totalTriangles() == 1, "near-FLT_MAX triangle is kept",
              "tris=" + std::to_string(m.totalTriangles()));
    }

    // 5) 正负无穷分别计数（与 NaN 一样记成 non-finite double）
    {
        const float pinf = std::numeric_limits<float>::infinity();
        SourceModel m;
        (void)m.build(sceneOf({{"pinf", triGeom(osg::Vec3(pinf, 0.0f, 0.0f),
                                                osg::Vec3(1.0f, 0.0f, 0.0f),
                                                osg::Vec3(0.0f, 1.0f, 0.0f)).get()},
                               {"ninf", triGeom(osg::Vec3(-pinf, 0.0f, 0.0f),
                                                osg::Vec3(1.0f, 0.0f, 0.0f),
                                                osg::Vec3(0.0f, 1.0f, 0.0f)).get()}}).get());
        check(m.nonFiniteGeometry.size() == 2, "both +inf and -inf drawables are recorded",
              "records=" + std::to_string(m.nonFiniteGeometry.size()));
        std::uint64_t nf = 0;
        for (const NonFiniteGeometry& g : m.nonFiniteGeometry) { nf += g.nonFiniteDoubles; }
        check(nf == 2, "each infinite vertex is counted once", "nf=" + std::to_string(nf));
    }

    // 6) 同一个 SourceModel 连续 build 两次：记账不得累加（A-64 的回归）
    {
        SourceModel m;
        const osg::ref_ptr<osg::Group> scene = sceneOf({
            {"good", triGeom(osg::Vec3(0, 0, 0), osg::Vec3(2, 0, 0), osg::Vec3(0, 2, 0)).get()},
            {"nan", triGeom(osg::Vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0),
                            osg::Vec3(0, 1, 0), osg::Vec3(1, 0, 0)).get()},
        });
        (void)m.build(scene.get());
        const std::size_t first = m.nonFiniteGeometry.size();
        (void)m.build(scene.get());
        check(first == 1 && m.nonFiniteGeometry.size() == 1,
              "rebuilding the same model does not accumulate records",
              "first=" + std::to_string(first) + " second="
              + std::to_string(m.nonFiniteGeometry.size()));
    }

    // 7) 坏 drawable 与好 drawable 并存：好的必须完整保留，包围盒只反映好几何
    {
        SourceModel m;
        const bool built = m.build(sceneOf({
            {"good", triGeom(osg::Vec3(0, 0, 0), osg::Vec3(2, 0, 0), osg::Vec3(0, 2, 0)).get()},
            {"bad", triGeom(osg::Vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0),
                            osg::Vec3(0, 1, 0), osg::Vec3(1, 0, 0)).get()},
        }).get());
        check(built, "a mixed scene still builds from its good geometry");
        check(m.totalTriangles() == 1, "exactly the good triangle survives",
              "tris=" + std::to_string(m.totalTriangles()));
        check(m.featureCount() == 1, "the dropped drawable creates no feature",
              "features=" + std::to_string(m.featureCount()));
        check(std::isfinite(m.bounds.xMin()) && std::isfinite(m.bounds.xMax())
              && std::isfinite(m.bounds.yMin()) && std::isfinite(m.bounds.yMax()),
              "bounds stay finite after the drop");
        check(m.nonFiniteGeometry.size() == 1 && m.nonFiniteGeometry[0].droppedTriangles == 1,
              "the dropped triangle is accounted for");
    }

    // 8) 报告链路：丢弃必须出现在 counters + warnings 里（不能只在 OSG_WARN 里）
    {
        const std::string dir = "test_out_coord_validity";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        auto counterOf = [](const ProductionPipeline& pl, const char* name) {
            for (const StageCount& c : pl.report().counters)
            {
                if (c.name == name) { return c.value; }
            }
            return std::uint64_t(0);
        };
        auto warningWith = [](const ProductionPipeline& pl, const std::string& needle) {
            for (const std::string& w : pl.warnings())
            {
                if (w.find(needle) != std::string::npos) { return true; }
            }
            return false;
        };

        osg::ref_ptr<osg::Group> root = sceneOf({
            {"keep_me", triGeom(osg::Vec3(0.0f, 0.0f, 0.0f), osg::Vec3(2.0f, 0.0f, 0.0f),
                                osg::Vec3(0.0f, 2.0f, 0.0f)).get()},
            {"drop_me", triGeom(osg::Vec3(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f),
                                osg::Vec3(0.0f, 1.0f, 0.0f), osg::Vec3(1.0f, 0.0f, 0.0f)).get()},
        });
        ProductionProfile profile;
        ProductionPipeline pipeline(profile);
        ProductionPipeline::Options opt;
        opt.outputDir = dir + "/out";
        const bool ok = pipeline.run(root.get(), opt);
        // 失败关闭：丢掉一条 drawable 后无法判断"剩下的够不够"，因此拒绝发布，
        // 而不是发个 warning 就把缺件的产物交出去（本轮语义变更）。
        check(!ok, "a scene with a dropped drawable fails the pipeline (fail closed)");
        {
            bool named = false;
            for (const std::string& f : pipeline.failures())
            {
                if (f.find("refusing to publish") != std::string::npos) { named = true; }
            }
            check(named, "the failure says it refuses to publish a partial dataset",
                  joinMessages(pipeline.failures()));
        }
        check(counterOf(pipeline, "nonFiniteDrawables") == 1,
              "nonFiniteDrawables reaches the report",
              std::to_string(counterOf(pipeline, "nonFiniteDrawables")));
        check(counterOf(pipeline, "nonFiniteDroppedTriangles") == 1,
              "dropped triangle count reaches the report",
              std::to_string(counterOf(pipeline, "nonFiniteDroppedTriangles")));
        check(counterOf(pipeline, "nonFiniteVertexCount") == 1,
              "non-finite vertex count reaches the report",
              std::to_string(counterOf(pipeline, "nonFiniteVertexCount")));
        check(warningWith(pipeline, "coordinate validity: dropped"),
              "the drop is named in warnings", joinMessages(pipeline.warnings()));
        check(warningWith(pipeline, "nothing was clamped, moved or rebuilt"),
              "the warning states that geometry was not silently repaired",
              joinMessages(pipeline.warnings()));
    }
}

// A-43：源法线数组里的零长度向量，写出侧必须重算成面法线，否则官方 validator
// 对整个内容文件判 severity=ERROR（lugouqiao_v17 实测 26 个文件 / 195 条）。
void testZeroNormalRepair()
{
    std::cout << "[nrmRepair] A-43 zero-length NORMAL rebuild" << std::endl;
    auto zeroNormals = [](unsigned n) {
        osg::ref_ptr<osg::Vec3Array> a = new osg::Vec3Array;
        for (unsigned i = 0; i < n; ++i) { a->push_back(osg::Vec3(0.0f, 0.0f, 0.0f)); }
        return a;
    };

    // 1) 入射非退化面：三个顶点的法线全零 -> 该面法线 +Z
    {
        osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
        pos->push_back(osg::Vec3(0, 0, 0));
        pos->push_back(osg::Vec3(1, 0, 0));
        pos->push_back(osg::Vec3(0, 1, 0));
        osg::ref_ptr<osg::Vec3Array> nrm = zeroNormals(3);
        std::vector<unsigned> tris = {0, 1, 2};
        const auto rc = SourceModel::repairZeroNormals(pos.get(), nrm.get(), tris);
        check(rc.fromFaces == 3 && rc.fromPrimitive == 0 && rc.leftZero == 0,
              "face-normal repair counts", "fromFaces=" + std::to_string(rc.fromFaces)
              + " fromPrimitive=" + std::to_string(rc.fromPrimitive)
              + " leftZero=" + std::to_string(rc.leftZero));
        for (unsigned v = 0; v < 3; ++v)
        {
            checkNear((*nrm)[v].length(), 1.0, 1e-5, "repaired normal is unit length v" + std::to_string(v));
            checkNear((*nrm)[v].z(), 1.0, 1e-5, "repaired normal matches face normal v" + std::to_string(v));
        }
    }

    // 2) 顶点只落在退化面上 -> 退到本图元平均面法线；已有法线一个字节都不许动
    {
        osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
        pos->push_back(osg::Vec3(0, 0, 0));
        pos->push_back(osg::Vec3(2, 0, 0));
        pos->push_back(osg::Vec3(0, 2, 0));
        pos->push_back(osg::Vec3(0, 2, 0));           // 与 v2 重合：(0,2,3) 面积为 0
        osg::ref_ptr<osg::Vec3Array> nrm = new osg::Vec3Array;
        nrm->push_back(osg::Vec3(0, 0, 1));
        nrm->push_back(osg::Vec3(0.25f, 0.0f, 1.0f));  // 非单位但非零：写出器另有 normalize，这里不该碰
        nrm->push_back(osg::Vec3(0, 0, 1));
        nrm->push_back(osg::Vec3(0, 0, 0));
        const osg::Vec3 keepBefore = (*nrm)[1];
        std::vector<unsigned> tris = {0, 1, 2, 0, 2, 3};
        const auto rc = SourceModel::repairZeroNormals(pos.get(), nrm.get(), tris);
        check(rc.fromFaces == 0 && rc.fromPrimitive == 1 && rc.leftZero == 0,
              "degenerate-only vertex falls back to primitive mean",
              "fromFaces=" + std::to_string(rc.fromFaces)
              + " fromPrimitive=" + std::to_string(rc.fromPrimitive)
              + " leftZero=" + std::to_string(rc.leftZero));
        checkNear((*nrm)[3].length(), 1.0, 1e-5, "fallback normal is unit length");
        checkNear((*nrm)[3].z(), 1.0, 1e-5, "fallback normal follows the good face");
        check((*nrm)[1] == keepBefore, "usable non-unit normal left untouched");
    }

    // 3) 全退化：宁可留零让 validator 判错，也不编造法线
    {
        osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
        for (int i = 0; i < 3; ++i) { pos->push_back(osg::Vec3(1, 1, 1)); }
        osg::ref_ptr<osg::Vec3Array> nrm = zeroNormals(3);
        std::vector<unsigned> tris = {0, 1, 2};
        const auto rc = SourceModel::repairZeroNormals(pos.get(), nrm.get(), tris);
        check(rc.fromFaces == 0 && rc.fromPrimitive == 0 && rc.leftZero == 3,
              "all-degenerate primitive fabricates nothing",
              "fromFaces=" + std::to_string(rc.fromFaces)
              + " fromPrimitive=" + std::to_string(rc.fromPrimitive)
              + " leftZero=" + std::to_string(rc.leftZero));
        check((*nrm)[0].length() == 0.0f, "left-zero stays zero");
    }

    // 4) 接线自检：materialize 出口的法线必须全部可用（前 3 条只测纯函数，
    //    证明不了调用点存在）
    {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->setName("zeronorm");
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
        pos->push_back(osg::Vec3(0, 0, 0));
        pos->push_back(osg::Vec3(1, 0, 0));
        pos->push_back(osg::Vec3(0, 1, 0));
        geom->setVertexArray(pos.get());
        osg::ref_ptr<osg::Vec3Array> nrm = zeroNormals(3);
        geom->setNormalArray(nrm.get(), osg::Array::BIND_PER_VERTEX);
        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3));
        geode->addDrawable(geom.get());
        root->addChild(geode.get());

        SourceModel m;
        check(m.build(root.get()), "source model built from zero-normal geometry");
        osg::ref_ptr<osg::Node> out = m.materialize(m.prims, {0}, true);
        check(out.valid(), "materialize returned a node");
        int drawables = 0, checked = 0;
        bool allUsable = true;
        std::function<void(osg::Node*)> walk = [&](osg::Node* n) {
            if (!n) { return; }
            if (osg::Geode* gd = n->asGeode())
            {
                for (unsigned i = 0; i < gd->getNumDrawables(); ++i)
                {
                    osg::Geometry* g = gd->getDrawable(i)->asGeometry();
                    if (!g || !g->getNormalArray()) { continue; }
                    ++drawables;
                    const osg::Vec3Array* a = dynamic_cast<const osg::Vec3Array*>(g->getNormalArray());
                    if (!a) { continue; }
                    for (const osg::Vec3& v : *a)
                    {
                        ++checked;
                        if (std::abs(v.length() - 1.0f) > 1e-5f) { allUsable = false; }
                    }
                }
            }
            if (osg::Group* grp = n->asGroup())
            {
                for (unsigned c = 0; c < grp->getNumChildren(); ++c) { walk(grp->getChild(c)); }
            }
        };
        walk(out.get());
        check(drawables == 1 && checked == 3, "materialized geometry carries a per-vertex normal array",
              "drawables=" + std::to_string(drawables) + " normals=" + std::to_string(checked));
        check(allUsable, "materialize hook repairs zero normals before they reach the writer");
    }
}

void testPartitionCoverageAndDeterminism()
{
    std::cout << "[partition] coverage + determinism" << std::endl;
    ProductionProfile profile;
    profile.budget.targetTrianglesPerTile = 200;
    profile.budget.maxTrianglesPerTile = 400;
    profile.budget.maxPrimitivesPerTile = 8;
    profile.budget.targetPrimitivesPerTile = 4;

    osg::ref_ptr<osg::Node> grid = makeGrid(8, 8, 10.0, 4);   // 64 对象 * 8 三角形
    SourceModel m;
    check(m.build(grid.get()), "grid build");

    std::vector<PrimRef> t1 = m.prims, t2 = m.prims;
    SpatialPartitioner p1(m, profile, t1);
    SpatialPartitioner p2(m, profile, t2);
    const int r1 = p1.build();
    const int r2 = p2.build();
    check(r1 >= 0 && r2 >= 0, "build returns root");
    check(p1.nodes.size() == p2.nodes.size(), "same node count on identical input",
          std::to_string(p1.nodes.size()) + " vs " + std::to_string(p2.nodes.size()));
    bool sameShape = true;
    for (std::size_t i = 0; i < p1.nodes.size() && i < p2.nodes.size(); ++i)
    {
        if (p1.nodes[i].items != p2.nodes[i].items) { sameShape = false; break; }
        if (p1.nodes[i].children.size() != p2.nodes[i].children.size()) { sameShape = false; break; }
    }
    check(sameShape, "identical tree shape (determinism)");
    check(p1.verifyCoverage(), "coverage check passes");
    check(p1.failures.empty(), "no partition failures",
          p1.failures.empty() ? std::string() : p1.failures.front());

    // 硬预算：叶节点不得超
    bool overLeaf = false;
    for (const PartitionNode& n : p1.nodes)
    {
        if (n.children.empty() && (n.triangles > profile.budget.maxTrianglesPerTile
                                   || n.primitiveCount > profile.budget.maxPrimitivesPerTile))
        {
            overLeaf = true;
        }
    }
    check(!overLeaf, "no leaf exceeds hard budget");
}

void testGiantSingleMeshSplits()
{
    std::cout << "[partition] giant single mesh must split" << std::endl;
    // 一个对象、一个 primitiveSet、20000 三角形：只能靠三角形级切分
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->setName("giant");
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
    osg::ref_ptr<osg::DrawElementsUInt> idx =
        new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
    const int N = 100;
    for (int y = 0; y < N; ++y)
    {
        for (int x = 0; x < N; ++x)
        {
            pos->push_back(osg::Vec3(x, y, 0));
            pos->push_back(osg::Vec3(x + 1, y, 0));
            pos->push_back(osg::Vec3(x + 1, y + 1, 0));
            pos->push_back(osg::Vec3(x, y + 1, 0));
            const unsigned b = static_cast<unsigned>(pos->size()) - 4;
            idx->push_back(b); idx->push_back(b + 1); idx->push_back(b + 2);
            idx->push_back(b); idx->push_back(b + 2); idx->push_back(b + 3);
        }
    }
    geom->setVertexArray(pos.get());
    geom->addPrimitiveSet(idx.get());
    geode->addDrawable(geom.get());

    SourceModel m;
    check(m.build(geode.get()), "giant build");
    check(m.prims.size() == 1, "single source unit", std::to_string(m.prims.size()));

    ProductionProfile profile;
    profile.budget.targetTrianglesPerTile = 2000;
    profile.budget.maxTrianglesPerTile = 4000;
    std::vector<PrimRef> table = m.prims;
    SpatialPartitioner sp(m, profile, table);
    const int root = sp.build();
    check(root >= 0, "partition root");
    const bool cov = sp.verifyCoverage();
    for (const std::string& w : sp.warnings) { std::cout << "    warn: " << w << std::endl; }
    for (const std::string& f : sp.failures) { std::cout << "    fail: " << f << std::endl; }
    check(cov, "giant coverage ok");
    check(table.size() > m.prims.size(), "triangle-level split appended new units",
          std::to_string(table.size()) + " > " + std::to_string(m.prims.size()));

    // 新单元仍然能被物化出几何（历史 bug：新 id 在 SourceModel::prims 里越界）
    std::vector<std::uint32_t> items;
    sp.subtreeItems(root, items);
    osg::ref_ptr<osg::Node> mat = m.materialize(table, items, true);
    check(mat.valid(), "materialize with split table");
    struct Cnt : public osg::NodeVisitor {
        Cnt() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
        void apply(osg::Geode& g) override { n += g.getNumDrawables(); traverse(g); }
        unsigned n = 0;
    } c;
    if (mat.valid()) { mat->accept(c); }
    check(c.n > 0, "materialized geometry is non-empty", "drawables=" + std::to_string(c.n));
}

void testErrorEvaluatorAnalytic()
{
    std::cout << "[error] analytic cases" << std::endl;
    ErrorEvalConfig cfg;
    GeometryErrorEvaluator ev(cfg);

    // 1) 同一张面：真值 0。求值器给的是"采样下界 + 保守半径上界"的区间，
    //    上界会被自适应细分压到容差量级（默认 1mm），所以这里断言下界为 0、
    //    上界不超过几个容差，而不是严格等于 0。
    TriangleSoup a, b;
    a.add(osg::Vec3d(0, 0, 0), osg::Vec3d(1, 0, 0), osg::Vec3d(0, 1, 0));
    b.add(osg::Vec3d(0, 0, 0), osg::Vec3d(1, 0, 0), osg::Vec3d(0, 1, 0));
    ErrorResult r0 = ev.evaluate(a, b);
    checkNear(r0.lowerBoundM, 0.0, 1e-9, "identical surfaces -> lower bound 0");
    check(r0.upperBoundM <= cfg.toleranceMeters * 5.0,
          "identical surfaces -> upper bound within tolerance",
          "upper=" + std::to_string(r0.upperBoundM));
    check(r0.upperBoundM >= 0.0, "identical surfaces -> non-negative");

    // 2) 平行面沿 Z 偏移 0.5m：双向 Hausdorff 应为 0.5m
    TriangleSoup src, prox;
    src.add(osg::Vec3d(0, 0, 0), osg::Vec3d(10, 0, 0), osg::Vec3d(0, 10, 0));
    prox.add(osg::Vec3d(0, 0, 0.5), osg::Vec3d(10, 0, 0.5), osg::Vec3d(0, 10, 0.5));
    ErrorResult r1 = ev.evaluate(src, prox);
    check(r1.lowerBoundM <= 0.5 + 1e-6, "lower bound <= true error",
          std::to_string(r1.lowerBoundM));
    check(r1.upperBoundM >= 0.5 - 1e-6, "upper bound >= true error",
          std::to_string(r1.upperBoundM));
    checkNear(r1.lowerBoundM, 0.5, 0.05, "lower bound converges to 0.5m");
    check(r1.upperBoundM < 0.5 + 1.0, "upper bound not wildly loose",
          std::to_string(r1.upperBoundM));

    // 3) 一侧为空：必须给出有限保守值，不能 NaN/Inf
    TriangleSoup empty;
    ErrorResult r2 = ev.evaluate(src, empty);
    check(std::isfinite(r2.geometricErrorM()), "empty proxy yields finite error");
    check(r2.geometricErrorM() > 0.0, "empty proxy yields non-zero error");
}

// BVH 查询必须等于暴力最近距离：建树会按质心中位数重排 order 数组，
// 叶子如果按"位置"而不是按 order 取三角形，查询命中的就是别的面的，
// 距离会随机偏大/偏小——偏小那一侧直接把 geometricError 写小（违规）。
void testMeshBVHMatchesBruteForce()
{
    std::cout << "[bvh] nearest distance equals brute force" << std::endl;

    TriangleSoup soup;
    for (int iy = 0; iy < 8; ++iy)
    {
        for (int ix = 0; ix < 8; ++ix)
        {
            const double x = ix * 5.0, y = iy * 5.0;
            soup.add(osg::Vec3d(x, y, 0.0), osg::Vec3d(x + 1.0, y, 0.0),
                     osg::Vec3d(x, y + 1.0, 0.0));
        }
    }
    MeshBVH bvh;
    bvh.build(soup);

    const osg::Vec3d probes[] = {
        osg::Vec3d(0.2, 0.2, 0.0),      // 落在第一个三角形内部
        osg::Vec3d(17.5, 12.3, 0.4),    // 网格中间的空隙
        osg::Vec3d(35.0, 35.0, 3.0),    // 右上角外侧
        osg::Vec3d(-4.0, 9.0, 0.0),     // 左侧远处
        osg::Vec3d(2.6, 27.1, -1.7),    // 任意斜上方
        osg::Vec3d(44.0, -6.0, 8.0)     // 全体外侧
    };
    for (const osg::Vec3d& p : probes)
    {
        // 基准：单三角形子树必然是叶子、order 恒等，逐三角形取最小即精确最近距离
        double brute = std::numeric_limits<double>::max();
        for (std::size_t t = 0; t < soup.size(); ++t)
        {
            TriangleSoup one;
            one.add(soup.v[t * 3], soup.v[t * 3 + 1], soup.v[t * 3 + 2]);
            MeshBVH b1;
            b1.build(one);
            brute = std::min(brute, b1.distance(p));
        }
        const double got = bvh.distance(p);
        checkNear(got, brute, 1e-12,
                  "bvh distance == brute force at (" + std::to_string(p.x()) + ", "
                  + std::to_string(p.y()) + ", " + std::to_string(p.z()) + ")");
    }
}

// L1 把 BVH 遍历从"固定左先"改成"近孩子优先"。顺序可以改，结果一位都不能变。
//
// 这里不去跑新旧两份实现对拍（旧实现已经不在了，而且新旧对拍要付一整轮转换的
// 代价），而是把查询结果钉死在基准上：每个三角形单独建一棵树必然是叶子、
// order 恒等，逐三角形取 min 就是全局最近距离。只要两种遍历都取到同一个全局
// 最小元素，它们必然等于同一个 double——所以断言用**逐位相等**，不用容差。
//
// 阳性/阴性对照在最后：判据必须能在 1 ulp 的偏差上翻脸，否则"逐位相等"是空话。
void testMeshBVHTraversalBitExact()
{
    std::cout << "[bvhOrder] traversal bit-exact vs per-triangle baseline" << std::endl;

    std::uint64_t seed = 0x9E3779B97F4A7C15ull;
    const auto rnd = [&seed]() {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>((seed >> 11) & 0xFFFFFull) / 1048575.0;  // [0,1]
    };
    const auto bitExact = [](double a, double b) { return a == b; };

    // 3 = 只有根叶子（没有内部节点），17/200/3000 = 会真正走到近/远孩子分支
    for (const std::size_t n : {std::size_t(3), std::size_t(17), std::size_t(200), std::size_t(3000)})
    {
        TriangleSoup soup;
        for (std::size_t t = 0; t < n; ++t)
        {
            const osg::Vec3d c(rnd() * 100.0, rnd() * 100.0, rnd() * 20.0);
            const osg::Vec3d e1(rnd() - 0.5, rnd() - 0.5, rnd() - 0.5);
            const osg::Vec3d e2(rnd() - 0.5, rnd() - 0.5, rnd() - 0.5);
            soup.add(c, c + e1, c + e2);
        }
        MeshBVH bvh;
        bvh.build(soup);
        check(bvh.triangleCount() == n, "bvhOrder triangle count n=" + std::to_string(n));

        // 基准树建一次就存下来：每个探针都重建 n 棵树会让这条用例跑到分钟级
        std::vector<MeshBVH> singles(n);
        for (std::size_t t = 0; t < n; ++t)
        {
            TriangleSoup s;
            s.add(soup.v[t * 3], soup.v[t * 3 + 1], soup.v[t * 3 + 2]);
            singles[t].build(s);
        }

        // 探针：既有mesh内部/外侧/远处，也有正好压在顶点上的退化位置
        std::vector<osg::Vec3d> probes;
        for (int i = 0; i < 64; ++i)
        {
            probes.emplace_back(rnd() * 140.0 - 20.0, rnd() * 140.0 - 20.0, rnd() * 40.0 - 10.0);
        }
        for (std::size_t t = 0; t < soup.size() && t < 4; ++t)
        {
            probes.push_back(soup.v[t * 3]);             // 恰在顶点上 ⇒ 距离 0
        }

        int exact = 0;
        for (const osg::Vec3d& p : probes)
        {
            double want = std::numeric_limits<double>::max();
            for (std::size_t t = 0; t < n; ++t) { want = std::min(want, singles[t].distance(p)); }
            const double got = bvh.distance(p);
            if (bitExact(got, want)) { ++exact; }
            else
            {
                std::ostringstream os;
                os << "n=" << n << " got=" << std::setprecision(17) << got
                   << " want=" << want << " diff=" << (got - want);
                check(false, "bvhOrder bit-exact query", os.str());
            }
        }
        check(exact == static_cast<int>(probes.size()),
              "bvhOrder all probes bit-exact n=" + std::to_string(n),
              "exact=" + std::to_string(exact) + "/" + std::to_string(probes.size()));

        // 对照：同一批数值里把基准抬高 1 ulp，判据必须拒绝；原值必须接受
        const double ref = singles[0].distance(probes.front());
        const double bumped = std::nextafter(ref, std::numeric_limits<double>::infinity());
        check(bitExact(ref, ref), "bvhOrder control: identical value accepted");
        check(!bitExact(bumped, ref), "bvhOrder control: 1-ulp-off rejected",
              "ref=" + std::to_string(ref) + " bumped=" + std::to_string(bumped));
    }
}

// 上界可靠性的三条底线（手册"不得私自降低上界"的可执行形式）。
// 这三例都是刻意用小预算把旧实现的退化路径踩出来的：旧实现按
// maxSamplesPerNode 把每三角形的采样点砍到 1~6 个，却仍然按 7 点集合
// 的覆盖半径 L/3 记账；顶点没查全的三角形干脆不进账。
void testErrorBoundSoundness()
{
    std::cout << "[error] bound soundness" << std::endl;
    const double pi = std::acos(-1.0);

    // 1) 环形目标 + 内接等边三角形：3 个顶点全落在环带里（顶点距离 0），
    //    真实误差完全由内部点（圆心）贡献 = 环带内半径。
    //    源三角形外接圆半径 1、最长边 L = sqrt(3)：
    //      顶点集合口径 R = L/sqrt(3) = 1.000 >= 0.98  -> 上界成立
    //      7 点集合口径 R = L/3       = 0.577 <  0.98  -> 上界被写小（违规）
    //    所以小预算（把密度压到只剩 3 个顶点）之后，上界仍然必须 >= 0.98。
    const double rIn = 0.98, rOut = 1.02;
    TriangleSoup ring, inscribed;
    const int segs = 48, copies = 40;
    for (int c = 0; c < copies; ++c)
    {
        const double ox = c * 6.0;
        for (int s = 0; s < segs; ++s)
        {
            const double t0 = 2.0 * pi * s / segs, t1 = 2.0 * pi * (s + 1) / segs;
            const osg::Vec3d i0(ox + rIn * std::cos(t0),  rIn * std::sin(t0), 0.0);
            const osg::Vec3d i1(ox + rIn * std::cos(t1),  rIn * std::sin(t1), 0.0);
            const osg::Vec3d o0(ox + rOut * std::cos(t0), rOut * std::sin(t0), 0.0);
            const osg::Vec3d o1(ox + rOut * std::cos(t1), rOut * std::sin(t1), 0.0);
            ring.add(i0, o0, o1);
            ring.add(i0, o1, i1);
        }
        osg::Vec3d p[3];
        for (int k = 0; k < 3; ++k)
        {
            const double a = pi / 2.0 + 2.0 * pi * k / 3.0;
            p[k] = osg::Vec3d(ox + std::cos(a), std::sin(a), 0.0);   // 半径 1 -> 落在环带内
        }
        inscribed.add(p[0], p[1], p[2]);
    }
    ErrorEvalConfig tight;
    tight.maxSamplesPerNode = 3;    // cap = 1 -> 每三角形只剩 3 个顶点，且细分预算为 0：
                                    // 此时上界完全由"顶点集合覆盖半径"撑起，口径错就露馅
    GeometryErrorEvaluator evTight(tight);
    ErrorResult ri = evTight.evaluate(inscribed, ring);
    check(ri.upperBoundM >= rIn - 1e-9, "vertex-only radius must still cover interior error",
          "upper=" + std::to_string(ri.upperBoundM) + " want>=" + std::to_string(rIn));
    check(ri.upperBoundM >= ri.lowerBoundM, "upper >= lower (inscribed ring case)");
    check(ri.boundLoose, "no refinement budget must be admitted as BOUND_LOOSE",
          std::string("status=") + ri.status);

    // 2) 大 soup 里只有一个离群三角形：它必须进账。旧实现用固定步长抽样
    //    （sampleEvery），步长没落在它头上时它对上下界都没有贡献，上界会
    //    停在"其它面都完全吻合"的水平 —— 数据里真实存在的 50 m 偏差被吞掉。
    const std::size_t crowd = 60001;   // 三角形总数 60002，离群者下标 60001（不是 3 的倍数）
    TriangleSoup plane, many;
    for (int i = 0; i < 10; ++i)
    {
        for (int j = 0; j < 10; ++j)
        {
            const double x0 = i * 2.0, x1 = x0 + 2.0, y0 = j * 2.0, y1 = y0 + 2.0;
            const osg::Vec3d a(x0, y0, 0.0), b(x1, y0, 0.0), c(x1, y1, 0.0), d(x0, y1, 0.0);
            plane.add(a, b, c);
            plane.add(a, c, d);
        }
    }
    for (std::size_t t = 0; t < crowd; ++t)
    {
        // 全部落在目标面覆盖范围（0..20 x 0..20）内、且完全吻合（距离 0）
        const double x = (t % 250) * 0.08, y = (t / 250) * 0.08;
        many.add(osg::Vec3d(x, y, 0.0), osg::Vec3d(x + 0.04, y, 0.0), osg::Vec3d(x, y + 0.04, 0.0));
    }
    many.add(osg::Vec3d(1.0, 1.0, 50.0), osg::Vec3d(2.0, 1.0, 50.0), osg::Vec3d(1.0, 2.0, 50.0));
    ErrorEvalConfig fullCfg;
    GeometryErrorEvaluator evDefault(fullCfg);
    ErrorResult ro = evDefault.evaluate(many, plane);
    check(ro.lowerBoundM >= 50.0 - 1e-6, "outlier triangle must reach the lower bound",
          "lower=" + std::to_string(ro.lowerBoundM));
    check(ro.upperBoundM >= 50.0, "outlier triangle must reach the upper bound",
          "upper=" + std::to_string(ro.upperBoundM));

    // 3) 预算真的不够时必须承认没收敛（BOUND_LOOSE），而不是悄悄报一个更小的数；
    //    反过来，堆排空 = 细分收敛，不能因为样本数撞过上限就误标。
    TriangleSoup coarse;
    coarse.add(osg::Vec3d(0, 0, 0), osg::Vec3d(10, 0, 0), osg::Vec3d(0, 10, 0));
    TriangleSoup fine;
    fine.add(osg::Vec3d(0, 0, 0.4), osg::Vec3d(10, 0, 0.4), osg::Vec3d(0, 10, 0.4));
    ErrorEvalConfig starved;
    starved.maxSamplesPerNode = 12;
    GeometryErrorEvaluator evStarved(starved);
    ErrorResult rs = evStarved.evaluate(coarse, fine);
    check(rs.boundLoose, "starved budget reports BOUND_LOOSE, not a tightened number",
          std::string("status=") + rs.status);
    check(rs.upperBoundM >= 0.4 - 1e-6, "starved budget upper bound still covers true error",
          "upper=" + std::to_string(rs.upperBoundM));
    // 收敛到容差以内时必须承认收敛（不能因为样本数撞过上限就误标 BOUND_LOOSE）：
    // 这里用放大的容差，让二分树在预算内真的排空——1 m² 的面要压到 1e-3 需要
    // 2^19 个子 patch，那是另一个量级的成本，不属于这条判据。
    ErrorEvalConfig convergeCfg;
    convergeCfg.toleranceMeters = 0.05;
    GeometryErrorEvaluator evFull(convergeCfg);
    TriangleSoup oneTri;
    oneTri.add(osg::Vec3d(0, 0, 0), osg::Vec3d(1, 0, 0), osg::Vec3d(0, 1, 0));
    ErrorResult rc = evFull.evaluate(oneTri, oneTri);
    check(!rc.boundLoose && rc.certified, "identical surface converges instead of claiming loose",
          std::string("status=") + rc.status + " upper=" + std::to_string(rc.upperBoundM));
}

// 独立基准：把 from 的每个三角形按重心坐标切成 n×n 份，逐点用"单三角形 BVH 取最小"
// 求到 to 的精确距离。单三角形子树必然是叶子、order 恒等，所以这条路径不经过
// 评估器实际使用的多三角形剪枝遍历，能独立证伪"上界跌破真实误差"。
double denseMaxDistanceTo(const TriangleSoup& from, const TriangleSoup& to, int n)
{
    std::vector<MeshBVH> single(to.size());
    for (std::size_t t = 0; t < to.size(); ++t)
    {
        TriangleSoup one;
        one.add(to.v[t * 3], to.v[t * 3 + 1], to.v[t * 3 + 2]);
        single[t].build(one);
    }
    double best = 0.0;
    for (std::size_t t = 0; t < from.size(); ++t)
    {
        const osg::Vec3d& a = from.v[t * 3];
        const osg::Vec3d& b = from.v[t * 3 + 1];
        const osg::Vec3d& c = from.v[t * 3 + 2];
        for (int i = 0; i <= n; ++i)
        {
            for (int j = 0; j + i <= n; ++j)
            {
                const double u = static_cast<double>(i) / n;
                const double v = static_cast<double>(j) / n;
                const osg::Vec3d p = a * (1.0 - u - v) + b * u + c * v;
                double d = std::numeric_limits<double>::max();
                for (const MeshBVH& bvh : single) { d = std::min(d, bvh.distance(p)); }
                best = std::max(best, d);
            }
        }
    }
    return best;
}

// A-55 V2：细分一个 patch 只引入 1 个新顶点（被细分那条边的中点），子 patch 的
// 另外 4 个角点在父 patch 建立时就已经查过距离，直接复用即可把每轮 6 次 BVH 查询
// 压成 1 次。省的是查询次数，值必须逐位不变，所以这里的判据是"独立测出的真实误差
// 仍被上界覆盖 + 样本记账不变"，而不是速度。
//
// 几何刻意选成三个角点距离互不相等、且首轮流到三条不同细分分支的三角形：
// 角点下标一旦写错，子 patch 的 maxSampleDist 会偏小、上界跌破真实误差，
// 第 (2) 条当场失败；样本数/堆里样本口径写错则第 (3) 条失败。
void testRefineReusesCornerDistances()
{
    std::cout << "[refineReuse] subdivision reuses parent corner distances" << std::endl;

    TriangleSoup src;
    // 最长边分别是 ab / bc / ca，覆盖细分循环里的三条切法分支
    src.add(osg::Vec3d(0, 0, 0),  osg::Vec3d(10, 0, 0), osg::Vec3d(3, 1, 0));
    src.add(osg::Vec3d(20, 0, 0), osg::Vec3d(30, 0, 0), osg::Vec3d(20, 10, 0));
    src.add(osg::Vec3d(40, 0, 0), osg::Vec3d(41, 0, 0), osg::Vec3d(42, 9, 0));
    // 目标面绕 y 轴倾斜：同一个源三角形三个角点的距离两两不等
    TriangleSoup dst;
    dst.add(osg::Vec3d(0, 0, 5), osg::Vec3d(60, 0, 0), osg::Vec3d(0, 60, 5));

    ErrorEvalConfig cfg;
    cfg.maxSamplesPerNode = 60000;   // 足够跑很多轮细分，又能当场跑完
    cfg.maxRefineIterations = 20000;
    GeometryErrorEvaluator ev(cfg);

    const GeometryErrorEvaluator::DirectionResult dr = ev.evaluateDirection(src, dst);

    // 1) 阳性对照：这个用例必须真的跑过细分，否则下面两条都是空话
    check(dr.refineIterations > 0, "refinement actually ran (positive control)",
          "iterations=" + std::to_string(dr.refineIterations)
          + " lower=" + std::to_string(dr.lower) + " upper=" + std::to_string(dr.upper));
    check(dr.initSamples >= src.size() * 3,
          "initial pass sampled every vertex",
          "initSamples=" + std::to_string(dr.initSamples));

    // 2) 独立性判据：上界必须覆盖独立稠密采样实测出的最大距离
    const double measured = denseMaxDistanceTo(src, dst, 40);
    check(dr.upper >= measured - 1e-9, "reused corner distances keep the upper bound sound",
          "upper=" + std::to_string(dr.upper) + " denseMeasured=" + std::to_string(measured));
    check(dr.lower <= measured + 1e-9 && dr.lower >= 0.0, "lower bound stays under the max",
          "lower=" + std::to_string(dr.lower) + " denseMeasured=" + std::to_string(measured));

    // 3) 记账不变：每轮细分固定贡献 6 个样本值（两个子 patch × 3 个角点），
    //    其中只有 1 个是新查询；all 里留下的值也必须与样本数一致
    check(dr.refineSamples == dr.refineIterations * 6,
          "each refinement iteration still books 6 sample values",
          "refineSamples=" + std::to_string(dr.refineSamples)
          + " iterations=" + std::to_string(dr.refineIterations));
    check(dr.samples == dr.initSamples + dr.refineSamples,
          "samples == initSamples + refineSamples",
          "samples=" + std::to_string(dr.samples));
    check(dr.all.size() == dr.samples,
          "every sample value is retained in this small-soup case",
          "all=" + std::to_string(dr.all.size()) + " samples=" + std::to_string(dr.samples));

    // 4) 同输入同二进制必须逐位相同（复用缓存后尤其要确认没有引入未初始化值）
    const GeometryErrorEvaluator::DirectionResult dr2 = ev.evaluateDirection(src, dst);
    const bool bitSame =
        std::memcmp(&dr.lower, &dr2.lower, sizeof(double)) == 0
        && std::memcmp(&dr.upper, &dr2.upper, sizeof(double)) == 0
        && std::memcmp(&dr.p95, &dr2.p95, sizeof(double)) == 0
        && std::memcmp(&dr.p99, &dr2.p99, sizeof(double)) == 0
        && dr.samples == dr2.samples && dr.refineIterations == dr2.refineIterations
        && dr.all.size() == dr2.all.size()
        && (dr.all.empty() || std::memcmp(dr.all.data(), dr2.all.data(),
                                         dr.all.size() * sizeof(double)) == 0);
    check(bitSame, "two runs are bit-identical",
          "upper1=" + std::to_string(dr.upper) + " upper2=" + std::to_string(dr2.upper));

    // 5) 双向聚合后的对外字段同样不能被复用改小
    const ErrorResult er = ev.evaluate(src, dst);
    const ErrorResult er2 = ev.evaluate(src, dst);
    check(er.upperBoundM >= measured - 1e-9, "aggregated upper covers measured error",
          "upper=" + std::to_string(er.upperBoundM) + " denseMeasured=" + std::to_string(measured));
    check(std::memcmp(&er.upperBoundM, &er2.upperBoundM, sizeof(double)) == 0
          && std::memcmp(&er.lowerBoundM, &er2.lowerBoundM, sizeof(double)) == 0
          && er.samplesUsed == er2.samplesUsed,
          "aggregated result is bit-identical across runs");

    // 6) 真实查询计数（手册 §6.1）：samples 是逻辑采样记录，distance() 调用
    //    次数只能实测。判据用"可算死的数"，不用"看起来差不多"：
    //    初始阶段每个三角形查 perTri 个点；细分每轮只查 1 个中点。
    {
        const std::size_t perTri = dr.initSamples / src.size();
        check(dr.initDistanceCalls == src.size() * perTri,
              "init queries == triangles x per-triangle samples",
              "calls=" + std::to_string(dr.initDistanceCalls)
              + " tris=" + std::to_string(src.size()) + " perTri=" + std::to_string(perTri));
        check(dr.refineDistanceCalls == dr.refineIterations,
              "one real query per refinement iteration (the rest are reused corners)",
              "refineCalls=" + std::to_string(dr.refineDistanceCalls)
              + " iterations=" + std::to_string(dr.refineIterations));
        check(dr.distanceCalls == dr.initDistanceCalls + dr.refineDistanceCalls,
              "distanceCalls == init + refine");
        // 样本数恒 >= 查询数：细分一次记 6 条样本、只发 1 次查询 —— 正因为如此，
        // 拿 samples 当查询次数会把成本高估
        check(dr.samples >= dr.distanceCalls,
              "logical samples are never fewer than real queries",
              "samples=" + std::to_string(dr.samples)
              + " calls=" + std::to_string(dr.distanceCalls));
        // BVH 工作量：每次查询至少访问根节点、至少测 1 个三角形
        check(dr.bvhNodeVisits >= dr.distanceCalls,
              "every query visits at least the root node",
              "visits=" + std::to_string(dr.bvhNodeVisits)
              + " calls=" + std::to_string(dr.distanceCalls));
        check(dr.bvhTriangleTests >= dr.distanceCalls,
              "every query tests at least one triangle",
              "tests=" + std::to_string(dr.bvhTriangleTests)
              + " calls=" + std::to_string(dr.distanceCalls));
        // 双向聚合必须等于两个单向之和
        check(er.distanceCalls >= dr.distanceCalls,
              "aggregated query count covers the single direction",
              "agg=" + std::to_string(er.distanceCalls)
              + " one=" + std::to_string(dr.distanceCalls));

        // 7) perTri 退化路径（预算压到只能采顶点）同样要对得上
        ErrorEvalConfig tight;
        tight.maxSamplesPerNode = 4;   // budget/2 = 2 < 2 tris * 3 => perTri 退到 3
        GeometryErrorEvaluator evTight(tight);
        const auto dt = evTight.evaluateDirection(src, dst);
        check(dt.initDistanceCalls == src.size() * 3,
              "degraded sampling still books exactly the queries it makes",
              "calls=" + std::to_string(dt.initDistanceCalls));
        check(dt.initSamples == src.size() * 3,
              "degraded sampling books one sample per query",
              "samples=" + std::to_string(dt.initSamples));
    }
}

void testFeatureManifest()
{
    std::cout << "[manifest] tile-local batchId mapping" << std::endl;
    TileFeatureMapper mapper;
    std::vector<FeatureId> t0{10, 20, 30};
    std::vector<FeatureId> t1{20, 40};
    mapper.addTile("tiles/t0.b3dm", t0, false);
    mapper.addTile("tiles/t1.b3dm", t1, true);

    // manifest 只登记 SourceModel 里出现过的 feature，所以这里要真的有这些对象
    SourceModel model;
    for (FeatureId fid : {10, 20, 30, 40})
    {
        SourceObject o;
        o.globalFeatureId = fid;
        o.name = "obj_" + std::to_string(fid);
        o.nodePath = "/root/obj_" + std::to_string(fid);
        SourceProperty p;
        p.key = "category";
        p.value = "test";
        o.properties.push_back(p);
        model.objects.push_back(o);
        model.featureToObject[fid] = static_cast<unsigned>(model.objects.size() - 1);
    }
    // 计数只算能回查到源 feature 的条目，manifest 与 conversion-report 同一函数
    check(mapper.sourceOccurrenceCount(model) == 5, "occurrence count",
          std::to_string(mapper.sourceOccurrenceCount(model)));
    // 落不到源 feature 上的条目（代理身份）不进计数：删掉 40 这个对象后
    // t1 里的那一条就不该被算进"源出现次数"
    {
        SourceModel partial;
        for (FeatureId fid : {10, 20, 30})
        {
            SourceObject o;
            o.globalFeatureId = fid;
            partial.objects.push_back(o);
            partial.featureToObject[fid] = static_cast<unsigned>(partial.objects.size() - 1);
        }
        check(mapper.sourceOccurrenceCount(partial) == 4, "unmapped batch entries excluded",
              std::to_string(mapper.sourceOccurrenceCount(partial)));
    }

    const json js = mapper.manifestJson(model);
    const std::string jsText = js.dump();
    check(jsText.find("\"20\"") != std::string::npos, "feature 20 present in manifest");
    check(jsText.find("tiles/t1.b3dm") != std::string::npos, "tile uri present");
    // feature 20 同时出现在两个瓦片里：必须能从 manifest 回查出两条 occurrence
    const std::string f20 = js["features"]["20"].dump();
    check(f20.find("tiles/t0.b3dm") != std::string::npos
          && f20.find("tiles/t1.b3dm") != std::string::npos,
          "cross-tile feature 20 has occurrences in both tiles");
    check(js["occurrenceCount"].get<std::size_t>() == 5, "manifest occurrence count");
    // 手册 17 节要求"代理 ID → 源 feature 集合"可查：mapper 侧只保证字段存在
    // 且是对象（内容由 ProductionPipeline::writeManifest 按 tileUri 填入）
    check(js.contains("aggregateProxies") && js["aggregateProxies"].is_object(),
          "manifest carries an aggregateProxies object");
    const std::vector<std::string> errs = mapper.validate();
    check(errs.empty(), "mapper validates", errs.empty() ? std::string() : errs.front());
}

// ---------------------------------------------------------------------------
// 手册 5.5 / AUDIT A-11 回归：源顶点属性 0 被占用时必须被搬到空闲槽，
// 不能被 _BATCHID 无声覆盖；属性 0 空闲时直接写 _BATCHID，不动其它槽。
// ---------------------------------------------------------------------------
void testBatchIdVisitorPreservesAttrib0()
{
    // --- 用例 1：属性 0 已被源数据占用，属性 2 也被占用（应落到 3）---
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array(6);
        geom->setVertexArray(verts);
        osg::ref_ptr<osg::Vec4Array> srcColor = new osg::Vec4Array(6);
        for (unsigned i = 0; i < 6; ++i) { (*srcColor)[i].set(1.0f, 0.5f, 0.25f, static_cast<float>(i)); }
        geom->setVertexAttribArray(0, srcColor, osg::Array::BIND_PER_VERTEX);
        osg::ref_ptr<osg::Vec2Array> uv = new osg::Vec2Array(6);
        geom->setVertexAttribArray(2, uv, osg::Array::BIND_PER_VERTEX);

        BatchIdVisitor biv;
        geom->accept(biv);
        // Geode 层才会递增 batchId；Drawable 层 apply 只写数组
        check(geom->getVertexAttribArray(0) != nullptr
              && geom->getVertexAttribArray(0)->getType() == osg::Array::FloatArrayType,
              "attrib0 reassigned to FloatArray (_BATCHID)");
        const auto* batchArr = dynamic_cast<const osg::FloatArray*>(geom->getVertexAttribArray(0));
        check(batchArr != nullptr && batchArr->size() == 6
              && (*batchArr)[0] == 0.0f,
              "_BATCHID written with correct length and value");
        // 源数据必须被搬到 >=2 的空闲槽（0 是新 _BATCHID，1 是 TANGENT 保留，2 已占 -> 3）
        const osg::Array* moved = geom->getVertexAttribArray(3);
        check(moved == srcColor.get(), "source attrib0 moved to slot 3");
        check(geom->getVertexAttribArray(2) == uv.get(), "existing slot 2 untouched");
    }
    // --- 用例 2：属性 0 空闲，直接写 _BATCHID，不得搬动任何槽 ---
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array(4);
        geom->setVertexArray(verts);
        osg::ref_ptr<osg::Vec3Array> normal = new osg::Vec3Array(4);
        geom->setVertexAttribArray(2, normal, osg::Array::BIND_PER_VERTEX);

        BatchIdVisitor biv;
        geom->accept(biv);
        check(geom->getVertexAttribArray(0) != nullptr
              && geom->getVertexAttribArray(0)->getType() == osg::Array::FloatArrayType,
              "clean attrib0 gets _BATCHID directly");
        check(geom->getVertexAttribArray(2) == normal.get(), "clean case leaves slot 2 alone");
    }
    // --- 用例 3：同一 Geometry 上跑两遍（双读修复 A-23 的场景），
    //     第二遍不得把已写的 _BATCHID 又当成"源数据"搬走 ---
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array(4);
        geom->setVertexArray(verts);
        osg::ref_ptr<osg::Vec4Array> srcData = new osg::Vec4Array(4);
        geom->setVertexAttribArray(0, srcData, osg::Array::BIND_PER_VERTEX);

        BatchIdVisitor first;
        geom->accept(first);
        BatchIdVisitor second;
        geom->accept(second);
        // 用例3 的 geom 只有 slot0 被源数据占用，slot2 空闲，
        // 因此第一遍把源数据搬到 slot2（findFreeAttribSlot 从 2 起找）。
        const osg::Array* slot0 = geom->getVertexAttribArray(0);
        check(slot0 != nullptr && slot0->getType() == osg::Array::FloatArrayType,
              "second pass keeps _BATCHID in slot 0");
        check(geom->getVertexAttribArray(2) == srcData.get(),
              "second pass does not re-move source data");
    }
}

// 把任意 OSG 节点的可读三角形展开进 TriangleSoup（误差评估用）
struct NodeTriangleCollector : public osg::NodeVisitor
{
    TriangleSoup soup;
    NodeTriangleCollector() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
    void apply(osg::Geometry& geometry) override
    {
        osg::Geometry* g = &geometry;
        const osg::Vec3Array* pos = dynamic_cast<const osg::Vec3Array*>(g->getVertexArray());
        if (!pos) { return; }
        for (unsigned p = 0; p < g->getNumPrimitiveSets(); ++p)
        {
            const osg::PrimitiveSet* ps = g->getPrimitiveSet(p);
            if (ps->getMode() != osg::PrimitiveSet::TRIANGLES) { continue; }
            std::vector<unsigned> idx;
            if (const auto* dui = dynamic_cast<const osg::DrawElementsUInt*>(ps))
            { idx.assign(dui->begin(), dui->end()); }
            else if (const auto* dus = dynamic_cast<const osg::DrawElementsUShort*>(ps))
            { idx.assign(dus->begin(), dus->end()); }
            else if (const auto* da = dynamic_cast<const osg::DrawArrays*>(ps))
            { for (unsigned i = da->getFirst(); i < da->getFirst() + da->getCount(); ++i) idx.push_back(i); }
            for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
            {
                if (idx[i] >= pos->size() || idx[i + 1] >= pos->size() || idx[i + 2] >= pos->size())
                { continue; }
                soup.add(osg::Vec3d((*pos)[idx[i]]), osg::Vec3d((*pos)[idx[i + 1]]),
                         osg::Vec3d((*pos)[idx[i + 2]]));
            }
        }
    }
};

// ---------------------------------------------------------------------------
// AUDIT A-26 回归：Simplifier 必须能简化含多个 primitiveSet 的几何。
// 原实现在 psetCount > 1 时直接 return（因为顶点重映射只按单个 pset 的
// usedVertices 重建 positions，多 pset 会互相踩索引），结果是这类几何
// 完全不简化 —— 预算驱动削减静默失效。
// ---------------------------------------------------------------------------
void testSimplifierMultiplePrimitiveSets()
{
    std::cout << "[simplifier] multi primitiveSet geometry" << std::endl;

    auto emitPatch = [](osg::Vec3Array* pos, osg::DrawElementsUInt* idx,
                        double ox, double oy, int n, unsigned& nextVert)
    {
        for (int y = 0; y < n; ++y)
        {
            for (int x = 0; x < n; ++x)
            {
                const double xa = ox + x, xb = ox + x + 1.0;
                const double ya = oy + y, yb = oy + y + 1.0;
                pos->push_back(osg::Vec3(static_cast<float>(xa), static_cast<float>(ya), 0.0f));
                pos->push_back(osg::Vec3(static_cast<float>(xb), static_cast<float>(ya), 0.0f));
                pos->push_back(osg::Vec3(static_cast<float>(xb), static_cast<float>(yb), 0.0f));
                pos->push_back(osg::Vec3(static_cast<float>(xa), static_cast<float>(yb), 0.0f));
                const unsigned b = nextVert;
                nextVert += 4;
                idx->push_back(b); idx->push_back(b + 1); idx->push_back(b + 2);
                idx->push_back(b); idx->push_back(b + 2); idx->push_back(b + 3);
            }
        }
    };

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
    osg::ref_ptr<osg::DrawElementsUInt> p0 =
        new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
    osg::ref_ptr<osg::DrawElementsUInt> p1 =
        new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
    unsigned nextVert = 0;
    emitPatch(pos.get(), p0.get(), 0.0, 0.0, 20, nextVert);     // 800 三角形
    emitPatch(pos.get(), p1.get(), 100.0, 0.0, 20, nextVert);   // 800 三角形
    geom->setVertexArray(pos.get());
    // 逐顶点离散 ID 属性：简化后长度必须与新顶点数一致，否则 _BATCHID 错位
    osg::ref_ptr<osg::FloatArray> ids = new osg::FloatArray;
    for (unsigned i = 0; i < nextVert; ++i) { ids->push_back(static_cast<float>(i % 7)); }
    geom->setVertexAttribArray(3, ids.get(), osg::Array::BIND_PER_VERTEX);
    geom->addPrimitiveSet(p0.get());
    geom->addPrimitiveSet(p1.get());

    const unsigned before = p0->getNumIndices() / 3 + p1->getNumIndices() / 3;
    check(before == 1600, "two primitive sets built", std::to_string(before));

    Simplifier simplifier(0.25, false, false, 1e-2f);
    geom->accept(simplifier);

    unsigned after = 0, maxIndex = 0;
    bool indexTypeOk = true;
    for (unsigned p = 0; p < geom->getNumPrimitiveSets(); ++p)
    {
        const osg::PrimitiveSet* ps = geom->getPrimitiveSet(p);
        if (ps->getMode() != osg::PrimitiveSet::TRIANGLES) { continue; }
        const auto* de = dynamic_cast<const osg::DrawElementsUInt*>(ps);
        const auto* ds = dynamic_cast<const osg::DrawElementsUShort*>(ps);
        if (de)
        {
            after += de->getNumIndices() / 3;
            for (const unsigned v : *de) { maxIndex = osg::maximum(maxIndex, v); }
        }
        else if (ds)
        {
            after += ds->getNumIndices() / 3;
            for (const unsigned short v : *ds) { maxIndex = osg::maximum(maxIndex, static_cast<unsigned>(v)); }
        }
        else { indexTypeOk = false; }
    }
    const unsigned vertsAfter = geom->getVertexArray()->getNumElements();
    check(indexTypeOk, "primitive sets stay index draw elements");
    check(geom->getNumPrimitiveSets() == 2, "both primitive sets kept",
          std::to_string(geom->getNumPrimitiveSets()));
    check(after < before, "multi-pset geometry actually simplifies",
          "before=" + std::to_string(before) + " after=" + std::to_string(after));
    check(after > 0, "multi-pset geometry not emptied", std::to_string(after));
    check(maxIndex < vertsAfter, "all indices within remapped vertex range",
          "maxIndex=" + std::to_string(maxIndex) + " verts=" + std::to_string(vertsAfter));
    const osg::Array* kept = geom->getVertexAttribArray(3);
    check(kept != nullptr && kept->getNumElements() == vertsAfter,
          "per-vertex attribute stays consistent with vertex count",
          kept ? ("attrib=" + std::to_string(kept->getNumElements())
                  + " verts=" + std::to_string(vertsAfter)) : std::string("attrib dropped"));
}

// ---------------------------------------------------------------------------
// AUDIT A-27 回归：聚合代理必须覆盖该节点的完整源几何范围。
// 原实现 buildAggregate 里 cluster 只取 getChild(0) 的 Geode，其余 feature
// 的几何被静默丢弃 —— 远景 LOD 会出现整块对象消失（手册 15 节 HOLE），
// 且违反 0.1「不能丢失源几何」与 7.3「代理构建失败不能导致几何丢失」。
// ---------------------------------------------------------------------------
// 阶段 2 裁定项：profile.maxProxyAttempts 必须真的驱动 simplifyNode 的加压阶梯。
// 在 ffbb3fe 里这个字段只被 toJson() 回显，循环写死 step < 8，属于无效配置。
void testProxyAttemptsIsEffective()
{
    std::cout << "[proxy] maxProxyAttempts actually drives the simplify ladder" << std::endl;

    const int N = 64;                       // 64x64 网格 -> 2*(N-1)^2 = 8192 三角形
    const unsigned FULL = 2 * static_cast<unsigned>(N - 1) * static_cast<unsigned>(N - 1);
    osg::ref_ptr<osg::Group> root = new osg::Group;
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->setName("grid");
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i)
            pos->push_back(osg::Vec3(static_cast<float>(i), static_cast<float>(j), 0.0f));
    osg::ref_ptr<osg::DrawElementsUInt> idx =
        new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
    auto v = [N](int i, int j) { return static_cast<unsigned>(j * N + i); };
    for (int j = 0; j + 1 < N; ++j)
        for (int i = 0; i + 1 < N; ++i)
        {
            idx->push_back(v(i, j)); idx->push_back(v(i + 1, j)); idx->push_back(v(i + 1, j + 1));
            idx->push_back(v(i, j)); idx->push_back(v(i + 1, j + 1)); idx->push_back(v(i, j + 1));
        }
    geom->setVertexArray(pos.get());
    geom->addPrimitiveSet(idx.get());
    geode->addDrawable(geom.get());
    root->addChild(geode.get());

    SourceModel m;
    check(m.build(root.get()), "grid scene build");
    std::vector<std::uint32_t> items;
    for (std::size_t i = 0; i < m.prims.size(); ++i) { items.push_back(static_cast<std::uint32_t>(i)); }

    const unsigned target = 200;
    auto runWith = [&](std::size_t attempts) -> ProxyResult {
        ProductionProfile p;
        p.maxProxyAttempts = attempts;
        p.budget.targetTrianglesPerTile = target;
        p.budget.maxTrianglesPerTile = FULL;
        ProxyMeshBuilder b(m, p, m.prims);
        // allowAggregate=false：只走身份保持型路径，结果面数完全由阶梯次数决定
        return b.build(items, target, false);
    };

    const ProxyResult r8 = runWith(8);
    const ProxyResult r1 = runWith(1);
    const ProxyResult r0 = runWith(0);

    check(r0.node.valid() && r1.node.valid() && r8.node.valid(), "all three ladders produced a node");
    check(r0.triangles == FULL, "attempts=0 leaves the proxy un-simplified",
          std::to_string(r0.triangles) + " vs " + std::to_string(FULL));
    check(r1.triangles < FULL, "attempts=1 simplifies once", std::to_string(r1.triangles));
    check(r8.triangles <= r1.triangles, "more attempts never increase the triangle count",
          std::to_string(r8.triangles) + " > " + std::to_string(r1.triangles));
    check(r8.triangles < r1.triangles, "the attempt budget changes the outcome (config is live)",
          std::to_string(r8.triangles) + " == " + std::to_string(r1.triangles));
    check(r8.triangles <= target, "the 8-step ladder still reaches the triangle target",
          std::to_string(r8.triangles));
    check(ProductionProfile().maxProxyAttempts == 8,
          "profile default stays 8 == the value hard-coded before this change");
    const ProxyResult r8b = runWith(8);
    check(r8b.triangles == r8.triangles && r8b.appliedRatio == r8.appliedRatio,
          "same attempts -> same proxy size", std::to_string(r8b.triangles));
}

void testProxyAggregateCoversSourceScope()
{
    std::cout << "[proxy] aggregate fallback covers source scope" << std::endl;

    // 每个 feature 只有 2 个三角形：身份保持型简化最低只能到 2*featureCount，
    // 远低于预算的目标时就会进入聚合 fallback。
    const int FEATURES = 1200;
    osg::ref_ptr<osg::Group> root = new osg::Group;
    for (int i = 0; i < FEATURES; ++i)
    {
        const double gx = (i % 40) * 10.0;
        const double gy = (i / 40) * 10.0;
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->setName("small_" + std::to_string(i));
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
        pos->push_back(osg::Vec3(static_cast<float>(gx), static_cast<float>(gy), 0.0f));
        pos->push_back(osg::Vec3(static_cast<float>(gx + 8.0), static_cast<float>(gy), 0.0f));
        pos->push_back(osg::Vec3(static_cast<float>(gx + 8.0), static_cast<float>(gy + 8.0), 0.0f));
        pos->push_back(osg::Vec3(static_cast<float>(gx), static_cast<float>(gy + 8.0), 0.0f));
        osg::ref_ptr<osg::DrawElementsUInt> idx =
            new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
        idx->push_back(0); idx->push_back(1); idx->push_back(2);
        idx->push_back(0); idx->push_back(2); idx->push_back(3);
        geom->setVertexArray(pos.get());
        geom->addPrimitiveSet(idx.get());
        geode->addDrawable(geom.get());
        root->addChild(geode.get());
    }

    SourceModel m;
    check(m.build(root.get()), "small-feature scene build");
    check(m.prims.size() == FEATURES, "one prim per feature",
          std::to_string(m.prims.size()));

    ProductionProfile profile;
    const unsigned target = 120;
    profile.budget.targetTrianglesPerTile = target;
    profile.budget.maxTrianglesPerTile = target * 2;

    std::vector<std::uint32_t> items;
    for (std::size_t i = 0; i < m.prims.size(); ++i) { items.push_back(static_cast<std::uint32_t>(i)); }

    ProxyMeshBuilder builder(m, profile, m.prims);
    const ProxyResult r = builder.build(items, target, true);
    check(r.failure.empty(), "aggregate proxy generation succeeds",
          r.failure.empty() ? std::string() : r.failure);
    check(r.node.valid(), "aggregate proxy node present");
    if (r.aggregate)
    {
        check(r.triangles <= target, "aggregate proxy within triangle budget",
              std::to_string(r.triangles) + " > " + std::to_string(target));
    }
    if (!r.node.valid()) { return; }

    // 覆盖性判据（A-27 的直接回归）：聚合器按 cell 出代理，任何一个被源占据的
    // cell 都必须在代理里有三角形。旧实现只取 cluster 的 child(0)，整个 cell
    // 的其余 feature 被丢掉，表现为远景出现整块空洞。
    std::vector<SourceModel::Triangle> srcTris;
    m.collectTriangles(m.prims, items, srcTris);
    TriangleSoup src, prox;
    for (const SourceModel::Triangle& t : srcTris) { src.add(t.a, t.b, t.c); }
    NodeTriangleCollector collector;
    r.node->accept(collector);
    prox = collector.soup;
    check(prox.size() > 0, "aggregate proxy carries triangles", std::to_string(prox.size()));

    const osg::Vec3d bmin(m.bounds.xMin(), m.bounds.yMin(), m.bounds.zMin());
    const osg::Vec3d bmax(m.bounds.xMax(), m.bounds.yMax(), m.bounds.zMax());
    const double diag = m.bounds.valid() ? (bmax - bmin).length() : 1.0;
    const double cellSize = std::max(1e-6, diag * 0.25);   // 与 build() 的 cellSize 一致

    using CellKey = std::array<long long, 3>;
    auto cellOf = [&](const osg::Vec3d& p) {
        return CellKey{ static_cast<long long>(std::floor(p.x() / cellSize)),
                        static_cast<long long>(std::floor(p.y() / cellSize)),
                        static_cast<long long>(std::floor(p.z() / cellSize)) };
    };
    auto occupiedCells = [&](const TriangleSoup& s) {
        std::set<CellKey> cells;
        for (std::size_t t = 0; t + 2 < s.v.size(); t += 3)
        {
            const osg::Vec3d c = (s.v[t] + s.v[t + 1] + s.v[t + 2]) * (1.0 / 3.0);
            cells.insert(cellOf(c));
        }
        return cells;
    };
    const std::set<CellKey> srcCells = occupiedCells(src);
    const std::set<CellKey> proxCells = occupiedCells(prox);
    std::vector<CellKey> missing;
    for (const CellKey& c : srcCells) { if (!proxCells.count(c)) { missing.push_back(c); } }
    check(missing.empty(), "every source cell is represented in the aggregate proxy",
          "srcCells=" + std::to_string(srcCells.size())
          + " proxCells=" + std::to_string(proxCells.size())
          + " missing=" + std::to_string(missing.size()));

    // 采样下界：聚合代理是"按 cell 抽稀 feature"，误差不得越过聚合 cell 自身
    // 尺度（cell 对角线 = cellSize*sqrt(2)）。A-27 旧行为是 497.2m（≈4 个 cell
    // 的整块空洞），现在 130m（<1 个 cell 对角线）。保守上界会把大三角形的
    // 覆盖半径也计入，不适合做硬阈值，这里一并打印如实反映。
    GeometryErrorEvaluator ev(profile.error);
    const ErrorResult er = ev.evaluate(src, prox);
    std::cout << "    error: lower=" << er.lowerBoundM << " upper=" << er.upperBoundM
              << " cellSize=" << cellSize << " cellDiag=" << cellSize * 1.4143
              << " tris=" << prox.size() << std::endl;
    check(er.lowerBoundM <= cellSize * 1.5,
          "aggregate proxy error stays within one cell scale",
          "lower=" + std::to_string(er.lowerBoundM) + " cellSize=" + std::to_string(cellSize));
}

// ---------------------------------------------------------------------------
// 远景 LOD 代理的纹理预算。回归的是 longxiang 的实测缺陷：调用方按 primitive
// 累加纹理字节，同一张纹理被 N 个 primitive 引用就算 N 次，42/42 代理被判
// "超预算"，整片退化成无贴图灰模。代理字节估算必须按 osg::Image 去重，并且
// 能折算"降到下限分辨率"后的字节，调用方才有依据先降分辨率、最后才去纹理。
osg::ref_ptr<osg::Geode> makeTexturedQuad(const osg::ref_ptr<osg::Texture2D>& tex)
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
    pos->push_back(osg::Vec3(0, 0, 0));
    pos->push_back(osg::Vec3(1, 0, 0));
    pos->push_back(osg::Vec3(1, 0, 1));
    pos->push_back(osg::Vec3(0, 0, 1));
    geom->setVertexArray(pos.get());
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3));
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 1));
    geode->addDrawable(geom.get());
    osg::ref_ptr<osg::StateSet> ss = new osg::StateSet;
    ss->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);
    geode->setStateSet(ss.get());
    return geode;
}

osg::ref_ptr<osg::Texture2D> makeTextured(const unsigned edgePx)
{
    osg::ref_ptr<osg::Image> img = new osg::Image;
    img->allocateImage(static_cast<int>(edgePx), static_cast<int>(edgePx), 1,
                       GL_RGBA, GL_UNSIGNED_BYTE);
    return new osg::Texture2D(img.get());
}

void testProxyTextureBudget()
{
    std::cout << "[proxyTexture] shared textures dedup + floor scaling" << std::endl;
    const unsigned edge = 2048u;
    const std::uint64_t perImage =
        (static_cast<std::uint64_t>(edge) * edge * 4ull) / 8ull;  // 估算口径：解码 /8

    osg::ref_ptr<osg::Texture2D> tex = makeTextured(edge);

    // 1) 两个 drawable 共享同一张纹理：只能算一次
    osg::ref_ptr<osg::Group> shared = new osg::Group;
    shared->addChild(makeTexturedQuad(tex).get());
    shared->addChild(makeTexturedQuad(tex).get());
    const ProxyBytes s = ProxyMeshBuilder::estimateBytes(shared.get());
    check(s.texFull == perImage, "shared texture counted once, not per primitive",
          "got=" + std::to_string(s.texFull) + " want=" + std::to_string(perImage));

    // 阳性对照：两张不同的纹理必须算两次，否则"去重"其实是永远只算一份
    osg::ref_ptr<osg::Group> distinct = new osg::Group;
    distinct->addChild(makeTexturedQuad(tex).get());
    distinct->addChild(makeTexturedQuad(makeTextured(edge)).get());
    const ProxyBytes d = ProxyMeshBuilder::estimateBytes(distinct.get());
    check(d.texFull == perImage * 2ull, "two distinct textures count twice",
          "got=" + std::to_string(d.texFull) + " want=" + std::to_string(perImage * 2ull));

    // 2) 降到下限分辨率的折算：面积比 (64/2048)^2 = 1/1024
    const std::uint64_t wantFloor =
        static_cast<std::uint64_t>(static_cast<double>(perImage)
            * (static_cast<double>(proxyTextureFloorEdgePx) / edge)
            * (static_cast<double>(proxyTextureFloorEdgePx) / edge));
    check(s.texFloor == wantFloor, "floor estimate scales by area",
          "got=" + std::to_string(s.texFloor) + " want=" + std::to_string(wantFloor));
    check(s.texFloor < s.texFull, "floor estimate is strictly smaller than full res");

    // 3) 小于下限的纹理不缩水（否则一个 32px 法线贴图会被"折算"成更小）
    // 注：变量名不能用 small —— Windows SDK 的 rpcndr.h 里有 `#define small char`。
    osg::ref_ptr<osg::Group> tinyGroup = new osg::Group;
    tinyGroup->addChild(makeTexturedQuad(makeTextured(32u)).get());
    const ProxyBytes tiny = ProxyMeshBuilder::estimateBytes(tinyGroup.get());
    const std::uint64_t tinyFull = (32ull * 32ull * 4ull) / 8ull;
    check(tiny.texFull == tinyFull && tiny.texFloor == tinyFull,
          "textures at or below the floor keep their full byte cost",
          "full=" + std::to_string(tiny.texFull) + " floor=" + std::to_string(tiny.texFloor));

    // 4) 几何估算照旧在 geo 里，不会被纹理估算吞掉。
    // shared 节点 = 2 个 quad，每个 quad 2 个 DrawArrays(TRIANGLES)（1 面 + 0 面）
    // 和 4 个顶点 → 2 面、8 顶点。
    check(s.geo == 2ull * 200ull + 8ull * 48ull, "geometry part unchanged",
          "got=" + std::to_string(s.geo));

    // 5) 空节点是 0，不是脏值
    const ProxyBytes empty = ProxyMeshBuilder::estimateBytes(nullptr);
    check(empty.geo == 0 && empty.texFull == 0 && empty.texFloor == 0,
          "null node estimates to zero");
}

// 超限兜底阶梯的档位选择。要紧的是"叶瓦片（textureMaxEdgePx=0）一档都不能少"：
// 判据一度写成与钳制后的全局 -tw/-th 比较，而叶瓦片的 topts 正是 2048，条件恒真，
// 整套字节兜底对叶静默失效；集成用例走的是三角形超限分支，看不见这个退化。
void testProxyTextureLadder()
{
    std::cout << "[proxyTextureLadder] byte-overflow downscale rungs" << std::endl;
    const std::vector<unsigned> leaf = ProductionPipeline::textureDownscaleLadder(0u);
    check(leaf.size() == 3, "leaf tiles keep every rung",
          "count=" + std::to_string(leaf.size()));
    check(!leaf.empty() && leaf.front() == 256u && leaf.back() == proxyTextureFloorEdgePx,
          "leaf ladder starts at 256 and stops at the floor",
          "front=" + std::to_string(leaf.empty() ? 0u : leaf.front())
              + " back=" + std::to_string(leaf.empty() ? 0u : leaf.back()));

    // 512px 代理首轮压过之后，256/128/64 仍然都比首轮低，一档都不该被跳过
    check(ProductionPipeline::textureDownscaleLadder(512u) == leaf,
          "a 512px proxy keeps the whole ladder because every rung is below it");

    const std::vector<unsigned> p128 = ProductionPipeline::textureDownscaleLadder(128u);
    check(p128.size() == 1 && (!p128.empty() && p128[0] == 64u),
          "a 128px proxy only has 64 left to try",
          "count=" + std::to_string(p128.size()));

    const std::vector<unsigned> atFloor =
        ProductionPipeline::textureDownscaleLadder(proxyTextureFloorEdgePx);
    check(atFloor.empty(), "at the floor there is nothing smaller to fall back to");
    check(!leaf.empty() && !p128.empty() && atFloor.empty(),
          "positive control: empty ladder is the floor case only, not a dead function");
}

// ---------------------------------------------------------------------------
// 代理瓦片降分辨率的隔离性。回归的是 longxiang 的产物级缺陷：给内节点代理压到
// 512px 之后，同一批次里的**叶**瓦片贴图字节掉了一到两成（t71 的 KTX2 从 534958B
// 变成 294305B，尺寸却仍是 2048x2048 —— 先缩小再被塞回大图集页的糊图特征）。
//
// 通路有两条，都必须钉住：
//   1) osg::clone(StateSet, DEEP_COPY_TEXTURES) 只克隆到 osg::Texture 层，
//      osg::Image 依旧是同一份共享对象；
//   2) resizeImageToPowerOfTwo 曾经就地 scaleImage，于是第一个降分辨率的瓦片
//      改掉了源模型里那张原图，后续所有引用它的瓦片（含叶）一起糊掉。
// ---------------------------------------------------------------------------
void testOptimizerResizeIsolation()
{
    std::cout << "[texResize] shared osg::Image must never be resized in place" << std::endl;

    osg::ref_ptr<osg::Image> img = new osg::Image;
    img->allocateImage(1024, 1024, 1, GL_RGBA, GL_UNSIGNED_BYTE);
    img->data(0, 0, 0)[0] = 123;

    // 前提核对（不是被测行为）：若哪天 OSG 把 DEEP_COPY_TEXTURES 做成连图一起深拷，
    // 这条会红，届时"私有化"就从必须变成冗余，但不能反过来假设它一直成立。
    osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(img.get());
    osg::ref_ptr<osg::StateSet> ss = new osg::StateSet;
    ss->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);
    osg::ref_ptr<osg::StateSet> cloned = osg::clone(ss.get(),
        osg::CopyOp::DEEP_COPY_STATESETS | osg::CopyOp::DEEP_COPY_STATEATTRIBUTES
                                       | osg::CopyOp::DEEP_COPY_TEXTURES);
    osg::Texture2D* clonedTex = static_cast<osg::Texture2D*>(
        cloned->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
    check(clonedTex != tex.get(), "clone gives a private Texture object");
    check(clonedTex->getImage() == img.get(),
          "premise: cloned Texture still SHARES the osg::Image (pixels are the shared part)");

    // 1) 需要改像素：必须返回私有副本，原图一个字节都不能动。
    osg::ref_ptr<osg::Image> shrunk =
        GltfOptimizer::TextureAtlasBuilderVisitor::resizeImageToPowerOfTwo(img, 512, 512);
    check(shrunk != img.get(), "a resize returns a private copy, not the shared image",
          "samePointer=" + std::to_string(shrunk == img.get()));
    check(shrunk->s() == 512 && shrunk->t() == 512, "copy is at the requested cap",
          "got=" + std::to_string(shrunk->s()) + "x" + std::to_string(shrunk->t()));
    check(img->s() == 1024 && img->t() == 1024, "source image keeps its resolution",
          "got=" + std::to_string(img->s()) + "x" + std::to_string(img->t()));
    check(img->data(0, 0, 0)[0] == 123, "source pixels untouched");

    // 2) 原图不得被写脏：ORIGIN_* 是共享对象上的元数据，写上去等于把别的瓦片
    //    的判定基准也改了。
    int originWidth = 0;
    check(!img->getUserValue(ORIGIN_WIDTH, originWidth),
          "source image carries no origin-width written by the resize");

    // 3) 副本自己记住改动前的尺寸，降档阶梯才能继续从它往下折。
    int copyOrigin = 0;
    check(shrunk->getUserValue(ORIGIN_WIDTH, copyOrigin) && copyOrigin == 1024,
          "the private copy records the pre-resize size",
          "got=" + std::to_string(copyOrigin));

    // 4) 不需要改动时返回入参本身：既不做无谓的整幅拷贝（图集路径每瓦片几十张图），
    //    也不留两份可被分别改写的像素。
    osg::ref_ptr<osg::Image> kept =
        GltfOptimizer::TextureAtlasBuilderVisitor::resizeImageToPowerOfTwo(shrunk, 2048, 2048);
    check(kept == shrunk.get(), "an image already within the cap is returned unchanged");

    // 阳性对照：低于上限但不是 2 的幂的图同样要私有化——判据是"像素会不会变"，
    // 不是"是否超上限"。少了这条，非 2 次幂贴图又会走回就地缩放。
    osg::ref_ptr<osg::Image> odd = new osg::Image;
    odd->allocateImage(300, 300, 1, GL_RGBA, GL_UNSIGNED_BYTE);
    osg::ref_ptr<osg::Image> oddOut =
        GltfOptimizer::TextureAtlasBuilderVisitor::resizeImageToPowerOfTwo(odd, 2048, 2048);
    check(oddOut != odd.get() && odd->s() == 300,
          "a sub-cap but non-power-of-two image is still copied, not resized in place",
          "out=" + std::to_string(oddOut->s()) + " src=" + std::to_string(odd->s()));

    // 5) 空图不炸、不伪造副本
    osg::ref_ptr<osg::Image> null;
    check(GltfOptimizer::TextureAtlasBuilderVisitor::resizeImageToPowerOfTwo(null, 512, 512) == nullptr,
          "null image passes through");
}

// ---------------------------------------------------------------------------
// 手册 9.5 / 17："每个 source feature 在其所属叶瓦片中都存在"、"REPLACE 子树
// 完整覆盖其 source scope"、失败关闭不留半成品。
//
// 这是唯一能观察到 A-27 那类缺陷的用例层级：覆盖丢失在 Hausdorff 数值上是
// 隐形的（被丢的小簇紧贴相邻表面时误差照样很小），只有跑完整管线、按
// globalFeatureId 逐叶核对才会暴露。
void testPipelineLeafCoverageInvariant()
{
    std::cout << "[pipeline] leaf coverage invariant" << std::endl;
    Utils::registerFileAliases();   // .b3dm -> osgdb_gltf 写出器

    ProductionProfile profile;
    profile.budget.maxTrianglesPerTile = 12;
    profile.budget.targetTrianglesPerTile = 8;

    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "wb_pipeline_coverage";
    std::error_code ec;
    std::filesystem::remove_all(out, ec);

    ProductionPipeline pipeline(profile);
    ProductionPipeline::Options opt;
    opt.outputDir = out.string();
    const bool ran = pipeline.run(makeGrid(8, 8, 1.0).get(), opt);
    const bool fin = pipeline.finalize();
    check(ran && fin, "synthetic pipeline run+finalize succeeds",
          joinMessages(pipeline.failures()));

    const unsigned srcFeatures = pipeline.report().sourceFeatureCount;
    check(srcFeatures == 64, "source registry holds one feature per grid object",
          std::to_string(srcFeatures));

    std::set<FeatureId> leafScope;
    unsigned leaves = 0, leafsWithContent = 0, emptyLeafContent = 0;
    for (const ProductionTile& t : pipeline.tiles())
    {
        if (!t.isLeaf) { continue; }
        ++leaves;
        if (t.hasContent) { ++leafsWithContent; }
        if (t.hasContent && t.triangles == 0) { ++emptyLeafContent; }
        leafScope.insert(t.sourceScope.begin(), t.sourceScope.end());
    }
    check(leaves > 1, "tree really split into several leaf tiles", std::to_string(leaves));
    check(leafsWithContent == leaves, "every leaf tile carries content",
          std::to_string(leafsWithContent) + "/" + std::to_string(leaves));
    check(emptyLeafContent == 0, "no leaf tile writes zero-triangle content");
    check(leafScope.size() == srcFeatures, "leaf tiles together cover every source feature",
          std::to_string(leafScope.size()) + "/" + std::to_string(srcFeatures));
    check(pipeline.report().featuresMissingFromLeaves == 0, "coverage hole counter stays zero");
    check(std::filesystem::exists(out / "tileset.json", ec), "tileset.json published");
    check(std::filesystem::exists(out / "conversion-report.json", ec), "report published");

    // A-47：仪表必须**接得上**才谈得上归因。上一轮的教训就是计时器在算、
    // 但没落到 report 里，跑完照样读不到。这里合成模型没有纹理，
    // 所以只钉"字段存在"（接线），数值闭合靠产物级复跑。
    {
        auto hasStage = [&pipeline](const char* name) {
            for (const StageTiming& s : pipeline.report().stages)
            {
                if (s.name == name) { return true; }
            }
            return false;
        };
        auto hasCounter = [&pipeline](const char* name) {
            for (const StageCount& c : pipeline.report().counters)
            {
                if (c.name == name) { return true; }
            }
            return false;
        };
        check(hasStage("micro_texture_export") && hasStage("micro_texture_export_process_cpu"),
              "texture-export instrument is wired into the report");
        check(hasStage("micro_materialize") && hasStage("micro_total_cpu"),
              "leaf materialize + CPU-layer stages exist");
        bool anyTileHasEvaluateSec = false;
        for (const ProductionTile& t : pipeline.tiles())
        {
            if (t.timing.evaluateSec > 0.0 || t.timing.materializeSec > 0.0) { anyTileHasEvaluateSec = true; break; }
        }
        check(anyTileHasEvaluateSec, "per-tile timing reaches the report (A-47 criterion 2)");
        check(hasCounter("micro_evaluate_samples"), "sample counts live in counters");
        bool countLeakedIntoStages = false;
        for (const StageTiming& s : pipeline.report().stages)
        {
            if (s.name.find("samples") != std::string::npos
                || s.name.find("candidates") != std::string::npos) { countLeakedIntoStages = true; }
        }
        check(!countLeakedIntoStages, "no counter leaked into the seconds array (A-47 criterion 3)");
    }

    // manifest.aggregateProxies 必须给每个"发布了内容的聚合代理"一条按 tileUri
    // 索引的映射，key 只能指向真实发布出去的瓦片（手册 17 节：从代理 ID 反查
    // 源 ID 集合）。降级成无内容的代理不带这个 key——它的 aggregateSources
    // 仍然非空（ProxyMeshBuilder 建的时候就填了），少了 hasContent 这道闸就会
    // 往 manifest 里写一个空字符串 key。
    {
        std::ifstream mf(out / "feature-manifest.json", std::ios::binary);
        nlohmann::json man;
        const bool parsed = mf.is_open() && (mf >> man).good();
        check(parsed, "feature-manifest.json published and parses",
              parsed ? "ok" : (out / "feature-manifest.json").string());
        const nlohmann::json& agg = man["aggregateProxies"];
        unsigned aggPublished = 0, emptyKeys = 0;
        for (const ProductionTile& t : pipeline.tiles())
        {
            if (t.aggregate && t.hasContent && !t.aggregateSources.empty()) { ++aggPublished; }
        }
        // 夹具必须真的产出聚合代理，否则下面所有断言都是空断言
        check(aggPublished >= 1, "fixture really publishes aggregate proxy tiles",
              std::to_string(aggPublished));
        check(agg.is_object() && agg.size() == aggPublished,
              "aggregateProxies has exactly one entry per published aggregate tile",
              std::to_string(agg.is_object() ? agg.size() : 0) + "/"
              + std::to_string(aggPublished));
        for (const auto& kv : agg.items())
        {
            if (kv.key().empty()) { ++emptyKeys; continue; }
            bool ok = false;
            for (const ProductionTile& t : pipeline.tiles())
            {
                if (t.uri == kv.key()) { ok = t.hasContent && t.aggregate && !t.aggregateSources.empty(); }
            }
            check(ok, "aggregateProxies key is a published aggregate tile", kv.key());
            check(!kv.value().empty(), "aggregate entry carries a non-empty source set", kv.key());
        }
        check(emptyKeys == 0, "no content-less proxy leaks an empty aggregateProxies key",
              std::to_string(emptyKeys));
    }

    // 失败关闭：硬字节上限压到不可能满足时，必须拒绝产出，而不是在输出目录里
    // 留下一个"看着成功其实缺件"的 tileset（手册 14.17）。
    ProductionProfile tight = profile;
    tight.budget.maxTileContentBytes = 256;
    tight.budget.targetTileContentBytes = 128;
    const std::filesystem::path out2 =
        std::filesystem::temp_directory_path() / "wb_pipeline_coverage_failclosed";
    std::filesystem::remove_all(out2, ec);
    ProductionPipeline rejecting(tight);
    ProductionPipeline::Options opt2;
    opt2.outputDir = out2.string();
    (void)rejecting.run(makeGrid(8, 8, 1.0).get(), opt2);
    const bool fin2 = rejecting.finalize();
    check(!fin2, "over-budget run is rejected instead of published");
    check(!std::filesystem::exists(out2 / "tileset.json", ec),
          "no half-baked tileset left in the output directory");
    check(!rejecting.failures().empty(), "rejection is explained by recorded failures",
          joinMessages(rejecting.failures()));
}

} // namespace

// A-31：内节点远景代理超硬限时必须"降级为无内容 + 计数"，而不是让整批数据集
// 报废；叶超限仍然判失败（14.17）。
// 构造方式：strictIdentity 关掉聚合兜底，把代理目标面数压到这套网格压不到的
// 程度（贴边四边形只能塌缩掉一部分内部边），内节点代理必然报
// "proxy exceeds triangle budget" → 走 w.failure 降级分支。
// 写盘后实测超限的那条分支（同一个函数里，会再删掉超限文件）是防御网，
// 兜"估算达标但实测超限"，本用例命中的是构建期分支，不为此伪造数据。
void testPipelineProxySkipDegrades()
{
    std::cout << "[pipeline] far-LOD proxy over budget degrades" << std::endl;
    Utils::registerFileAliases();

    // 512 个源三角形、8 三角形/对象：分区后叶瓦片 64 面、中层代理 96 面（都
    // 达标），根代理要覆盖全部 512 面、压不到 100 面目标，于是只有根这一层
    // 远景 LOD 降级为"无内容"，叶与中层照常发布。
    ProductionProfile profile;
    profile.strictIdentity = true;              // 禁止聚合 fallback 兜底
    profile.lockSeamBorder = true;              // 开边界三角形删不动
    profile.budget.maxTrianglesPerTile = 150;
    profile.budget.targetTrianglesPerTile = 100;

    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "wb_pipeline_proxyskip";
    std::error_code ec;
    std::filesystem::remove_all(out, ec);

    ProductionPipeline pipeline(profile);
    ProductionPipeline::Options opt;
    opt.outputDir = out.string();
    // 64 个对象 × 4 quad × 2 三角形 = 512 个源三角形
    (void)pipeline.run(makeGrid(8, 8, 1.0, 4).get(), opt);
    const bool fin = pipeline.finalize();

    check(fin, "dataset still publishes when only far-LOD proxies are over budget",
          joinMessages(pipeline.failures()));
    check(pipeline.report().proxySkippedCount >= 1, "skipped-proxy counter is non-zero",
          std::to_string(pipeline.report().proxySkippedCount));

    std::set<FeatureId> leafScope;
    unsigned publishedContent = 0, skippedInternal = 0, overBudgetContent = 0;
    for (const ProductionTile& t : pipeline.tiles())
    {
        if (t.isLeaf) { leafScope.insert(t.sourceScope.begin(), t.sourceScope.end()); }
        if (t.hasContent)
        {
            ++publishedContent;
            if (t.triangles > profile.budget.maxTrianglesPerTile) { ++overBudgetContent; }
        }
        else if (!t.isLeaf && !t.children.empty() && !t.sourceScope.empty())
        {
            ++skippedInternal;
            // 超限文件必须已从产物里删掉：留一个"没人引用但确实超限"的瓦片
            // 同样违反手册 7.3
            const std::filesystem::path p = out / "tiles"
                / ("t" + std::to_string(t.id) + ".b3dm");
            check(!std::filesystem::exists(p, ec),
                  "over-budget proxy file removed from published output", p.string());
        }
    }
    check(overBudgetContent == 0, "no published tile exceeds the triangle hard limit",
          std::to_string(overBudgetContent));
    check(skippedInternal >= 1, "some internal nodes really degraded to content-less",
          std::to_string(skippedInternal));
    {
        bool explained = false;
        for (const std::string& w : pipeline.warnings())
        {
            if (w.find("carries no far-LOD content") != std::string::npos
                || w.find("over-budget far-LOD file removed") != std::string::npos)
            {
                explained = true;
                break;
            }
        }
        check(explained, "the degradation is explained in warnings, not silently dropped",
              std::to_string(pipeline.warnings().size()) + " warnings");
    }
    check(pipeline.report().featuresMissingFromLeaves == 0,
          "degrading a proxy loses no source feature");
    check(leafScope.size() == pipeline.report().sourceFeatureCount,
          "leaf tiles still cover the whole source registry",
          std::to_string(leafScope.size()) + "/"
          + std::to_string(pipeline.report().sourceFeatureCount));
    check(std::filesystem::exists(out / "tileset.json", ec), "tileset.json published");

    // 反向对照：把树深硬限压到 1 层——512 个源三角形在这个深度下无论如何
    // 都装不进预算，分区器只能在 maxTreeDepth 处报"仍然超硬预算"。
    // 此时必须失败关闭、一个瓦片产物都不留（手册 3 / 14.17）。
    ProductionProfile unsplittable = profile;
    unsplittable.budget.maxTreeDepth = 1;
    const std::filesystem::path out2 =
        std::filesystem::temp_directory_path() / "wb_pipeline_leaffailclosed";
    std::filesystem::remove_all(out2, ec);
    ProductionPipeline rejecting(unsplittable);
    ProductionPipeline::Options opt2;
    opt2.outputDir = out2.string();
    (void)rejecting.run(makeGrid(8, 8, 1.0, 4).get(), opt2);
    check(!rejecting.finalize(), "a dataset that cannot fit the budget at leaf level is a hard failure",
          joinMessages(rejecting.failures()));
    check(!std::filesystem::exists(out2 / "tileset.json", ec),
          "failed dataset leaves no tileset behind");
}


// ---------------------------------------------------------------------------
// A-36 / A-37：包围体发布闸（selfValidate 1b）。
// 这里验的是检查本身：非法包围盒必须拒绝发布，合法紧贴盒必须放行。
// 三轴都要有越界用例——第一版实现把半轴下标写成 b[4]/b[8]（盒布局其实是
// [cx,cy,cz, hx,0,0, 0,hy,0, 0,0,hz]，半轴在 3/7/11），Y/Z 轴的越界被读成
// 半轴 0 而静默放行，同时在真实数据上随时可能反过来误伤。
// ---------------------------------------------------------------------------
void testPipelineBoundsGate()
{
    std::cout << "[bounds] bounding-volume release gate" << std::endl;
    Utils::registerFileAliases();

    ProductionProfile profile;
    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "wb_pipeline_bounds";
    std::error_code ec;
    std::filesystem::remove_all(out, ec);

    ProductionPipeline pipeline(profile);
    ProductionPipeline::Options opt;
    opt.outputDir = out.string();
    // 只 run()，不 finalize()：finalize 会把暂存目录搬走，而 selfValidate 的
    // URI 检查需要暂存目录里真实存在的内容文件。
    const bool ran = pipeline.run(makeGrid(4, 4, 1.0).get(), opt);
    check(ran, "bounds gate fixture builds", joinMessages(pipeline.failures()));

    std::string uri;
    for (const ProductionTile& t : pipeline.tiles())
    {
        if (t.hasContent) { uri = t.uri; break; }
    }
    check(!uri.empty(), "fixture has a content tile to point the probe tileset at");

    auto box = [](double cx, double cy, double cz, double hx, double hy, double hz) {
        return nlohmann::json::array({cx, cy, cz,
                                      hx, 0.0, 0.0,
                                      0.0, hy, 0.0,
                                      0.0, 0.0, hz});
    };
    auto probe = [&pipeline](const nlohmann::json& root) -> std::vector<std::string>
    {
        nlohmann::json ts;
        ts["asset"]["version"] = "1.0";
        ts["root"] = root;
        const std::filesystem::path p =
            std::filesystem::temp_directory_path() / "wb_bounds_probe.json";
        std::ofstream f(p, std::ios::trunc);
        f << ts.dump();
        f.close();
        // 只统计包围体类失败：其它检查（URI、预算、覆盖）对本次探针同样会说话，
        // 混在一起就把"闸有没有生效"变成了"总共报了几条"。
        const std::size_t before = pipeline.failures().size();
        pipeline.selfValidate(p.string());
        std::vector<std::string> hits;
        for (std::size_t i = before; i < pipeline.failures().size(); ++i)
        {
            const std::string& m = pipeline.failures()[i];
            if (m.find("boundingVolume") != std::string::npos
                || m.find("child box not covered") != std::string::npos
                || m.find("geometricError is not") != std::string::npos)
            {
                hits.push_back(m);
            }
        }
        return hits;
    };
    auto leafRoot = [&uri](const nlohmann::json& b, double ge) {
        nlohmann::json t;
        t["boundingVolume"]["box"] = b;
        t["geometricError"] = ge;
        t["refine"] = "REPLACE";
        t["content"]["uri"] = uri;
        return t;
    };
    auto nestedRoot = [&box, &uri](const nlohmann::json& childBox) {
        nlohmann::json child;
        child["boundingVolume"]["box"] = childBox;
        child["geometricError"] = 5.0;
        child["refine"] = "REPLACE";
        nlohmann::json root;
        root["boundingVolume"]["box"] = box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
        root["geometricError"] = 100.0;
        root["refine"] = "REPLACE";
        root["content"]["uri"] = uri;
        root["children"] = nlohmann::json::array({ child });
        return root;
    };

    // 合法：三轴同时"紧贴"父盒内壁（|Δc| + hChild == hParent），
    // 容差取父半轴的 1e-6 相对值，所以等号必须放行。
    check(probe(nestedRoot(box(5.0, 4.0, 3.0, 5.0, 6.0, 7.0))).empty(),
          "child tightly inscribed in parent on all three axes is accepted");

    // 每一条轴单独越界都必须被抓住，并且报出正确的轴号。
    const double over[3][6] = {
        { 6.0, 0.0, 0.0, 5.0, 1.0, 1.0 },   // X：|6|+5 > 10
        { 0.0, 6.0, 0.0, 1.0, 5.0, 1.0 },   // Y：只有读 3/7/11 才会发现
        { 0.0, 0.0, 6.0, 1.0, 1.0, 5.0 },   // Z
    };
    for (int a = 0; a < 3; ++a)
    {
        const auto hits = probe(nestedRoot(box(over[a][0], over[a][1], over[a][2],
                                               over[a][3], over[a][4], over[a][5])));
        const bool rightAxis = hits.size() == 1
            && hits[0].find("on axis " + std::to_string(a)) != std::string::npos;
        check(rightAxis, "child overflowing parent on one axis is rejected on that axis",
              hits.empty() ? "no failure reported" : joinMessages(hits));
    }

    // 非法几何值
    check(!probe(leafRoot(box(0.0, 0.0, 0.0, -1.0, 10.0, 10.0), 10.0)).empty(),
          "negative half-axis is rejected");
    check(!probe(leafRoot(nlohmann::json::array({ 0.0, 0.0, 0.0,
                                                 nlohmann::json::value_t::null, 0.0, 0.0,
                                                 0.0, 10.0, 0.0, 0.0, 0.0, 10.0 }),
                          10.0)).empty(),
          "a box washed out of NaN (JSON null) is rejected");
    {
        // NaN/inf 半轴在 JSON 里没有合法字面量：nlohmann 把它们 dump 成 null，
        // 再解析回来就是"box 里混进非数字元素"。这条用例跑通整条洗白链——
        // 序列化必须把 NaN 暴露成 null，发布闸必须拒绝，而不是当 0 放行（A-36）。
        nlohmann::json b = nlohmann::json::array({ 0.0, 0.0, 0.0,
            std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0,
            0.0, 10.0, 0.0, 0.0, 0.0, 10.0 });
        const std::string text = b.dump();
        check(text.find("null") != std::string::npos,
              "a non-finite half-axis serializes out of JSON as null", text);
        nlohmann::json back;
        std::istringstream is(text);
        is >> back;
        check(!probe(leafRoot(back, 10.0)).empty(),
              "a NaN half-axis serialized as JSON null is rejected");
    }
    check(!probe(leafRoot(box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0), -1.0)).empty(),
          "negative geometricError is rejected");
    check(probe(leafRoot(box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0), 10.0)).empty(),
          "a plain valid tile reports no bounding-volume failure");
}

// ---------------------------------------------------------------------------
// A-41 回归：i3dm 的**每实例位姿**必须逐条落进二进制特征表。
// 过去的失效方式很隐蔽：N 个 MatrixTransform 共享同一份原型 drawable，折变换优化
// 用 TransformAttributeFunctor **就地**把 N 个矩阵连乘到这份共享数组上，再把矩阵复位
// 成单位阵，于是 `createFeatureI3DMTableBinary` 只能 decompose 出全 0 位姿——
// 容器仍是合法 i3dm、validator 0 error、画面也有几何，只有实例位置是空的。
// 所以本用例断言的是三件当时**同时**成立才会通过的事：
//   ① 特征表 POSITION 逐条等于 inverse(原型世界) · 实例世界 的平移（Y-up 轴序）
//   ② GLB 里的原型几何仍只在它自己的局部尺度内（没有被连乘搬走）
//   ③ GLB 节点不带 TRS（位姿只能在特征表里，否则渲染时双变换）
// ①是后来才补上的：原型几何已按世界矩阵烘好，特征表若直接写实例世界矩阵，
// 就等于把世界变换乘两遍——位姿看着齐全（299/299 非零）却整体偏移一个原型平移，
// 只有拿"相对原型"的数值去比才暴露得出来。
namespace {

osg::ref_ptr<osg::Geode> makeIdenticalQuadGeode(const std::string& name)
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->setName(name);
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
    // 每个 Geode 各自一份数组、数值完全相同：这就是签名分组要认的"重复实例"
    pos->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
    pos->push_back(osg::Vec3(1.0f, 0.0f, 0.0f));
    pos->push_back(osg::Vec3(1.0f, 1.0f, 0.0f));
    pos->push_back(osg::Vec3(0.0f, 1.0f, 0.0f));
    osg::ref_ptr<osg::DrawElementsUInt> idx =
        new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
    idx->push_back(0); idx->push_back(1); idx->push_back(2);
    idx->push_back(0); idx->push_back(2); idx->push_back(3);
    geom->setVertexArray(pos.get());
    geom->addPrimitiveSet(idx.get());
    geode->addDrawable(geom.get());
    return geode;
}

std::uint32_t readU32(const std::vector<char>& b, std::size_t at)
{
    std::uint32_t v = 0;
    std::memcpy(&v, b.data() + at, sizeof(v));
    return v;
}

float readF32(const std::vector<char>& b, std::size_t at)
{
    float v = 0.0f;
    std::memcpy(&v, b.data() + at, sizeof(v));
    return v;
}

struct I3dmParsed
{
    bool ok = false;
    nlohmann::json ft;          // 特征表 JSON
    std::size_t ftBin = 0;      // 二进制特征表起点
    nlohmann::json glb;         // GLB 的 asset JSON
    std::size_t glbBin = 0;     // GLB BIN chunk 起点
};

I3dmParsed parseI3dm(const std::vector<char>& b)
{
    I3dmParsed r;
    if (b.size() < 32 || std::memcmp(b.data(), "i3dm", 4) != 0) { return r; }
    // i3dm 头 32 B：ftj@12 ftb@16 btj@20 btb@24 gltfFormat@28（b3dm 是 28 B 头）
    const std::size_t ftj = readU32(b, 12), ftb = readU32(b, 16);
    const std::size_t btj = readU32(b, 20), btb = readU32(b, 24);
    r.ft = nlohmann::json::parse(b.data() + 32, b.data() + 32 + ftj);
    r.ftBin = 32 + ftj;
    const std::size_t glbOff = 32 + ftj + ftb + btj + btb;
    if (glbOff + 20 > b.size() || std::memcmp(b.data() + glbOff, "glTF", 4) != 0) { return r; }
    const std::size_t jsonLen = readU32(b, glbOff + 12);
    r.glb = nlohmann::json::parse(b.data() + glbOff + 20, b.data() + glbOff + 20 + jsonLen);
    r.glbBin = glbOff + 12 + 8 + jsonLen + 8;
    r.ok = true;
    return r;
}

} // namespace

void testInstancePosesSurvive()
{
    std::cout << "[i3dm] per-instance poses survive write-out" << std::endl;
    Utils::registerFileAliases();

    // 平移三轴都不为零且互不相同：任何"连乘"或"丢位姿"都会改变这个集合。
    // proto 故意也带一个非零平移，用来验证特征表里的矩阵是**相对原型**的。
    const osg::Vec3d off[5] = {
        osg::Vec3d(1.0, 2.0, 3.0),    // 原型
        osg::Vec3d(10.0, 3.0, -4.0),
        osg::Vec3d(-7.0, 2.5, 6.0),
        osg::Vec3d(100.0, -11.0, 13.0),
        osg::Vec3d(-30.0, 40.0, -50.0),
    };
    const unsigned kInst = 4;   // 除原型以外的实例数

    osg::ref_ptr<osg::Group> root = new osg::Group;
    for (unsigned i = 0; i < 5; ++i)
    {
        osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
        mt->setMatrix(osg::Matrixd::translate(off[i]));
        mt->addChild(makeIdenticalQuadGeode("lamp_" + std::to_string(i)).get());
        root->addChild(mt.get());
    }

    ProductionProfile profile;
    profile.budget.maxTrianglesPerTile = 12;
    profile.budget.targetTrianglesPerTile = 8;
    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "wb_pipeline_instance_poses";
    std::error_code ec;
    std::filesystem::remove_all(out, ec);

    ProductionPipeline pipeline(profile);
    ProductionPipeline::Options opt;
    opt.outputDir = out.string();
    const bool ran = pipeline.run(root.get(), opt);
    const bool fin = pipeline.finalize();
    check(ran && fin, "instanced fixture builds and publishes",
          joinMessages(pipeline.failures()));
    check(pipeline.report().instanceTileCount == 1,
          "exactly one i3dm tile published",
          std::to_string(pipeline.report().instanceTileCount));
    check(pipeline.report().instanceCount == kInst,
          "the 4 non-prototype copies are registered as instances",
          std::to_string(pipeline.report().instanceCount));

    std::filesystem::path tile;
    for (const auto& e : std::filesystem::directory_iterator(out / "tiles", ec))
    {
        if (e.path().extension() == ".i3dm") { tile = e.path(); }
    }
    check(!tile.empty(), "an .i3dm content file exists on disk", tile.string());
    if (tile.empty()) { return; }

    std::ifstream in(tile, std::ios::binary);
    const std::vector<char> buf((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    const I3dmParsed r = parseI3dm(buf);
    check(r.ok, "content file parses as i3dm with a GLB");
    if (!r.ok) { return; }

    check(r.ft.contains("INSTANCES_LENGTH") && r.ft["INSTANCES_LENGTH"] == kInst,
          "feature table declares every instance", r.ft.dump());
    check(r.ft.contains("POSITION") && r.ft["POSITION"].contains("byteOffset"),
          "feature table carries a POSITION column", r.ft.dump());
    if (!r.ft.contains("POSITION")) { return; }

    const std::size_t po = r.ft["POSITION"]["byteOffset"];
    unsigned nz = 0, exact = 0;
    for (unsigned i = 0; i < kInst; ++i)
    {
        const std::size_t at = r.ftBin + po + static_cast<std::size_t>(i) * 12;
        const osg::Vec3d rel = off[i + 1] - off[0];
        // OSG→glTF 的 Y-up 轴序换算：(x, y, z) -> (x, -z, y)
        const double want[3] = { rel.x(), -rel.z(), rel.y() };
        double got[3] = { want[0] - 1e9, want[1] - 1e9, want[2] - 1e9 };
        double maxAbs = 0.0;
        for (int c = 0; c < 3; ++c)
        {
            got[c] = readF32(buf, at + static_cast<std::size_t>(c) * 4);
            maxAbs = std::max(maxAbs, std::abs(got[c]));
        }
        if (maxAbs > 0.0f) { ++nz; }
        if (std::abs(got[0] - want[0]) < 1e-3 && std::abs(got[1] - want[1]) < 1e-3
            && std::abs(got[2] - want[2]) < 1e-3)
        { ++exact; }
        else
        {
            std::cout << "  instance " << i << " got (" << got[0] << "," << got[1] << ","
                      << got[2] << ") want (" << want[0] << "," << want[1] << ","
                      << want[2] << ")" << std::endl;
        }
    }
    check(nz == kInst, "no instance position collapsed to zero",
          std::to_string(nz) + "/" + std::to_string(kInst));
    check(exact == kInst, "every instance position equals inverse(prototype) * instance",
          std::to_string(exact) + "/" + std::to_string(kInst));

    // ② 原型几何只能待在自己的尺度里。这里的顶点已按原型世界矩阵烘好（平移
    //    (1,2,3)，四边形占 1×1），所以任何一轴的量级都不该超过 3；被 4 个实例
    //    矩阵就地连乘时会平移到 |v|≈72（纯平移不改跨度，所以必须看绝对位置）。
    double geoMaxAbs = 0.0;
    for (const auto& mesh : r.glb.value("meshes", nlohmann::json::array()))
        for (const auto& prim : mesh.value("primitives", nlohmann::json::array()))
        {
            const int ai = prim.value("attributes", nlohmann::json::object())
                               .value("POSITION", -1);
            if (ai < 0 || !r.glb.contains("accessors")) { continue; }
            const nlohmann::json& acc = r.glb["accessors"][ai];
            for (const double v : acc.value("min", nlohmann::json::array()))
            { geoMaxAbs = std::max(geoMaxAbs, std::abs(v)); }
            for (const double v : acc.value("max", nlohmann::json::array()))
            { geoMaxAbs = std::max(geoMaxAbs, std::abs(v)); }
        }
    check(geoMaxAbs > 0.0 && geoMaxAbs <= 3.0 + 1e-6,
          "prototype geometry stays at its own scale (not multiplied by instance matrices)",
          "maxAbs=" + std::to_string(geoMaxAbs));

    // ③ 位姿只能有一份：GLB 节点带 TRS 就会与特征表双变换
    unsigned trsNodes = 0;
    for (const auto& nd : r.glb.value("nodes", nlohmann::json::array()))
    {
        if (nd.contains("translation") || nd.contains("rotation")
            || nd.contains("scale") || nd.contains("matrix")) { ++trsNodes; }
    }
    check(trsNodes == 0, "no per-node TRS competes with the feature table",
          std::to_string(trsNodes));
}

// ---------------------------------------------------------------------------
// A-42 回归：产物里不得出现本地绝对路径。两个泄漏口都是"路径当名字"：
//   ① 纹理写进 <out>/textures 后，Osg2Gltf 把**完整输出路径**写进 GLB 的
//      bufferView.name / image.name —— 这是随瓦片**分发出去**的；
//   ② FBX 导入器用源文件绝对路径命名根节点，它顺着 nodePath 进 feature-manifest.json。
// ①的另一种伤害是瓦片字节数随输出目录长度变化，跨目录的哈希根本没法比。
// 这里只查 JSON 字符串字面量形态（"D:/…）：二进制 float 数据里裸的 X:/ 会大量误报，
// 那种命中不能当证据（t03 曾被误判成 7 个瓦片泄漏）。
namespace {

bool hasQuotedAbsolutePath(const std::string& s)
{
    for (std::size_t i = 1; i + 3 < s.size(); ++i)
    {
        if (s[i] != '"') { continue; }
        const char a = s[i + 1];
        if (!((a >= 'A' && a <= 'Z') || (a >= 'a' && a <= 'z'))) { continue; }
        if (s[i + 2] != ':') { continue; }
        if (s[i + 3] == '/' || s[i + 3] == '\\') { return true; }
    }
    return false;
}

std::string readAll(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

void testArtifactsCarryNoLocalPaths()
{
    std::cout << "[paths] published artifacts carry no absolute local paths" << std::endl;
    Utils::registerFileAliases();

    // 探测器自检：正例必须能被认出——否则"0 泄漏"可能只是谓词写坏了恒返回 false。
    check(hasQuotedAbsolutePath(R"({"nodePath":"D:/secret_dir/a.fbx/x"})"),
          "detector fires on a leaked posix-style path");
    check(hasQuotedAbsolutePath(R"({"name":"E:\\dev\\x\\a.fbx"})"),
          "detector fires on a leaked windows-style path");
    check(!hasQuotedAbsolutePath(R"({"uri":"tiles/t0.b3dm","v":"1.2:/3"})"),
          "detector ignores ordinary uri/numeric text");

    // 根节点名故意用绝对路径：修复前它会原样进 manifest 的 nodePath。
    osg::ref_ptr<osg::Group> root = new osg::Group;
    root->setName("D:/secret_dir/asset_name.fbx");
    const osg::Vec3d pos[3] = { osg::Vec3d(0.0, 0.0, 0.0),
                                osg::Vec3d(7.0, 0.0, 0.0),
                                osg::Vec3d(0.0, 9.0, 0.0) };
    for (unsigned i = 0; i < 3; ++i)
    {
        osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
        mt->setMatrix(osg::Matrixd::translate(pos[i]));
        mt->addChild(makeIdenticalQuadGeode("sign_" + std::to_string(i)).get());
        root->addChild(mt.get());
    }

    ProductionProfile profile;
    profile.budget.maxTrianglesPerTile = 12;
    profile.budget.targetTrianglesPerTile = 8;
    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "wb_pipeline_no_local_paths";
    std::error_code ec;
    std::filesystem::remove_all(out, ec);

    ProductionPipeline pipeline(profile);
    ProductionPipeline::Options opt;
    opt.outputDir = out.string();
    const bool ran = pipeline.run(root.get(), opt);
    const bool fin = pipeline.finalize();
    check(ran && fin, "fixture publishes", joinMessages(pipeline.failures()));
    if (!fin) { return; }

    const std::filesystem::path manifest = out / "feature-manifest.json";
    check(std::filesystem::exists(manifest), "feature-manifest.json exists", manifest.string());
    const std::string mf = readAll(manifest);
    check(!hasQuotedAbsolutePath(mf),
          "manifest leaks no absolute local path (nodePath keeps only the asset name)",
          mf.substr(0, 240));
    check(mf.find("asset_name.fbx") != std::string::npos,
          "the asset name itself is still recorded, not silently dropped", mf.substr(0, 240));

    unsigned tiles = 0, leaked = 0;
    std::string firstLeak;
    for (const auto& e : std::filesystem::directory_iterator(out / "tiles", ec))
    {
        if (!e.is_regular_file()) { continue; }
        const std::string ext = e.path().extension().string();
        if (ext != ".b3dm" && ext != ".i3dm" && ext != ".pnts" && ext != ".cmpt") { continue; }
        ++tiles;
        const std::string body = readAll(e.path());
        if (hasQuotedAbsolutePath(body))
        {
            ++leaked;
            if (firstLeak.empty()) { firstLeak = e.path().filename().string(); }
        }
    }
    check(tiles > 0, "tiles were written to scan", std::to_string(tiles));
    check(leaked == 0, "no tile embeds an absolute local path",
          std::to_string(leaked) + " leaked, first=" + firstLeak);
}

// ---------------------------------------------------------------------------
// A-51 方案 A：代理写出把图集子矩形 KHR_texture_transform 折进 TEXCOORD_0。
// 折叠改的是贴图坐标，属质量敏感路径，所以这里把"什么时候许折、什么时候必须
// 不折"逐条钉住；合并效果本身由下面的预算闸 + 产物对照负责。
// ---------------------------------------------------------------------------
namespace {

osg::ref_ptr<osg::Geometry> makeTexturedQuad(const std::vector<osg::Vec2>& uvs)
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
    pos->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
    pos->push_back(osg::Vec3(1.0f, 0.0f, 0.0f));
    pos->push_back(osg::Vec3(1.0f, 1.0f, 0.0f));
    pos->push_back(osg::Vec3(0.0f, 1.0f, 0.0f));
    // osg::Vec2Array 的析构是 protected，测试里只能经 ref_ptr 在堆上建；
    // 源数据用 std::vector 保存，方便按值比对而不用碰数组对象本身。
    osg::ref_ptr<osg::Vec2Array> tc = new osg::Vec2Array;
    tc->reserve(static_cast<osg::Vec2Array::size_type>(uvs.size()));
    for (const osg::Vec2& uv : uvs) { tc->push_back(uv); }
    osg::ref_ptr<osg::DrawElementsUInt> idx =
        new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
    idx->push_back(0); idx->push_back(1); idx->push_back(2);
    idx->push_back(0); idx->push_back(2); idx->push_back(3);
    geom->setVertexArray(pos.get());
    geom->setTexCoordArray(0, tc.get());
    geom->addPrimitiveSet(idx.get());
    return geom;
}

osg::ref_ptr<osg::Texture2D> makeAtlasPage()
{
    osg::ref_ptr<osg::Image> img = new osg::Image;
    img->allocateImage(4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE);
    return new osg::Texture2D(img.get());
}

// 图集打包写在宿主（贴图或 GltfMaterial）上的那组键；值类型必须与写手读的一致。
void setSlotTransform(osg::Object* holder, const char* enableKey, const char* oxKey,
                      const char* oyKey, const char* sxKey, const char* syKey,
                      const char* tcKey, double ox, double oy, double sx, double sy,
                      int texCoord)
{
    holder->setUserValue(enableKey, true);
    holder->setUserValue(oxKey, ox);
    holder->setUserValue(oyKey, oy);
    holder->setUserValue(sxKey, sx);
    holder->setUserValue(syKey, sy);
    holder->setUserValue(tcKey, texCoord);
}

void setBasecolorRect(osg::Object* holder, double ox, double oy, double sx, double sy,
                      int texCoord = 0)
{
    setSlotTransform(holder, TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME,
                     TEX_TRANSFORM_BASECOLOR_OFFSET_X, TEX_TRANSFORM_BASECOLOR_OFFSET_Y,
                     TEX_TRANSFORM_BASECOLOR_SCALE_X, TEX_TRANSFORM_BASECOLOR_SCALE_Y,
                     TEX_TRANSFORM_BASECOLOR_TEXCOORD, ox, oy, sx, sy, texCoord);
}

bool slotEnabled(osg::Object* holder, const char* enableKey)
{
    bool on = false;
    holder->getUserValue(enableKey, on);
    return on;
}

osg::ref_ptr<osg::StateSet> texturedStateSet(osg::Texture2D* tex)
{
    osg::ref_ptr<osg::StateSet> ss = new osg::StateSet;
    ss->setTextureAttributeAndModes(0, tex, osg::StateAttribute::ON);
    return ss;
}

// 折叠前后必须采到同一个纹素。写手对任何变换都会再乘一次 Y 翻转
//（flipGltfTextureYAxis：offsetY=1-offsetY、scaleY=-scaleY），
// glTF 侧最终坐标 = scale'·uv + offset'。这里把两条路径分别算出来对比，
// 钉住的是"折进 UV 用的必须是变换原值"这条约定。
bool samplesSameTexel(const osg::Vec2& originalUv, const osg::Vec2& foldedUv,
                      double ox, double oy, double sx, double sy)
{
    const double unfoldedU = sx * originalUv.x() + ox;
    const double unfoldedV = (1.0 - oy) - sy * originalUv.y();
    const double foldedU = 1.0 * foldedUv.x() + 0.0;
    const double foldedV = 1.0 - 1.0 * foldedUv.y();
    return std::abs(unfoldedU - foldedU) < 1e-6 && std::abs(unfoldedV - foldedV) < 1e-6;
}

} // namespace

void testFoldTextureTransformIntoUv()
{
    std::cout << "[foldUv] atlas sub-rect folded into TEXCOORD_0" << std::endl;

    std::vector<osg::Vec2> uvs;
    uvs.push_back(osg::Vec2(0.0f, 0.0f));
    uvs.push_back(osg::Vec2(1.0f, 0.0f));
    uvs.push_back(osg::Vec2(1.0f, 1.0f));
    uvs.push_back(osg::Vec2(0.0f, 1.0f));

    // ① 两个 drawable 共用同一张图集页、子矩形不同：折完材质逐值相同才能合并
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> a = makeTexturedQuad(uvs);
    osg::ref_ptr<osg::Geometry> b = makeTexturedQuad(uvs);
    osg::ref_ptr<osg::Texture2D> texA = makeAtlasPage();
    osg::ref_ptr<osg::Texture2D> texB = makeAtlasPage();
    setBasecolorRect(texA.get(), 0.25, 0.50, 0.50, 0.25);
    setBasecolorRect(texB.get(), 0.00, 0.00, 0.50, 0.25);
    osg::ref_ptr<osg::StateSet> ssA = texturedStateSet(texA.get());
    osg::ref_ptr<osg::StateSet> ssB = texturedStateSet(texB.get());
    a->setStateSet(ssA.get());
    b->setStateSet(ssB.get());
    geode->addDrawable(a.get());
    geode->addDrawable(b.get());

    osg::ref_ptr<osg::Vec2Array> origA =
        dynamic_cast<osg::Vec2Array*>(a->getTexCoordArray(0));
    check(slotEnabled(texA.get(), TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME),
          "premise: transform is enabled before folding (positive control)");

    GltfOptimizer::TextureUvBackup backup;
    const unsigned folded = GltfOptimizer::foldTextureTransformsIntoUv(geode.get(), backup);
    check(folded == 2, "both sub-rects folded", std::to_string(folded));
    check(!slotEnabled(texA.get(), TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME)
          && !slotEnabled(texB.get(), TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME),
          "transform keys cleared so the writer emits one identical material");

    const osg::Vec2Array* bakedA =
        dynamic_cast<const osg::Vec2Array*>(a->getTexCoordArray(0));
    check(bakedA != nullptr && bakedA != origA.get(),
          "geometry switches to a fresh array (never resizes the possibly-shared one)");
    bool mathOk = bakedA != nullptr && bakedA->size() == 4;
    for (int i = 0; mathOk && i < 4; ++i)
    {
        mathOk = std::abs((*bakedA)[i].x() - float(0.50 * uvs[i].x() + 0.25)) < 1e-6
              && std::abs((*bakedA)[i].y() - float(0.25 * uvs[i].y() + 0.50)) < 1e-6;
    }
    check(mathOk, "baked UV == sx*u+ox, sy*v+oy (OSG-space transform, pre-flip)");
    check(origA.valid() && (*origA)[2].x() == 1.0f && (*origA)[2].y() == 1.0f,
          "original array values survive untouched");
    bool sameTexel = bakedA != nullptr;
    for (int i = 0; sameTexel && i < 4; ++i)
    {
        sameTexel = samplesSameTexel(uvs[i], (*bakedA)[i], 0.25, 0.50, 0.50, 0.25);
    }
    check(sameTexel, "folded sampling hits the same texel as the unfolded transform");

    std::vector<osg::Vec2> firstBaked;
    if (bakedA != nullptr)
    {
        for (const osg::Vec2& uv : *bakedA) { firstBaked.push_back(uv); }
    }

    // ② 还原往返：原始数组指针与数值都回来，备份清空
    const unsigned restored = GltfOptimizer::restoreFoldedTextureUvs(backup);
    check(restored == 2, "restore puts both arrays back", std::to_string(restored));
    check(a->getTexCoordArray(0) == origA.get(), "restored array is the original object");
    check(backup.empty(), "restore empties the backup (no stale pointer for the next tile)");

    // ③ 契约：fold 起手先整体还原，所以连续两次 fold 的烘焙永远建立在原始 UV 上，
    //    变换只作用一次。反例（不还原直接再折）会把 u 折成 0.25u+0.375，
    //    与第一次的 0.50u+0.25 不同，这条检查就会红。
    const unsigned second = GltfOptimizer::foldTextureTransformsIntoUv(geode.get(), backup);
    check(second == 2, "a second fold re-derives both sub-rects from the original UVs",
          std::to_string(second));
    const osg::Vec2Array* rebaked =
        dynamic_cast<const osg::Vec2Array*>(a->getTexCoordArray(0));
    bool idempotent = rebaked != nullptr && rebaked->size() == firstBaked.size();
    for (std::size_t i = 0; idempotent && i < firstBaked.size(); ++i)
    {
        idempotent = std::abs((*rebaked)[i].x() - firstBaked[i].x()) < 1.0e-6f
                  && std::abs((*rebaked)[i].y() - firstBaked[i].y()) < 1.0e-6f;
    }
    check(idempotent, "two folds give the same UVs (the transform never stacks)");
    check(rebaked != nullptr && rebaked != origA.get(),
          "the second fold also writes a fresh array, never the possibly-shared original");

    // ④ 无变换的 drawable 一个字节都不许动（阴性对照）
    osg::ref_ptr<osg::Geode> plainGeode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> plain = makeTexturedQuad(uvs);
    osg::ref_ptr<osg::Texture2D> texP = makeAtlasPage();
    osg::ref_ptr<osg::StateSet> ssP = texturedStateSet(texP.get());
    plain->setStateSet(ssP.get());
    plainGeode->addDrawable(plain.get());
    osg::Array* plainUv = plain->getTexCoordArray(0);
    GltfOptimizer::TextureUvBackup backupP;
    check(GltfOptimizer::foldTextureTransformsIntoUv(plainGeode.get(), backupP) == 0
          && plain->getTexCoordArray(0) == plainUv,
          "drawable without a transform is left alone");

    // ⑤ 共享贴图 + 一个 TEXCOORD_0 不是 Vec2Array 的兄弟：谁都不许折，
    //    否则清掉共享键会让那个兄弟按原始 UV 采到图集的错误子矩形
    osg::ref_ptr<osg::Geode> shared = new osg::Geode;
    osg::ref_ptr<osg::Geometry> s1 = makeTexturedQuad(uvs);
    osg::ref_ptr<osg::Geometry> s2 = makeTexturedQuad(uvs);
    osg::ref_ptr<osg::Vec3Array> s2uv = new osg::Vec3Array;
    s2uv->push_back(osg::Vec3(0.0f, 0.0f, 0.0f));
    s2uv->push_back(osg::Vec3(1.0f, 0.0f, 0.0f));
    s2uv->push_back(osg::Vec3(1.0f, 1.0f, 0.0f));
    s2uv->push_back(osg::Vec3(0.0f, 1.0f, 0.0f));
    s2->setTexCoordArray(0, s2uv.get());
    osg::ref_ptr<osg::Texture2D> texS = makeAtlasPage();
    setBasecolorRect(texS.get(), 0.25, 0.25, 0.50, 0.50);
    osg::ref_ptr<osg::StateSet> ssS = texturedStateSet(texS.get());
    s1->setStateSet(ssS.get());
    s2->setStateSet(ssS.get());
    shared->addDrawable(s1.get());
    shared->addDrawable(s2.get());
    osg::Array* s1Uv = s1->getTexCoordArray(0);
    GltfOptimizer::TextureUvBackup backupS;
    check(GltfOptimizer::foldTextureTransformsIntoUv(shared.get(), backupS) == 0,
          "a sibling that cannot fold blocks the shared holder");
    check(slotEnabled(texS.get(), TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME),
          "shared holder keeps its key (clearing it would mis-sample the sibling)");
    check(s1->getTexCoordArray(0) == s1Uv, "blocked candidate leaves UVs untouched");

    // ⑥ 同一 drawable 上两个槽变换不一致：保守不折（折哪个都是错的）
    osg::ref_ptr<osg::Geode> twoSlot = new osg::Geode;
    osg::ref_ptr<osg::Geometry> g6 = makeTexturedQuad(uvs);
    osg::ref_ptr<osg::StateSet> ss6 = new osg::StateSet;
    osg::ref_ptr<GltfPbrMRMaterial> mr = new GltfPbrMRMaterial;
    mr->baseColorTexture = makeAtlasPage();
    mr->normalTexture = makeAtlasPage();
    setBasecolorRect(mr.get(), 0.25, 0.25, 0.50, 0.50);
    setSlotTransform(mr.get(), TEX_TRANSFORM_NORMAL_TEXTURE_NAME,
                     TEX_TRANSFORM_NORMAL_OFFSET_X, TEX_TRANSFORM_NORMAL_OFFSET_Y,
                     TEX_TRANSFORM_NORMAL_SCALE_X, TEX_TRANSFORM_NORMAL_SCALE_Y,
                     TEX_TRANSFORM_NORMAL_TEXCOORD, 0.00, 0.50, 0.50, 0.25, 0);
    ss6->setAttributeAndModes(mr.get(), osg::StateAttribute::ON);
    g6->setStateSet(ss6.get());
    twoSlot->addDrawable(g6.get());
    osg::Array* uv6 = g6->getTexCoordArray(0);
    GltfOptimizer::TextureUvBackup backup6;
    check(GltfOptimizer::foldTextureTransformsIntoUv(twoSlot.get(), backup6) == 0,
          "two enabled slots with different transforms refuse to fold");
    check(slotEnabled(mr.get(), TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME)
          && slotEnabled(mr.get(), TEX_TRANSFORM_NORMAL_TEXTURE_NAME),
          "both keys stay enabled when the fold is refused");
    check(g6->getTexCoordArray(0) == uv6, "refused fold keeps the UV array");

    // ⑦ texCoord != 0：折进 0 号通道会改变采样，必须不折
    osg::ref_ptr<osg::Geode> tcGeode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> g7 = makeTexturedQuad(uvs);
    osg::ref_ptr<osg::Texture2D> tex7 = makeAtlasPage();
    setBasecolorRect(tex7.get(), 0.0, 0.0, 0.50, 0.50, /*texCoord=*/1);
    osg::ref_ptr<osg::StateSet> ss7 = texturedStateSet(tex7.get());
    g7->setStateSet(ss7.get());
    tcGeode->addDrawable(g7.get());
    GltfOptimizer::TextureUvBackup backup7;
    check(GltfOptimizer::foldTextureTransformsIntoUv(tcGeode.get(), backup7) == 0
          && slotEnabled(tex7.get(), TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME),
          "transform on UV set 1 is not folded into TEXCOORD_0");

    // ⑧ 恒等变换：不动 UV，只清键（材质值不变，但少一次材质分裂）。
    //    清键本身就是对节点的改动，所以条目仍进备份，让"fold 之前 = fold 之后"
    //    这条契约成立（还原会把键装回去）。
    osg::ref_ptr<osg::Geode> idGeode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> g8 = makeTexturedQuad(uvs);
    osg::ref_ptr<osg::Texture2D> tex8 = makeAtlasPage();
    setBasecolorRect(tex8.get(), 0.0, 0.0, 1.0, 1.0);
    osg::ref_ptr<osg::StateSet> ss8 = texturedStateSet(tex8.get());
    g8->setStateSet(ss8.get());
    idGeode->addDrawable(g8.get());
    osg::Array* uv8 = g8->getTexCoordArray(0);
    GltfOptimizer::TextureUvBackup backup8;
    check(GltfOptimizer::foldTextureTransformsIntoUv(idGeode.get(), backup8) == 1,
          "identity transform folds by clearing the key only");
    check(g8->getTexCoordArray(0) == uv8, "identity fold writes no UV");
    check(!slotEnabled(tex8.get(), TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME),
          "identity fold clears the key");
    check(backup8.size() == 1, "identity fold is still undoable", std::to_string(backup8.size()));
    GltfOptimizer::restoreFoldedTextureUvs(backup8);
    check(slotEnabled(tex8.get(), TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME),
          "restore puts the identity key back (fold leaves the node byte-identical)");
}

// ---------------------------------------------------------------------------
// 手册 7.3 的 primitive / 材质预算闸（A-32/A-33 同族：声明了预算却没有闸）。
// 判据必须读**产物**：最终 primitive 数由写出侧 GltfMerger::mergeMeshes() 决定，
// 在 OSG 节点上数 drawable 得到的是另一个数。
// ---------------------------------------------------------------------------
namespace {

void putU32(std::vector<char>& b, std::size_t at, std::uint32_t v)
{
    std::memcpy(b.data() + at, &v, sizeof(v));
}

// 手工拼一个最小 b3dm：28B 头 + "{}  " 特征表 + GLB（只有 JSON chunk）。
std::vector<char> makeFakeB3dm(unsigned primitives, unsigned materials)
{
    nlohmann::json gltf;
    gltf["asset"]["version"] = "2.0";
    nlohmann::json prims = nlohmann::json::array();
    for (unsigned i = 0; i < primitives; ++i)
    {
        prims.push_back({ { "attributes", { { "POSITION", 0 } } } });
    }
    gltf["meshes"] = nlohmann::json::array({ { { "primitives", prims } } });
    nlohmann::json mats = nlohmann::json::array();
    for (unsigned i = 0; i < materials; ++i) { mats.push_back(nlohmann::json::object()); }
    gltf["materials"] = mats;

    std::string jsonText = gltf.dump();
    while (jsonText.size() % 4 != 0) { jsonText += ' '; }
    const std::uint32_t jsonLen = static_cast<std::uint32_t>(jsonText.size());
    const std::size_t glbLen = 12 + 8 + jsonLen;
    const std::size_t ftj = 4;
    const std::uint32_t total = static_cast<std::uint32_t>(28 + ftj + glbLen);

    std::vector<char> out(total, '\0');
    std::memcpy(out.data(), "b3dm", 4);
    putU32(out, 4, 1);                 // version
    putU32(out, 8, total);
    putU32(out, 12, static_cast<std::uint32_t>(ftj));
    putU32(out, 16, 0);                // featureTableBinary
    putU32(out, 20, 0);                // batchTableJson
    putU32(out, 24, 0);                // batchTableBinary
    std::memcpy(out.data() + 28, "{}  ", 4);
    const std::size_t g = 28 + ftj;
    std::memcpy(out.data() + g, "glTF", 4);
    putU32(out, g + 4, 2);
    putU32(out, g + 8, static_cast<std::uint32_t>(glbLen));
    putU32(out, g + 12, jsonLen);
    std::memcpy(out.data() + g + 16, "JSON", 4);
    std::memcpy(out.data() + g + 20, jsonText.data(), jsonText.size());
    return out;
}

} // namespace

// A-52 回归：图集画布的**未被子矩形覆盖的缝隙**必须是 0。
// TexturePacker::pack 用 osg::Image::allocateImage 建画布，而 OSG 只 new 缓冲区不清零；
// 两张源图很少铺满画布，缝隙里留着的就是上一次堆使用的字节。页名（computeImageHash）
// 和 KTX2 都按整张画布算，于是同一份输入两次运行会产出不同的图集页。
//
// 污染堆是这条用例的全部难点：小画布直接向系统要新虚拟页时天然全 0，去掉清零也照样通过
// （2026-10-21 实测：只写"分配前塞一个 1 MiB 脏 vector"的版本，负对照 10/0 空过）。
// 所以先用第一轮探出画布的真实字节数，再把同尺寸的堆块反复污染成 0xFF 后释放，
// 逼 allocator 把脏块回收给后续画布；多轮取和，降低 LFH 挑块的偶然性。
void testAtlasCanvasZeroed()
{
    std::cout << "[atlasCanvas] uncovered atlas pixels must be zero (A-52)" << std::endl;

    auto solid = [](int w, int h, unsigned char v) -> osg::ref_ptr<osg::Image> {
        osg::ref_ptr<osg::Image> im = new osg::Image;
        im->allocateImage(w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE);
        std::memset(im->data(), v, im->getImageSizeInBytes());
        return im;
    };
    // 70x50 + 60x40：必然留出 (x>=70, y<50) 之类的缝隙
    osg::ref_ptr<osg::Image> a = solid(70, 50, 0xAA);
    osg::ref_ptr<osg::Image> b = solid(60, 40, 0x55);

    struct Pack
    {
        osg::ref_ptr<osg::Image> atlas;
        size_t numPacked = 0;
        double ax = 0, ay = 0, bx = 0, by = 0;
        int aw = 0, ah = 0, bw = 0, bh = 0;
    };
    auto doPack = [&a, &b]() -> Pack {
        Pack p;
        TexturePacker packer(256, 256);
        const size_t idA = packer.addElement(a);
        const size_t idB = packer.addElement(b);
        p.atlas = packer.pack(p.numPacked, true, true);
        if (p.atlas.valid())
        {
            packer.getPackingData(idA, p.ax, p.ay, p.aw, p.ah);
            packer.getPackingData(idB, p.bx, p.by, p.bw, p.bh);
        }
        return p;
    };

    auto inside = [](int x, int y, double rx, double ry, int rw, int rh) {
        return x >= static_cast<int>(rx) && x < static_cast<int>(rx) + rw
            && y >= static_cast<int>(ry) && y < static_cast<int>(ry) + rh;
    };

    // 缝隙扫描：gaps=缝隙像素数，badGap=其中非零的字节所在像素数
    int gaps = 0, badGap = 0, badA = 0, badB = 0;
    auto scan = [&inside](const Pack& p, int& g, int& bg, int& aErr, int& bErr) {
        const unsigned char* base = p.atlas->data();
        const int s = p.atlas->s(), t = p.atlas->t();
        const int rowStep = static_cast<int>(p.atlas->getRowStepInBytes());
        const int pix = static_cast<int>(p.atlas->getPixelSizeInBits() / 8);
        for (int y = 0; y < t; ++y)
        {
            const unsigned char* row = base + static_cast<size_t>(y) * rowStep;
            for (int x = 0; x < s; ++x)
            {
                const unsigned char* px = row + static_cast<size_t>(x) * pix;
                const bool inA = inside(x, y, p.ax, p.ay, p.aw, p.ah);
                const bool inB = inside(x, y, p.bx, p.by, p.bw, p.bh);
                if (!inA && !inB)
                {
                    ++g;
                    for (int c = 0; c < pix; ++c) { if (px[c] != 0) { ++bg; break; } }
                }
                else if (inA)
                {
                    if (px[0] != 0xAA || px[1] != 0xAA) { ++aErr; }
                }
                else
                {
                    if (px[0] != 0x55 || px[1] != 0x55) { ++bErr; }
                }
            }
        }
    };

    Pack first = doPack();
    check(first.atlas.valid(), "atlas produced");
    if (!first.atlas.valid()) { return; }
    check(first.numPacked == 2, "both images placed", std::to_string(first.numPacked));
    check(first.atlas->getPixelSizeInBits() / 8 >= 3, "atlas has >=3 channels");
    scan(first, gaps, badGap, badA, badB);

    // 阳性对照：缝隙区域必须真的存在，否则这条用例什么都没钉住
    check(gaps > 0, "fixture leaves uncovered canvas pixels", std::to_string(gaps));
    check(badA == 0, "sub-rect A content blitted", "bad=" + std::to_string(badA));
    check(badB == 0, "sub-rect B content blitted", "bad=" + std::to_string(badB));

    // 轮 0 已计入；再跑 5 轮，每轮前把同尺寸堆块涂成 0xFF 并释放
    const size_t canvasBytes = first.atlas->getImageSizeInBytes();
    int rounds = 1;
    for (int round = 0; round < 5; ++round)
    {
        {
            std::vector<std::vector<unsigned char>> pool(8);
            for (std::vector<unsigned char>& v : pool) { v.assign(canvasBytes, 0xFFu); }
            check(!pool.back().empty() && pool.back().front() == 0xFFu,
                  "heap dirtied with canvas-sized blocks");
        }   // 8 块脏内存同时释放，落在同一个大小桶里
        Pack p = doPack();
        if (!p.atlas.valid()) { continue; }
        ++rounds;
        int rg = 0, rbg = 0, ra = 0, rb = 0;
        scan(p, rg, rbg, ra, rb);
        gaps += rg; badGap += rbg; badA += ra; badB += rb;
    }

    check(badA == 0, "sub-rect A content blitted (all rounds)", "bad=" + std::to_string(badA));
    check(badB == 0, "sub-rect B content blitted (all rounds)", "bad=" + std::to_string(badB));
    check(badGap == 0, "uncovered atlas pixels are zeroed",
          "dirty=" + std::to_string(badGap) + "/" + std::to_string(gaps)
              + " rounds=" + std::to_string(rounds));
}

void testPrimitiveBudgetGate()
{
    std::cout << "[budgetGate] glTF primitive / material budget" << std::endl;
    Utils::registerFileAliases();

    ProductionProfile profile;
    profile.budget.maxPrimitivesPerTile = 8;
    profile.budget.maxMaterialsPerTile = 8;

    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "wb_budget_gate";
    std::error_code ec;
    std::filesystem::remove_all(out, ec);

    ProductionPipeline pipeline(profile);
    ProductionPipeline::Options opt;
    opt.outputDir = out.string();
    // 暂存目录名现在带 pid（多任务不撞车），测试要按路径检查暂存区，
    // 所以这里显式指定一个固定名字（Options 里本来就允许覆盖）。
    opt.stagingDir = (out / ".wb-staging").string();
    const bool ran = pipeline.run(makeGrid(4, 4, 1.0).get(), opt);
    check(ran, "budget gate fixture builds", joinMessages(pipeline.failures()));

    std::string uri;
    for (const ProductionTile& t : pipeline.tiles())
    {
        if (t.hasContent) { uri = t.uri; break; }
    }
    check(!uri.empty(), "fixture has a content tile to swap");
    const std::filesystem::path staging = out / ".wb-staging";
    const std::filesystem::path content = staging / uri;
    check(std::filesystem::exists(content, ec), "staged content file exists", content.string());

    // selfValidate 只认磁盘上的 tileset.json，而 tileset 是 finalize 才写的。
    // 探针必须用生产同一个序列化函数落到 finalize 会用的那个路径上：自己拼一份
    // "看着像"的 JSON 会把 URI 口径写歪，而 URI 对不上时 content 循环整段空转，
    // 下面的"超标必拒"就全成了假绿。
    const std::string tsPath = (staging / "tileset.json").string();
    check(pipeline.writeTilesetJson(tsPath), "probe tileset.json written", tsPath);
    // 只看本次探针那个 URI 的裁决：其它瓦片同样会说话，混在一起就把
    // "闸有没有生效"变成了"总共报了几条"。
    auto probe = [&pipeline, &uri, &tsPath]()
    {
        const std::size_t before = pipeline.failures().size();
        pipeline.selfValidate(tsPath);
        std::vector<std::string> hits;
        for (std::size_t i = before; i < pipeline.failures().size(); ++i)
        {
            const std::string& m = pipeline.failures()[i];
            if (m.find(uri.c_str()) != std::string::npos) { hits.push_back(m); }
        }
        return hits;
    };
    auto writeFake = [&](unsigned prims, unsigned mats) {
        const std::vector<char> blob = makeFakeB3dm(prims, mats);
        std::ofstream f(content, std::ios::binary | std::ios::trunc);
        f.write(blob.data(), static_cast<std::streamsize>(blob.size()));
        f.close();
    };

    // 阳性对照①：真实写出的 b3dm 必须解析得出、且在预算内。
    // 这一条同时证明容器读法（28B 头 + 表长）对写出器的产物成立。
    check(probe().empty(), "real written b3dm parses and is within budget",
          joinMessages(probe()));

    // 阳性对照②：正好卡在上限的瓦片放行（否则下面三条"超标必拒"只是空转）。
    writeFake(8, 8);
    check(probe().empty(), "at-budget tile passes the gate");

    writeFake(9, 8);
    {
        const std::vector<std::string> hits = probe();
        bool ok = !hits.empty();
        for (const std::string& h : hits)
        {
            if (h.find("primitive budget") == std::string::npos) { ok = false; }
        }
        check(ok, "one primitive over the declared budget is rejected",
              hits.empty() ? "no hit" : hits[0]);
    }

    writeFake(8, 9);
    {
        const std::vector<std::string> hits = probe();
        bool ok = !hits.empty();
        for (const std::string& h : hits)
        {
            if (h.find("material budget") == std::string::npos) { ok = false; }
        }
        check(ok, "one material over the declared budget is rejected",
              hits.empty() ? "no hit" : hits[0]);
    }

    // 读不出 glTF 也必须是失败：预算无法证明 ≠ 没超
    {
        std::ofstream f(content, std::ios::binary | std::ios::trunc);
        f << "not a 3d tiles container";
        f.close();
    }
    {
        const std::vector<std::string> hits = probe();
        bool ok = !hits.empty();
        for (const std::string& h : hits)
        {
            if (h.find("parsable glTF") == std::string::npos) { ok = false; }
        }
        check(ok, "an unparsable content body is a failure, not a pass",
              hits.empty() ? "no hit" : hits[0]);
    }
}



int main(int argc, char** argv)
{
    std::string filter;
    if (argc > 1) { filter = argv[1]; }
    // 逐用例落盘：本环境进程退出阶段会异常中断，块缓冲的 stdout 只能看到
    // 表头、看不到"哪个用例死掉、死了之前过了几条"。这里每行立即 flush。
    std::ofstream progress("test_production_progress.txt", std::ios::trunc);
    // 崩溃定位用：本环境退出码不可信，异常/中止只在 stderr 里留下痕迹才能归因
    std::set_terminate([]() {
        std::cerr << "TERMINATE: " << std::flush;
        const auto e = std::current_exception();
        if (!e) { std::cerr << "no active exception (abort / fastfail)" << std::endl; }
        else
        {
            try { std::rethrow_exception(e); }
            catch (const std::exception& ex) { std::cerr << "exception: " << ex.what() << std::endl; }
            catch (...) { std::cerr << "unknown exception object" << std::endl; }
        }
        std::cerr.flush();
        std::_Exit(3);
    });
    auto run = [&](const char* name, void (*fn)()) {
        if (!filter.empty() && std::string(name).find(filter) == std::string::npos) { return; }
        const int p0 = g_passed;
        const int f0 = g_failed;
        if (progress.is_open()) { progress << ">> " << name << std::endl; }
        try
        {
            fn();
        }
        catch (const std::exception& ex)
        {
            ++g_failed;
            std::cerr << "  EXCEPTION in " << name << ": " << ex.what() << std::endl;
            if (progress.is_open()) { progress << "!! exception: " << ex.what() << std::endl; }
        }
        if (progress.is_open())
        {
            progress << "<< " << name << " checks=" << (g_passed - p0 + g_failed - f0)
                     << " failed=" << (g_failed - f0) << std::endl;
        }
    };
    run("profile", &testProfileValidation);
    run("sourceModel", &testSourceModelBaking);
    run("indexGuard", &testIndexValidationDropsOffendingTriangles);
    run("coordValidity", &testCoordinateValidity);
    run("cjkCleanup", &testCjkFailureCleanup);
    run("staging", &testStagingHygiene);
    run("objPrecheck", &testObjIntegrityPrecheck);
    run("nrmRepair", &testZeroNormalRepair);
    run("partition", &testPartitionCoverageAndDeterminism);
    run("giant", &testGiantSingleMeshSplits);
    run("error", &testErrorEvaluatorAnalytic);
    run("bvh", &testMeshBVHMatchesBruteForce);
    run("bvhOrder", &testMeshBVHTraversalBitExact);
    run("error-soundness", &testErrorBoundSoundness);
    run("refineReuse", &testRefineReusesCornerDistances);
    run("manifest", &testFeatureManifest);
    run("batchid", &testBatchIdVisitorPreservesAttrib0);
    run("simplifier", &testSimplifierMultiplePrimitiveSets);
    run("proxyAggregate", &testProxyAggregateCoversSourceScope);
    run("proxyAttempts", &testProxyAttemptsIsEffective);
    run("proxyTexture", &testProxyTextureBudget);
    run("proxyTextureLadder", &testProxyTextureLadder);
    run("texResize", &testOptimizerResizeIsolation);
    run("pipelineCoverage", &testPipelineLeafCoverageInvariant);
    run("proxySkip", &testPipelineProxySkipDegrades);
    run("bounds", &testPipelineBoundsGate);
    run("instancePoses", &testInstancePosesSurvive);
    run("noLocalPaths", &testArtifactsCarryNoLocalPaths);
    run("foldUv", &testFoldTextureTransformIntoUv);
    run("budgetGate", &testPrimitiveBudgetGate);
    run("atlasCanvas", &testAtlasCanvasZeroed);

    std::cout << std::endl << "passed=" << g_passed << " failed=" << g_failed << std::endl;
    std::cout.flush();
    // 同时落盘：崩溃/异常中断时块缓冲的 stdout 会被丢掉，只有文件里的那行能
    // 说明"跑到哪一步、过了几条"（历史上一次未捕获的 json 异常就是这样把
    // 58 条用例静默吞掉的，见 test_production_progress.txt）
    {
        std::ofstream f("test_production_result.txt", std::ios::trunc);
        f << "passed=" << g_passed << " failed=" << g_failed << "\n";
    }
    return g_failed == 0 ? 0 : 1;
}
