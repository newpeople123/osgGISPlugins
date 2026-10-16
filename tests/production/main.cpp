// 生产级管线单元测试（手册 5/6/7/9 节）
//
// 运行：test_production.exe          跑全部用例
//       test_production.exe <name>   只跑名字含 <name> 的用例
//
// 退出码：0 全通过；1 有用例失败。
// 注意：不依赖任何外部资产，全部用例在内存里程序化构造几何。
#include "3dtiles/production/ProductionPipeline.h"
#include "3dtiles/production/GeometryErrorEvaluator.h"
#include "3dtiles/production/SourceModel.h"
#include "3dtiles/production/SpatialPartitioner.h"
#include "3dtiles/production/ProductionProfile.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Array>
#include <iostream>
#include <map>
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
    check(mapper.occurrenceCount() == 5, "occurrence count",
          std::to_string(mapper.occurrenceCount()));

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

} // namespace

int main(int argc, char** argv)
{
    std::string filter;
    if (argc > 1) { filter = argv[1]; }
    auto run = [&](const char* name, void (*fn)()) {
        if (!filter.empty() && std::string(name).find(filter) == std::string::npos) { return; }
        fn();
    };
    run("profile", &testProfileValidation);
    run("sourceModel", &testSourceModelBaking);
    run("partition", &testPartitionCoverageAndDeterminism);
    run("giant", &testGiantSingleMeshSplits);
    run("error", &testErrorEvaluatorAnalytic);
    run("manifest", &testFeatureManifest);
    run("batchid", &testBatchIdVisitorPreservesAttrib0);

    std::cout << std::endl << "passed=" << g_passed << " failed=" << g_failed << std::endl;
    std::cout.flush();
    // 同时落盘：本环境下进程退出阶段可能异常结束，std::cout 的块缓冲会被丢掉
    {
        std::ofstream f("test_production_result.txt", std::ios::trunc);
        f << "passed=" << g_passed << " failed=" << g_failed << "\n";
    }
    // 注意：本环境下进程退出码会被宿主改写，因此以文件 + stdout 双通道输出
    return g_failed == 0 ? 0 : 1;
}
