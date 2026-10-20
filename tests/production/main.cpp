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
#include "utils/Simplifier.h"
#include "utils/Utils.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Array>
#include <osg/PrimitiveSet>
#include <osg/ComputeBoundsVisitor>
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
#include <map>
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
    run("nrmRepair", &testZeroNormalRepair);
    run("partition", &testPartitionCoverageAndDeterminism);
    run("giant", &testGiantSingleMeshSplits);
    run("error", &testErrorEvaluatorAnalytic);
    run("manifest", &testFeatureManifest);
    run("batchid", &testBatchIdVisitorPreservesAttrib0);
    run("simplifier", &testSimplifierMultiplePrimitiveSets);
    run("proxyAggregate", &testProxyAggregateCoversSourceScope);
    run("pipelineCoverage", &testPipelineLeafCoverageInvariant);
    run("proxySkip", &testPipelineProxySkipDegrades);
    run("bounds", &testPipelineBoundsGate);
    run("instancePoses", &testInstancePosesSurvive);
    run("noLocalPaths", &testArtifactsCarryNoLocalPaths);

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
