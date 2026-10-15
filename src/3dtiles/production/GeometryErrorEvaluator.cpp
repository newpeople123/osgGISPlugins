#include "3dtiles/production/GeometryErrorEvaluator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <limits>

using namespace osgGISPlugins;
using namespace osgGISPlugins::production;

namespace {

// 点到三角形最近距离平方
double pointTriangleDistanceSq(const osg::Vec3d& p,
                               const osg::Vec3d& a,
                               const osg::Vec3d& b,
                               const osg::Vec3d& c)
{
    const osg::Vec3d ab = b - a;
    const osg::Vec3d ac = c - a;
    const osg::Vec3d ap = p - a;

    const double d1 = ab * ap;
    const double d2 = ac * ap;
    if (d1 <= 0.0 && d2 <= 0.0) { return (p - a).length2(); }

    const osg::Vec3d bp = p - b;
    const double d3 = ab * bp;
    const double d4 = ac * bp;
    if (d3 >= 0.0 && d4 <= d3) { return (p - b).length2(); }

    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        const double v = d1 / (d1 - d3);
        return (p - (a + ab * v)).length2();
    }

    const osg::Vec3d cp = p - c;
    const double d5 = ab * cp;
    const double d6 = ac * cp;
    if (d6 >= 0.0 && d5 <= d6) { return (p - c).length2(); }

    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        const double w = d2 / (d2 - d6);
        return (p - (a + ac * w)).length2();
    }

    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return (p - (b + (c - b) * w)).length2();
    }

    const double denom = 1.0 / (va + vb + vc);
    const double v = vb * denom;
    const double w = vc * denom;
    const osg::Vec3d closest = a + ab * v + ac * w;
    return (p - closest).length2();
}

} // namespace

// ---------------------------------------------------------------------------
// MeshBVH
// ---------------------------------------------------------------------------
void MeshBVH::build(const TriangleSoup& soup)
{
    _nodes.clear();
    _tris.clear();
    _verts = soup.v;
    const std::size_t n = soup.size();
    if (n == 0) { return; }
    _tris.resize(n);
    std::vector<std::size_t> order(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        _tris[i][0] = i * 3;
        _tris[i][1] = i * 3 + 1;
        _tris[i][2] = i * 3 + 2;
        order[i] = i;
    }
    buildRecursive(order, 0, n, 0);
}

int MeshBVH::buildRecursive(std::vector<std::size_t>& order, std::size_t begin, std::size_t end, int depth)
{
    const int self = static_cast<int>(_nodes.size());
    _nodes.push_back(Node());
    // 注意：绝不能在这里取 Node& 引用。下面的递归会继续 push_back 到 _nodes，
    // vector 扩容后引用立即失效，写 _nodes[self].left/right 就是往已释放的内存写，
    // 表现为大网格上随机崩溃（历史 bug：t03 13 万三角形在这里崩）。
    _nodes[self].bmin.set(std::numeric_limits<double>::max(),
                          std::numeric_limits<double>::max(),
                          std::numeric_limits<double>::max());
    _nodes[self].bmax.set(-std::numeric_limits<double>::max(),
                  -std::numeric_limits<double>::max(),
                  -std::numeric_limits<double>::max());
    for (std::size_t i = begin; i < end; ++i)
    {
        const std::array<std::size_t, 3>& t = _tris[order[i]];
        for (int k = 0; k < 3; ++k)
        {
            const osg::Vec3d& v = _verts[t[k]];
            _nodes[self].bmin.x() = std::min(_nodes[self].bmin.x(), v.x());
            _nodes[self].bmin.y() = std::min(_nodes[self].bmin.y(), v.y());
            _nodes[self].bmin.z() = std::min(_nodes[self].bmin.z(), v.z());
            _nodes[self].bmax.x() = std::max(_nodes[self].bmax.x(), v.x());
            _nodes[self].bmax.y() = std::max(_nodes[self].bmax.y(), v.y());
            _nodes[self].bmax.z() = std::max(_nodes[self].bmax.z(), v.z());
        }
    }
    const std::size_t count = end - begin;
    if (count <= 4 || depth > 40)
    {
        _nodes[self].begin = begin;
        _nodes[self].end = end;
        return self;
    }
    // 沿最长轴用质心中位数切分
    const double dx = _nodes[self].bmax.x() - _nodes[self].bmin.x();
    const double dy = _nodes[self].bmax.y() - _nodes[self].bmin.y();
    const double dz = _nodes[self].bmax.z() - _nodes[self].bmin.z();
    const int axis = (dx >= dy && dx >= dz) ? 0 : ((dy >= dz) ? 1 : 2);
    std::sort(order.begin() + begin, order.begin() + end,
              [this, axis](std::size_t a, std::size_t b) {
                  const std::array<std::size_t, 3>& ta = _tris[a];
                  const std::array<std::size_t, 3>& tb = _tris[b];
                  const osg::Vec3d ca = (_verts[ta[0]] + _verts[ta[1]] + _verts[ta[2]]) / 3.0;
                  const osg::Vec3d cb = (_verts[tb[0]] + _verts[tb[1]] + _verts[tb[2]]) / 3.0;
                  const double ka = (axis == 0) ? ca.x() : (axis == 1) ? ca.y() : ca.z();
                  const double kb = (axis == 0) ? cb.x() : (axis == 1) ? cb.y() : cb.z();
                  if (ka != kb) { return ka < kb; }
                  return a < b;
              });
    const std::size_t mid = begin + count / 2;
    _nodes[self].begin = begin;
    _nodes[self].end = end;
    _nodes[self].left = buildRecursive(order, begin, mid, depth + 1);
    _nodes[self].right = buildRecursive(order, mid, end, depth + 1);
    return self;
}

double MeshBVH::distanceSquaredRecursive(int nodeIdx, const osg::Vec3d& p, double best) const
{
    const Node& node = _nodes[nodeIdx];
    // 到盒子的最近距离平方下界
    double boxDistSq = 0.0;
    for (int k = 0; k < 3; ++k)
    {
        const double lo = (k == 0) ? node.bmin.x() : (k == 1) ? node.bmin.y() : node.bmin.z();
        const double hi = (k == 0) ? node.bmax.x() : (k == 1) ? node.bmax.y() : node.bmax.z();
        const double v = (k == 0) ? p.x() : (k == 1) ? p.y() : p.z();
        if (v < lo) { boxDistSq += (lo - v) * (lo - v); }
        else if (v > hi) { boxDistSq += (v - hi) * (v - hi); }
    }
    if (boxDistSq > best) { return best; }

    if (node.right < 0)
    {
        for (std::size_t i = node.begin; i < node.end; ++i)
        {
            const std::array<std::size_t, 3>& t = _tris[i];
            const double d = pointTriangleDistanceSq(p, _verts[t[0]], _verts[t[1]], _verts[t[2]]);
            if (d < best) { best = d; }
        }
        return best;
    }
    const double dl = distanceSquaredRecursive(node.left, p, best);
    if (dl < best) { best = dl; }
    const double dr = distanceSquaredRecursive(node.right, p, best);
    return std::min(best, dr);
}

double MeshBVH::distanceSquared(const osg::Vec3d& p) const
{
    if (_nodes.empty()) { return std::numeric_limits<double>::infinity(); }
    return distanceSquaredRecursive(0, p, std::numeric_limits<double>::max());
}

double MeshBVH::distance(const osg::Vec3d& p) const
{
    const double d2 = distanceSquared(p);
    return std::isfinite(d2) ? std::sqrt(d2) : std::numeric_limits<double>::infinity();
}

// ---------------------------------------------------------------------------
// GeometryErrorEvaluator
// ---------------------------------------------------------------------------
GeometryErrorEvaluator::DirectionResult
GeometryErrorEvaluator::evaluateDirection(const TriangleSoup& from, const TriangleSoup& to) const
{
    DirectionResult res;
    MeshBVH target;
    target.build(to);
    if (from.empty())
    {
        res.budgetExhausted = false;
        return res;
    }
    if (target.empty())
    {
        // 目标为空：距离无定义，用源包围盒对角线作为保守上界（明确标记）
        osg::Vec3d bmin(std::numeric_limits<double>::max(),
                        std::numeric_limits<double>::max(),
                        std::numeric_limits<double>::max());
        osg::Vec3d bmax(-std::numeric_limits<double>::max(),
                        -std::numeric_limits<double>::max(),
                        -std::numeric_limits<double>::max());
        for (const osg::Vec3d& v : from.v)
        {
            bmin.x() = std::min(bmin.x(), v.x()); bmax.x() = std::max(bmax.x(), v.x());
            bmin.y() = std::min(bmin.y(), v.y()); bmax.y() = std::max(bmax.y(), v.y());
            bmin.z() = std::min(bmin.z(), v.z()); bmax.z() = std::max(bmax.z(), v.z());
        }
        res.lower = (bmax - bmin).length();
        res.upper = res.lower;
        res.maxSample = res.lower;
        res.budgetExhausted = true;
        return res;
    }

    struct HeapItem {
        double key;        // upper bound
        Patch patch;
        bool operator<(const HeapItem& o) const { return key < o.key; }
    };

    auto patchRadius = [](const osg::Vec3d& a, const osg::Vec3d& b, const osg::Vec3d& c) {
        // 采样点集合 = 3 顶点 + 3 边中点 + 重心。
        // patch 内任意点到最近采样点的距离 <= 最长边 / 3（保守）。
        const double l0 = (b - a).length();
        const double l1 = (c - b).length();
        const double l2 = (a - c).length();
        return std::max(std::max(l0, l1), l2) / 3.0;
    };

    std::priority_queue<HeapItem> heap;
    std::size_t samples = 0;
    double globalLower = 0.0;

    // 初始采样密度自适应（性能 + 正确性同时保证）：
    // 采样预算是固定的，三角形越多每个三角形分到的初始采样越少。
    // 不做这层退化的话，13 万三角形的源面会产生 94 万个初始采样，
    // 单次 evaluate 要 100 秒以上（实测 t03：127s）。
    std::size_t perTri = _cfg.initialSamplesPerTriangle;
    if (perTri > 7) { perTri = 7; }
    if (perTri == 0) { perTri = 1; }
    const std::size_t budget = (_cfg.maxSamplesPerNode > 0) ? _cfg.maxSamplesPerNode : 1;
    while (perTri > 1 && from.size() * perTri > budget / 2) { --perTri; }

    // 未被初始采样的三角形不能直接丢掉，也不能按 0 计入——那样上界就不成立了。
    // 给它们一个"全局保守上界"：源面与目标面并集包围盒的对角线，真实距离不可能
    // 超过它。它们因此会排在优先队列最前面，被细分循环优先取样、快速收敛。
    osg::Vec3d fbmin(std::numeric_limits<double>::max(),
                     std::numeric_limits<double>::max(),
                     std::numeric_limits<double>::max());
    osg::Vec3d fbmax(-std::numeric_limits<double>::max(),
                     -std::numeric_limits<double>::max(),
                     -std::numeric_limits<double>::max());
    for (const osg::Vec3d& v : from.v)
    {
        fbmin.x() = std::min(fbmin.x(), v.x()); fbmax.x() = std::max(fbmax.x(), v.x());
        fbmin.y() = std::min(fbmin.y(), v.y()); fbmax.y() = std::max(fbmax.y(), v.y());
        fbmin.z() = std::min(fbmin.z(), v.z()); fbmax.z() = std::max(fbmax.z(), v.z());
    }
    const double globalConservative = (fbmax - fbmin).length();

    const std::size_t sampleEvery =
        (from.size() * perTri > budget / 2)
            ? (from.size() * perTri + budget / 2 - 1) / (budget / 2)
            : 1;
    if (sampleEvery > 1) { perTri = 1; }

    for (std::size_t t = 0; t < from.size(); ++t)
    {
        const osg::Vec3d& a = from.v[t * 3];
        const osg::Vec3d& b = from.v[t * 3 + 1];
        const osg::Vec3d& c = from.v[t * 3 + 2];
        Patch p{a, b, c, 0.0, 0.0, 0};
        if (t % sampleEvery == 0)
        {
            // 初始采样：3 顶点 + 3 边中点 + 重心
            const osg::Vec3d pts[7] = {
                a, b, c,
                (a + b) * 0.5, (b + c) * 0.5, (c + a) * 0.5,
                (a + b + c) / 3.0
            };
            for (std::size_t k = 0; k < perTri; ++k)
            {
                const double d = target.distance(pts[k]);
                p.maxSampleDist = std::max(p.maxSampleDist, d);
                res.all.push_back(d);
                ++samples;
            }
            globalLower = std::max(globalLower, p.maxSampleDist);
        }
        else
        {
            p.maxSampleDist = globalConservative;
        }
        p.radius = patchRadius(a, b, c);
        heap.push({p.maxSampleDist + p.radius, p});
    }

    res.samples = samples;
    double upper = heap.empty() ? 0.0 : heap.top().key;

    // 自适应细分：优先处理上界最高的 patch
    unsigned iterations = 0;
    while (!heap.empty() && iterations < _cfg.maxRefineIterations
           && samples < _cfg.maxSamplesPerNode)
    {
        const HeapItem top = heap.top();
        if (top.key <= globalLower + _cfg.toleranceMeters) { break; }
        heap.pop();

        const Patch& p = top.patch;
        if (p.depth > 24) { continue; }
        // 沿最长边细分（取最长边的中点，把三角形一分为二）
        const double l0 = (p.b - p.a).length2();
        const double l1 = (p.c - p.b).length2();
        const double l2 = (p.a - p.c).length2();
        Patch c1, c2;
        c1.depth = c2.depth = p.depth + 1;
        if (l0 >= l1 && l0 >= l2)
        {
            const osg::Vec3d m = (p.a + p.b) * 0.5;
            c1.a = p.a; c1.b = m; c1.c = p.c;
            c2.a = m; c2.b = p.b; c2.c = p.c;
        }
        else if (l1 >= l2)
        {
            const osg::Vec3d m = (p.b + p.c) * 0.5;
            c1.a = p.a; c1.b = p.b; c1.c = m;
            c2.a = p.a; c2.b = m; c2.c = p.c;
        }
        else
        {
            const osg::Vec3d m = (p.c + p.a) * 0.5;
            c1.a = p.a; c1.b = p.b; c1.c = m;
            c2.a = m; c2.b = p.b; c2.c = p.c;
        }
        for (int k = 0; k < 2; ++k)
        {
            Patch& np = (k == 0) ? c1 : c2;
            np.maxSampleDist = p.maxSampleDist;   // 继承父 patch 的已知采样最大值
            const osg::Vec3d pts[3] = {
                np.a, np.b, np.c,
            };
            // 只新增 3 个顶点（父 patch 的采样已计入）
            for (int q = 0; q < 3; ++q)
            {
                const double d = target.distance(pts[q]);
                np.maxSampleDist = std::max(np.maxSampleDist, d);
                res.all.push_back(d);
                ++samples;
            }
            globalLower = std::max(globalLower, np.maxSampleDist);
            np.radius = patchRadius(np.a, np.b, np.c);
            heap.push({np.maxSampleDist + np.radius, np});
        }
        ++iterations;
        upper = heap.empty() ? upper : heap.top().key;
    }

    if (!heap.empty())
    {
        // 剩余 patch 的最大上界
        upper = std::max(upper, heap.top().key);
    }
    res.lower = globalLower;
    res.upper = std::max(upper, globalLower);
    res.maxSample = globalLower;
    res.samples = samples;
    res.budgetExhausted = (samples >= _cfg.maxSamplesPerNode) || (iterations >= _cfg.maxRefineIterations);

    if (!res.all.empty())
    {
        std::vector<double> sorted = res.all;
        std::sort(sorted.begin(), sorted.end());
        res.p95 = sorted[static_cast<std::size_t>(sorted.size() * 0.95)];
        res.p99 = sorted[static_cast<std::size_t>(sorted.size() * 0.99)];
    }
    return res;
}

ErrorResult GeometryErrorEvaluator::evaluate(const TriangleSoup& source, const TriangleSoup& proxy) const
{
    ErrorResult out;
    if (source.empty() && proxy.empty())
    {
        out.lowerBoundM = out.upperBoundM = 0.0;
        out.certified = true;
        out.status = "EMPTY_BOTH";
        return out;
    }
    if (source.empty() || proxy.empty())
    {
        // 一侧为空：另一侧完全缺失，误差取非空侧的包围盒对角线（保守）
        const TriangleSoup& s = source.empty() ? proxy : source;
        osg::Vec3d bmin(std::numeric_limits<double>::max(),
                        std::numeric_limits<double>::max(),
                        std::numeric_limits<double>::max());
        osg::Vec3d bmax(-std::numeric_limits<double>::max(),
                        -std::numeric_limits<double>::max(),
                        -std::numeric_limits<double>::max());
        for (const osg::Vec3d& v : s.v)
        {
            bmin.x() = std::min(bmin.x(), v.x()); bmax.x() = std::max(bmax.x(), v.x());
            bmin.y() = std::min(bmin.y(), v.y()); bmax.y() = std::max(bmax.y(), v.y());
            bmin.z() = std::min(bmin.z(), v.z()); bmax.z() = std::max(bmax.z(), v.z());
        }
        const double diag = (bmax - bmin).length();
        out.lowerBoundM = diag * 0.5;
        out.upperBoundM = diag;
        out.maxSampleM = diag;
        out.p95M = diag;
        out.p99M = diag;
        out.boundLoose = true;
        out.certified = false;
        out.status = "ONE_SIDE_EMPTY";
        return out;
    }

    const DirectionResult s2p = evaluateDirection(source, proxy);
    const DirectionResult p2s = evaluateDirection(proxy, source);

    out.lowerBoundM = std::max(s2p.lower, p2s.lower);
    out.upperBoundM = std::max(s2p.upper, p2s.upper);
    out.maxSampleM = std::max(s2p.maxSample, p2s.maxSample);
    out.p95M = std::max(s2p.p95, p2s.p95);
    out.p99M = std::max(s2p.p99, p2s.p99);
    out.samplesUsed = s2p.samples + p2s.samples;
    out.boundLoose = s2p.budgetExhausted || p2s.budgetExhausted;

    const double gap = out.upperBoundM - out.lowerBoundM;
    out.certified = (gap <= _cfg.toleranceMeters)
                 || (out.upperBoundM > 0.0 && (gap / out.upperBoundM) < _cfg.certifyRatio);
    out.status = out.certified ? "CERTIFIED_BOUND_WITHIN_TOLERANCE"
                               : (out.boundLoose ? "BOUND_LOOSE" : "BOUND_NOT_CONVERGED");
    return out;
}
