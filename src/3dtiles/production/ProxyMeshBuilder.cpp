#include "3dtiles/production/ProxyMeshBuilder.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PrimitiveSet>
#include <osg/BoundingBox>
#include <osg/ComputeBoundsVisitor>
#include <algorithm>
#include <map>
#include <set>
#include "utils/Utils.h"
#include "utils/Simplifier.h"

using namespace osgGISPlugins;
using namespace osgGISPlugins::production;

ProxyMeshBuilder::ProxyMeshBuilder(const SourceModel& model, const ProductionProfile& profile,
                                   const std::vector<PrimRef>& primTable)
    : _model(model), _prims(primTable), _profile(profile)
{
}

unsigned ProxyMeshBuilder::countTriangles(osg::Node* node)
{
    if (!node) { return 0; }
    Utils::TriangleCounterVisitor tcv;
    node->accept(tcv);
    return tcv.count;
}

double ProxyMeshBuilder::simplifyNode(osg::Node* node, unsigned targetTriangles,
                                      double targetErrorMeters) const
{
    const unsigned before = countTriangles(node);
    if (before == 0 || targetTriangles >= before) { return 1.0; }
    const double ratio = static_cast<double>(targetTriangles) / static_cast<double>(before);

    // meshopt 的 target_error 与顶点数据同单位；这里用米制误差目标直接给，
    // 真实误差由 GeometryErrorEvaluator 独立复核，不依赖返回值（手册 9.1）。
    Simplifier simplifier(ratio, false, _profile.lockSeamBorder,
                          static_cast<float>(targetErrorMeters));
    node->accept(simplifier);

    const unsigned after = countTriangles(node);
    return (before > 0) ? static_cast<double>(after) / static_cast<double>(before) : 1.0;
}

osg::ref_ptr<osg::Node> ProxyMeshBuilder::buildAggregate(
    const std::vector<std::uint32_t>& items,
    double cellSize,
    std::map<FeatureId, std::vector<FeatureId>>& sources) const
{
    // 按空间体素把对象聚合；每个被占用的 cell 生成一个低面代理 + 新 proxyFeatureId
    struct Cell {
        long long x, y, z;
        bool operator<(const Cell& o) const {
            if (x != o.x) { return x < o.x; }
            if (y != o.y) { return y < o.y; }
            return z < o.z;
        }
    };
    std::map<Cell, std::vector<std::uint32_t>> cells;
    std::map<Cell, std::set<FeatureId>> cellFeatures;
    for (std::uint32_t id : items)
    {
        const PrimRef& pr = _prims[id];
        const osg::Vec3d& c = pr.centroid;
        Cell key{ static_cast<long long>(std::floor(c.x() / cellSize)),
                  static_cast<long long>(std::floor(c.y() / cellSize)),
                  static_cast<long long>(std::floor(c.z() / cellSize)) };
        cells[key].push_back(id);
        cellFeatures[key].insert(pr.globalFeatureId);
    }

    osg::ref_ptr<osg::Group> root = new osg::Group;
    // proxyFeatureId 放在源 feature 空间之外，避免与真实源对象 ID 混淆（手册 8.2）
    FeatureId nextProxyId = 0xF0000000u;
    std::vector<Cell> keys;
    for (auto& kv : cells) { keys.push_back(kv.first); }
    std::sort(keys.begin(), keys.end());

    for (const Cell& k : keys)
    {
        const std::vector<std::uint32_t>& citems = cells[k];
        // 聚合代理：把整个 cell 的几何合成一个 Geode（不再保留逐对象身份）
        osg::ref_ptr<osg::Node> cluster = _model.materialize(_prims, citems, false);
        if (!cluster.valid()) { continue; }
        // 强简化到 cell 预算
        const unsigned target = std::max<unsigned>(
            4u, _profile.budget.targetTrianglesPerTile / static_cast<unsigned>(std::max<std::size_t>(1, cells.size())));
        simplifyNode(cluster.get(), target, static_cast<double>(cellSize) * 0.05);

        osg::Geode* geode = cluster->asGroup() && cluster->asGroup()->getNumChildren()
            ? cluster->asGroup()->getChild(0)->asGeode() : nullptr;
        osg::ref_ptr<osg::Group> wrapper = new osg::Group;
        if (geode)
        {
            osg::ref_ptr<osg::Geode> g = new osg::Geode;
            for (unsigned d = 0; d < geode->getNumDrawables(); ++d)
            {
                g->addDrawable(geode->getDrawable(d));
            }
            g->setName(std::string("proxy_cluster_") + std::to_string(nextProxyId));
            wrapper->addChild(g.get());
        }
        else
        {
            wrapper = cluster->asGroup();
        }
        if (!wrapper.valid() || wrapper->getNumChildren() == 0) { continue; }
        root->addChild(wrapper.get());

        const FeatureId pid = nextProxyId++;
        std::vector<FeatureId> srcs(cellFeatures[k].begin(), cellFeatures[k].end());
        std::sort(srcs.begin(), srcs.end());
        sources[pid] = srcs;
    }
    return root;
}

ProxyResult ProxyMeshBuilder::build(const std::vector<std::uint32_t>& items,
                                    unsigned targetTriangles,
                                    bool allowAggregate) const
{
    ProxyResult res;
    if (items.empty())
    {
        res.failure = "empty item set";
        return res;
    }

    // 身份保持型代理：每个 globalFeatureId 一个 Geode
    osg::ref_ptr<osg::Node> node = _model.materialize(_prims, items, true);
    if (!node.valid())
    {
        res.failure = "materialize failed";
        return res;
    }
    const unsigned tris = countTriangles(node.get());
    res.triangles = tris;

    if (tris <= targetTriangles)
    {
        res.node = node;
        res.identity = true;
        res.appliedRatio = 1.0;
        return res;
    }

    // 逐步加压（手册 7.2 第 3 点：按 triangle budget 从高到低逐步降低）
    const double ratios[] = {1.0, 0.7, 0.5, 0.35, 0.25, 0.15, 0.08, 0.04, 0.02, 0.01};
    osg::ref_ptr<osg::Node> best = nullptr;
    unsigned bestTris = tris;
    double bestRatio = 1.0;
    // 米制误差目标：节点尺寸越大允许的绝对误差越大
    osg::BoundingBox bb;
    for (std::uint32_t id : items) { if (id < _prims.size()) { bb.expandBy(_prims[id].aabb); } }
    const double diag = bb.valid() ? (bb._max - bb._min).length() : 1.0;
    const double targetErrorMeters = diag * 0.01;

    for (double r : ratios)
    {
        osg::ref_ptr<osg::Node> attempt = _model.materialize(_prims, items, true);
        if (!attempt.valid()) { continue; }
        const unsigned want = static_cast<unsigned>(std::ceil(static_cast<double>(tris) * r));
        const unsigned tgt = std::min<unsigned>(want, targetTriangles);
        const double applied = simplifyNode(attempt.get(), tgt, targetErrorMeters);
        const unsigned got = countTriangles(attempt.get());
        best = attempt;
        bestTris = got;
        bestRatio = applied;
        if (got <= targetTriangles) { break; }
    }

    if (bestTris <= targetTriangles || !allowAggregate)
    {
        res.node = best;
        res.triangles = bestTris;
        res.appliedRatio = bestRatio;
        if (bestTris > targetTriangles)
        {
            // 手册 8.3 第 7 点：锁边导致达不到目标时接受更多三角形，但必须如实上报
            res.failure = _profile.strictIdentity
                ? "strictIdentity: proxy exceeds triangle budget, failing"
                : std::string();
        }
        return res;
    }

    // ---- 聚合代理 fallback（手册 7.3 第 5 点）----
    if (_profile.strictIdentity)
    {
        res.failure = "strictIdentity forbids aggregate proxy; cannot meet budget";
        return res;
    }

    double cellSize = diag * 0.25;
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        std::map<FeatureId, std::vector<FeatureId>> sources;
        osg::ref_ptr<osg::Node> agg = buildAggregate(items, cellSize, sources);
        const unsigned aggTris = countTriangles(agg.get());
        if (agg.valid() && aggTris > 0)
        {
            res.node = agg;
            res.triangles = aggTris;
            res.aggregate = true;
            res.aggregateSources = sources;
            res.appliedRatio = static_cast<double>(aggTris) / static_cast<double>(tris);
            if (aggTris <= targetTriangles) { return res; }
        }
        cellSize *= 1.8;
    }

    // 仍超预算：如实失败，绝不输出超限瓦片（手册 7.3 第 6 点）
    res.failure = "proxy still over triangle budget after aggregate fallback ("
                + std::to_string(res.triangles) + " > " + std::to_string(targetTriangles) + ")";
    return res;
}
