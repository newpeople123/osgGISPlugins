#include "3dtiles/production/SpatialPartitioner.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_set>

using namespace osgGISPlugins;
using namespace osgGISPlugins::production;

double SpatialPartitioner::Load::maxRatio(const ResourceBudget& b) const
{
    double r = 0.0;
    r = std::max(r, static_cast<double>(tri) / static_cast<double>(b.maxTrianglesPerTile));
    r = std::max(r, static_cast<double>(vert) / static_cast<double>(b.maxVerticesPerTile));
    r = std::max(r, static_cast<double>(prim) / static_cast<double>(b.maxPrimitivesPerTile));
    r = std::max(r, static_cast<double>(mat) / static_cast<double>(b.maxMaterialsPerTile));
    r = std::max(r, static_cast<double>(bytes) / static_cast<double>(b.maxTileContentBytes));
    return r;
}

SpatialPartitioner::SpatialPartitioner(const SourceModel& model, const ProductionProfile& profile,
                                       std::vector<PrimRef>& primTable)
    : prims(primTable), _model(model), _profile(profile)
{
}

double SpatialPartitioner::surfaceArea(const osg::BoundingBox& bb) const
{
    if (!bb.valid()) { return 0.0; }
    const double dx = std::max(0.0, static_cast<double>(bb.xMax() - bb.xMin()));
    const double dy = std::max(0.0, static_cast<double>(bb.yMax() - bb.yMin()));
    const double dz = std::max(0.0, static_cast<double>(bb.zMax() - bb.zMin()));
    return 2.0 * (dx * dy + dy * dz + dz * dx);
}

// 子节点建完后回填本节点的 items / bounds / 负载统计。
//
// 关键：内部节点的 items 语义是"该子树覆盖的**叶子**单元集合"（手册 6.5 要求
// 子节点集合并 == 父节点集合）。三角形级切分会不断造出新的叶子单元，如果只把
// 本层的切分结果写进去，下一层再切一次就不相等了，coverage 校验必然误报。
void SpatialPartitioner::refreshItemsFromChildren(int self)
{
    PartitionNode& n = nodes[self];
    std::vector<std::uint32_t> u;
    for (int c : n.children)
    {
        if (c < 0 || static_cast<std::size_t>(c) >= nodes.size()) { continue; }
        u.insert(u.end(), nodes[c].items.begin(), nodes[c].items.end());
    }
    std::sort(u.begin(), u.end());
    u.erase(std::unique(u.begin(), u.end()), u.end());
    n.items = u;
    n.bounds = computeBounds(n.items);
    const Load nl = computeLoad(n.items);
    n.triangles = nl.tri;
    n.vertices = nl.vert;
    n.primitiveCount = nl.prim;
    n.materialCount = nl.mat;
    n.byteEstimate = nl.bytes;
    const double ratio = nl.maxRatio(_profile.budget);
    n.overHard = ratio > 1.0;
}

SpatialPartitioner::Load SpatialPartitioner::computeLoad(const std::vector<std::uint32_t>& items) const
{
    Load L;
    std::set<unsigned> mats;
    std::set<std::pair<unsigned, unsigned>> primKeys;
    std::uint64_t bytes = 0;
    for (std::uint32_t id : items)
    {
        const PrimRef& pr = prims[id];
        L.tri += pr.triCount;
        L.vert += pr.vertexCount;
        mats.insert(pr.materialId);
        primKeys.insert(std::make_pair(pr.meshId, pr.primSetIndex));
        bytes += pr.byteEstimate;
    }
    L.prim = primKeys.size();
    L.mat = mats.size();
    // 每材质按 64 KB 计纹理/材质表开销（KTX2 256x256 的常见量级；
    // 旧的 4 KB 明显低估，会让带纹理瓦片的字节预算形同虚设）
    L.bytes = bytes + mats.size() * 65536ull;
    return L;
}

osg::BoundingBox SpatialPartitioner::computeBounds(const std::vector<std::uint32_t>& items) const
{
    osg::BoundingBox bb;
    for (std::uint32_t id : items) { bb.expandBy(prims[id].aabb); }
    return bb;
}

void SpatialPartitioner::evaluateCandidate(const std::vector<std::uint32_t>& items,
                                           const osg::BoundingBox& parentBounds,
                                           int axis, double plane, int level,
                                           const std::vector<int>& groupOf,
                                           Candidate& out) const
{
    out = Candidate();
    out.axis = axis;
    out.plane = plane;
    out.level = level;

    // groupOf[i] 给出 item i 属于哪个"不可分单位"；
    // 同一单位的 item 必须整体落在同一侧，否则该候选非法（对象级/primitive 级约束）
    std::map<int, char> sideOfGroup;
    std::vector<char> side(items.size(), 0);
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const double c = (axis == 0) ? prims[items[i]].centroid.x()
                       : (axis == 1) ? prims[items[i]].centroid.y()
                                     : prims[items[i]].centroid.z();
        const char s = (c <= plane) ? 0 : 1;
        const int g = groupOf[i];
        auto it = sideOfGroup.find(g);
        if (it == sideOfGroup.end())
        {
            sideOfGroup[g] = s;
        }
        else if (it->second != s)
        {
            // 会把同一个对象/primitive 拆开 —— 该 level 不允许
            return;
        }
        side[i] = s;
    }

    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (side[i] == 0) { out.left.push_back(items[i]); }
        else { out.right.push_back(items[i]); }
    }
    if (out.left.empty() || out.right.empty()) { return; }

    const Load ll = computeLoad(out.left);
    const Load lr = computeLoad(out.right);
    const Load lp = computeLoad(items);
    const ResourceBudget& b = _profile.budget;

    out.maxRatioAfter = std::max(ll.maxRatio(b), lr.maxRatio(b));

    const osg::BoundingBox bl = computeBounds(out.left);
    const osg::BoundingBox br = computeBounds(out.right);
    const double saP = surfaceArea(parentBounds);
    const double saL = surfaceArea(bl);
    const double saR = surfaceArea(br);
    const double nP = static_cast<double>(std::max<std::uint64_t>(1, lp.tri));
    const double sah = (saP > 1e-12)
        ? (saL * static_cast<double>(ll.tri) + saR * static_cast<double>(lr.tri)) / (saP * nP)
        : 1.0;

    // overlap：两侧包围盒相交体积 / 较小者体积
    double overlap = 0.0;
    {
        const double ix = std::max(0.0, static_cast<double>(std::min(bl.xMax(), br.xMax()) - std::max(bl.xMin(), br.xMin())));
        const double iy = std::max(0.0, static_cast<double>(std::min(bl.yMax(), br.yMax()) - std::max(bl.yMin(), br.yMin())));
        const double iz = std::max(0.0, static_cast<double>(std::min(bl.zMax(), br.zMax()) - std::max(bl.zMin(), br.zMin())));
        const double iv = ix * iy * iz;
        const double vl = std::max(1e-12, static_cast<double>(bl.xMax() - bl.xMin()) * static_cast<double>(bl.yMax() - bl.yMin()) * static_cast<double>(bl.zMax() - bl.zMin()));
        const double vr = std::max(1e-12, static_cast<double>(br.xMax() - br.xMin()) * static_cast<double>(br.yMax() - br.yMin()) * static_cast<double>(br.zMax() - br.zMin()));
        overlap = iv / std::min(vl, vr);
        if (overlap > 1.0) { overlap = 1.0; }
    }

    const double nL = static_cast<double>(ll.tri);
    const double nR = static_cast<double>(lr.tri);
    const double imbalance = (nL + nR > 0.0) ? std::fabs(nL - nR) / (nL + nR) : 0.0;

    // 材质重复比例
    std::set<unsigned> ml, mr;
    for (std::uint32_t id : out.left) { ml.insert(prims[id].materialId); }
    for (std::uint32_t id : out.right) { mr.insert(prims[id].materialId); }
    unsigned shared = 0;
    for (unsigned m : ml) { if (mr.count(m)) { ++shared; } }
    const double dup = (ml.size() + mr.size() > 0)
        ? static_cast<double>(shared) / static_cast<double>(ml.size() + mr.size()) : 0.0;

    // 语义破坏：被拆开的"不可分单位"比例（对象级切分时为 0，primitive 级时恒 >0）
    double semantic = 0.0;
    if (level == 0) { semantic = 0.0; }
    else if (level == 1)
    {
        // 拆开的是对象：按对象数统计
        std::set<FeatureId> fl, fr;
        for (std::uint32_t id : out.left) { fl.insert(prims[id].globalFeatureId); }
        for (std::uint32_t id : out.right) { fr.insert(prims[id].globalFeatureId); }
        unsigned broken = 0;
        for (FeatureId f : fl) { if (fr.count(f)) { ++broken; } }
        semantic = (fl.size() + fr.size() > 0)
            ? static_cast<double>(broken) / static_cast<double>(fl.size() + fr.size()) : 0.0;
    }
    else
    {
        semantic = 1.0;
    }

    const PartitionWeights& w = _profile.weights;
    out.cost = w.wLoad * out.maxRatioAfter
             + w.wSAH * sah
             + w.wOverlap * overlap
             + w.wBalance * imbalance
             + w.wMaterial * dup
             + w.wSemantic * semantic;
    out.valid = true;

    // 确定 tie-break：代价 -> 轴 -> 平面 -> 左侧数量
    std::uint64_t key = 0;
    key = (static_cast<std::uint64_t>(out.left.size()) << 32) ^ (static_cast<std::uint64_t>(axis) << 8)
        ^ static_cast<std::uint64_t>(static_cast<long long>(plane * 1000.0) & 0xFF);
    out.tieKey = key;
}

void SpatialPartitioner::generateCandidates(const std::vector<std::uint32_t>& items,
                                            const osg::BoundingBox& bounds,
                                            int level,
                                            std::vector<Candidate>& out) const
{
    // 不可分单位：level 0 -> 按 globalFeatureId；level 1 -> 按 (mesh,primSet)；level 2 -> 单个 item
    std::vector<int> groupOf(items.size(), 0);
    std::map<std::uint64_t, int> gid;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const PrimRef& pr = prims[items[i]];
        std::uint64_t key;
        if (level == 0)
        {
            key = pr.globalFeatureId;
        }
        else if (level == 1)
        {
            key = (static_cast<std::uint64_t>(pr.meshId) << 32) | pr.primSetIndex;
        }
        else
        {
            key = 0x8000000000000000ull | items[i];
        }
        auto it = gid.find(key);
        if (it == gid.end())
        {
            const int g = static_cast<int>(gid.size());
            gid[key] = g;
            groupOf[i] = g;
        }
        else
        {
            groupOf[i] = it->second;
        }
    }

    const unsigned bins = _profile.weights.bins;
    for (int axis = 0; axis < 3; ++axis)
    {
        const double lo = (axis == 0) ? bounds.xMin() : (axis == 1) ? bounds.yMin() : bounds.zMin();
        const double hi = (axis == 0) ? bounds.xMax() : (axis == 1) ? bounds.yMax() : bounds.zMax();
        const double span = hi - lo;
        // 退化轴跳过（手册 6.5：零轴改用其他轴）
        if (span <= 1e-12) { continue; }

        std::vector<double> planes;
        for (unsigned b = 1; b < bins; ++b)
        {
            planes.push_back(lo + span * static_cast<double>(b) / static_cast<double>(bins));
        }
        // 中位数候选：按单位质心排序取中点
        {
            std::vector<std::pair<double, int>> units;
            std::map<int, double> sum;
            std::map<int, unsigned> cnt;
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                const PrimRef& pr = prims[items[i]];
                const double c = (axis == 0) ? pr.centroid.x() : (axis == 1) ? pr.centroid.y() : pr.centroid.z();
                sum[groupOf[i]] += c;
                cnt[groupOf[i]] += 1;
            }
            for (auto& kv : sum) { units.push_back({kv.second / cnt[kv.first], kv.first}); }
            std::sort(units.begin(), units.end());
            if (units.size() >= 2)
            {
                const std::size_t mid = units.size() / 2;
                planes.push_back((units[mid - 1].first + units[mid].first) * 0.5);
            }
        }

        for (double p : planes)
        {
            Candidate c;
            evaluateCandidate(items, bounds, axis, p, level, groupOf, c);
            if (c.valid) { out.push_back(c); }
        }
    }
}

bool SpatialPartitioner::splitItemByTriangles(std::uint32_t itemId, int axis,
                                              std::uint32_t& outLeft, std::uint32_t& outRight)
{
    PrimRef base = prims[itemId];
    if (base.tris.size() < 2) { return false; }

    // 按三角形质心排序，取中位数切开（不产生新顶点，不改属性）
    std::vector<std::pair<double, std::uint32_t>> keys;
    keys.reserve(base.tris.size());
    for (std::uint32_t t : base.tris)
    {
        unsigned idx[3];
        _model.triangleIndices(base, t, idx);
        const SourceMesh& m = _model.meshes[base.meshId];
        const osg::Vec3& a = (*m.positions)[idx[0]];
        const osg::Vec3& b = (*m.positions)[idx[1]];
        const osg::Vec3& c = (*m.positions)[idx[2]];
        const double cc = (axis == 0) ? (a.x() + b.x() + c.x()) / 3.0
                        : (axis == 1) ? (a.y() + b.y() + c.y()) / 3.0
                                      : (a.z() + b.z() + c.z()) / 3.0;
        keys.push_back({cc, t});
    }
    std::sort(keys.begin(), keys.end(), [](const std::pair<double, std::uint32_t>& a,
                                           const std::pair<double, std::uint32_t>& b) {
        if (a.first != b.first) { return a.first < b.first; }
        return a.second < b.second;
    });
    const std::size_t mid = keys.size() / 2;
    if (mid == 0 || mid == keys.size()) { return false; }

    PrimRef L = base, R = base;
    L.tris.clear();
    R.tris.clear();
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        if (i < mid) { L.tris.push_back(keys[i].second); }
        else { R.tris.push_back(keys[i].second); }
    }
    L.triCount = static_cast<unsigned>(L.tris.size());
    R.triCount = static_cast<unsigned>(R.tris.size());
    L.splitLevel = 2;
    R.splitLevel = 2;
    // 重算指标
    auto refresh = [this, &base](PrimRef& pr) {
        pr.aabb.init();
        osg::Vec3d cen(0, 0, 0);
        std::vector<unsigned> verts;
        for (std::uint32_t t : pr.tris)
        {
            unsigned idx[3];
            _model.triangleIndices(pr, t, idx);
            const SourceMesh& m = _model.meshes[pr.meshId];
            for (int k = 0; k < 3; ++k)
            {
                const osg::Vec3& v = (*m.positions)[idx[k]];
                pr.aabb.expandBy(v);
                cen += osg::Vec3d(v);
                verts.push_back(idx[k]);
            }
        }
        std::sort(verts.begin(), verts.end());
        verts.erase(std::unique(verts.begin(), verts.end()), verts.end());
        pr.vertexCount = static_cast<unsigned>(verts.size());
        pr.centroid = pr.tris.empty() ? osg::Vec3d(0, 0, 0) : cen / static_cast<double>(pr.tris.size() * 3);
        // 字节估算按三角形比例从父 PrimRef 分摊（含纹理字节），禁止在此
        // 用纯几何公式重算——那会把 SourceModel 层的校准估算整个覆盖掉，
        // 子节点被低估后分区器停止切分，最终写出超限瓦片（lugouqiao 实测
        // 单瓦片 51MB 漏网即此因 + 纹理字节未计）。
        const std::uint64_t perTriBytes = (base.triCount > 0)
            ? base.byteEstimate / static_cast<std::uint64_t>(base.triCount) : 0ull;
        pr.byteEstimate = perTriBytes * static_cast<std::uint64_t>(pr.tris.size());
    };
    refresh(L);
    refresh(R);

    outLeft = static_cast<std::uint32_t>(prims.size());
    prims.push_back(L);
    outRight = static_cast<std::uint32_t>(prims.size());
    prims.push_back(R);
    return true;
}

int SpatialPartitioner::recursiveBuild(const std::vector<std::uint32_t>& items, unsigned depth, int parent)
{
    const int self = static_cast<int>(nodes.size());
    nodes.push_back(PartitionNode());
    nodes[self].parent = parent;
    nodes[self].depth = depth;
    nodes[self].items = items;
    nodes[self].bounds = computeBounds(items);
    const Load L = computeLoad(items);
    nodes[self].triangles = L.tri;
    nodes[self].vertices = L.vert;
    nodes[self].primitiveCount = L.prim;
    nodes[self].materialCount = L.mat;
    nodes[self].byteEstimate = L.bytes;
    const double ratio = L.maxRatio(_profile.budget);
    nodes[self].overHard = ratio > 1.0;

    if (items.empty()) { return self; }

    const bool atMaxDepth = depth >= _profile.budget.maxTreeDepth;
    if (atMaxDepth)
    {
        if (nodes[self].overHard)
        {
            std::string what = "maxTreeDepth reached at depth " + std::to_string(depth) +
                               " but node still over hard budget: triangles=" + std::to_string(nodes[self].triangles) +
                               " vertices=" + std::to_string(nodes[self].vertices) +
                               " primitives=" + std::to_string(nodes[self].primitiveCount) +
                               " materials=" + std::to_string(nodes[self].materialCount) +
                               " estBytes=" + std::to_string(nodes[self].byteEstimate);
            failures.push_back(what);
        }
        return self;
    }

    // 单单元且无法再分却仍超预算 -> 明确失败（手册 6.5）
    if (items.size() == 1 && nodes[self].overHard)
    {
        // 注意 pr 是引用：splitItemByTriangles 会 push_back 到 prims，
        // 可能让引用失效，因此每次都要重新取（见下方 featureId 的取法）。
        const FeatureId fid0 = prims[items[0]].globalFeatureId;
        if (prims[items[0]].tris.size() <= 1)
        {
            failures.push_back("single unsplittable unit (1 triangle) over hard budget: feature=" +
                               std::to_string(fid0) +
                               " triangles=" + std::to_string(nodes[self].triangles));
            return self;
        }
        // 强制三角形级切分
        int bestAxis = -1;
        double bestSpan = -1.0;
        for (int axis = 0; axis < 3; ++axis)
        {
            const double span = (axis == 0) ? (nodes[self].bounds.xMax() - nodes[self].bounds.xMin())
                              : (axis == 1) ? (nodes[self].bounds.yMax() - nodes[self].bounds.yMin())
                                            : (nodes[self].bounds.zMax() - nodes[self].bounds.zMin());
            if (span > bestSpan) { bestSpan = span; bestAxis = axis; }
        }
        std::uint32_t li = 0, ri = 0;
        if (bestAxis >= 0 && splitItemByTriangles(items[0], bestAxis, li, ri))
        {
            nodes[self].splitAxis = bestAxis;
            nodes[self].splitLevel = 2;
            const int cl = recursiveBuild({li}, depth + 1, self);
            const int cr = recursiveBuild({ri}, depth + 1, self);
            nodes[self].children.push_back(cl);
            nodes[self].children.push_back(cr);
            refreshItemsFromChildren(self);
            return self;
        }
        failures.push_back("cannot split unit further but over hard budget: feature=" +
                           std::to_string(fid0));
        return self;
    }

    // 生成候选：先对象级，再 primitive 级，再三角形级（后者只在必要时）
    std::vector<Candidate> cands;
    generateCandidates(items, nodes[self].bounds, 0, cands);
    if (cands.empty() && nodes[self].overHard)
    {
        generateCandidates(items, nodes[self].bounds, 1, cands);
    }

    const double parentRatio = ratio;
    double parentCost = _profile.weights.wLoad * parentRatio;   // 父节点"不分裂"的代价基准

    // 超预算时要求候选必须降低最大负载比例
    std::vector<Candidate> usable;
    for (const Candidate& c : cands)
    {
        if (nodes[self].overHard)
        {
            if (c.maxRatioAfter < parentRatio - 1e-9) { usable.push_back(c); }
        }
        else
        {
            if (c.cost < parentCost * (1.0 - _profile.weights.minImprovement)) { usable.push_back(c); }
        }
    }

    if (usable.empty())
    {
        // 超预算但对象级/primitive 级都不行：强制做三角形级切分
        if (nodes[self].overHard)
        {
            std::vector<std::uint32_t> newItems;
            bool did = false;
            for (std::uint32_t id : items)
            {
                const PrimRef& pr = prims[id];
                if (pr.tris.size() >= 2)
                {
                    int axis = -1;
                    double bestSpan = -1.0;
                    for (int a = 0; a < 3; ++a)
                    {
                        const double span = (a == 0) ? (pr.aabb.xMax() - pr.aabb.xMin())
                                          : (a == 1) ? (pr.aabb.yMax() - pr.aabb.yMin())
                                                     : (pr.aabb.zMax() - pr.aabb.zMin());
                        if (span > bestSpan) { bestSpan = span; axis = a; }
                    }
                    std::uint32_t li = 0, ri = 0;
                    if (axis >= 0 && splitItemByTriangles(id, axis, li, ri))
                    {
                        newItems.push_back(li);
                        newItems.push_back(ri);
                        did = true;
                        continue;
                    }
                }
                newItems.push_back(id);
            }
            if (did)
            {
                nodes[self].splitLevel = 2;
                // 直接用质心在最长轴上二分 newItems，交给下一层继续处理
                std::vector<std::uint32_t> left, right;
                osg::BoundingBox nb = computeBounds(newItems);
                int axis = 0;
                double bestSpan = -1.0;
                for (int a = 0; a < 3; ++a)
                {
                    const double span = (a == 0) ? (nb.xMax() - nb.xMin())
                                      : (a == 1) ? (nb.yMax() - nb.yMin())
                                                 : (nb.zMax() - nb.zMin());
                    if (span > bestSpan) { bestSpan = span; axis = a; }
                }
                const double mid = (axis == 0) ? (nb.xMin() + nb.xMax()) * 0.5
                                 : (axis == 1) ? (nb.yMin() + nb.yMax()) * 0.5
                                               : (nb.zMin() + nb.zMax()) * 0.5;
                for (std::uint32_t id : newItems)
                {
                    const double c = (axis == 0) ? prims[id].centroid.x()
                                   : (axis == 1) ? prims[id].centroid.y()
                                                 : prims[id].centroid.z();
                    if (c <= mid) { left.push_back(id); } else { right.push_back(id); }
                }
                if (left.empty() || right.empty())
                {
                    // 所有质心同侧：按源 ID 中位数硬分（手册 6.5 tie-break）
                    left.clear(); right.clear();
                    std::vector<std::uint32_t> sorted = newItems;
                    std::sort(sorted.begin(), sorted.end());
                    const std::size_t m = sorted.size() / 2;
                    left.assign(sorted.begin(), sorted.begin() + m);
                    right.assign(sorted.begin() + m, sorted.end());
                }
                nodes[self].splitAxis = axis;
                nodes[self].splitPos = mid;
                const int cl = recursiveBuild(left, depth + 1, self);
                const int cr = recursiveBuild(right, depth + 1, self);
                nodes[self].children.push_back(cl);
                nodes[self].children.push_back(cr);
                refreshItemsFromChildren(self);
                return self;
            }
            warnings.push_back("node over hard budget but no split candidate found; emitting as-is"
                               " (triangles=" + std::to_string(nodes[self].triangles) + ")");
        }
        return self;   // 已满足预算，停止分裂
    }

    // 选代价最小者；同代价按 (axis, plane, leftSize) 固定 tie-break
    const Candidate* best = &usable[0];
    for (const Candidate& c : usable)
    {
        if (c.cost < best->cost - 1e-12) { best = &c; }
        else if (std::fabs(c.cost - best->cost) <= 1e-12)
        {
            if (c.axis < best->axis) { best = &c; }
            else if (c.axis == best->axis && c.plane < best->plane) { best = &c; }
        }
    }

    nodes[self].splitAxis = best->axis;
    nodes[self].splitPos = best->plane;
    nodes[self].splitLevel = best->level;
    const int cl = recursiveBuild(best->left, depth + 1, self);
    const int cr = recursiveBuild(best->right, depth + 1, self);
    nodes[self].children.push_back(cl);
    nodes[self].children.push_back(cr);
    refreshItemsFromChildren(self);
    return self;
}

int SpatialPartitioner::build()
{
    nodes.clear();
    failures.clear();
    warnings.clear();
    if (prims.empty()) { return -1; }
    std::vector<std::uint32_t> all(prims.size());
    for (std::size_t i = 0; i < prims.size(); ++i) { all[i] = static_cast<std::uint32_t>(i); }
    return recursiveBuild(all, 0, -1);
}

void SpatialPartitioner::subtreeItems(int node, std::vector<std::uint32_t>& out) const
{
    if (node < 0 || node >= static_cast<int>(nodes.size())) { return; }
    const PartitionNode& n = nodes[node];
    if (n.children.empty())
    {
        out.insert(out.end(), n.items.begin(), n.items.end());
        return;
    }
    for (int c : n.children) { subtreeItems(c, out); }
}

bool SpatialPartitioner::verifyCoverage()
{
    // 手册 6.5 的不变量是"子节点**声明**的单元集合并 == 父节点声明的集合"。
    // 注意不能拿子树的叶子集合来比：内部节点在三角形级切分后，它的 items 是
    // "这一层切分出来的单元"，而子节点会继续往下切，叶子集合必然更大。
    // 之前用 subtreeItems() 收集叶子集合，导致所有做过切分的节点都误报。
    bool ok = true;
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        const PartitionNode& n = nodes[i];
        if (n.children.empty()) { continue; }
        std::vector<std::uint32_t> a;
        for (int c : n.children)
        {
            if (c < 0 || static_cast<std::size_t>(c) >= nodes.size())
            {
                ok = false;
                warnings.push_back("child index out of range " + std::to_string(c)
                                   + " under parent " + std::to_string(i));
                continue;
            }
            const PartitionNode& ch = nodes[c];
            if (ch.items.empty())
            {
                ok = false;
                warnings.push_back("empty child node " + std::to_string(c) +
                                   " under parent " + std::to_string(i));
                continue;
            }
            a.insert(a.end(), ch.items.begin(), ch.items.end());
        }
        std::vector<std::uint32_t> b = n.items;
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        a.erase(std::unique(a.begin(), a.end()), a.end());
        b.erase(std::unique(b.begin(), b.end()), b.end());
        if (a != b)
        {
            ok = false;
            warnings.push_back("coverage mismatch at node " + std::to_string(i) +
                               ": children declare " + std::to_string(a.size()) +
                               " units, node declares " + std::to_string(b.size()));
        }
    }
    return ok;
}
