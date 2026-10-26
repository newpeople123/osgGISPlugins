#include "3dtiles/production/GeometryErrorEvaluator.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <limits>
#include <thread>
#include <vector>
#ifdef _WIN32
#define NOMINMAX          // 否则 windows.h 的 min/max 宏吃掉 std::numeric_limits<>::max()
#include <windows.h>
#endif

using namespace osgGISPlugins;
using namespace osgGISPlugins::production;

namespace {

#ifdef _WIN32
// 当前线程已累计的 CPU 秒（user + kernel）。仪表要用 CPU 秒而不是壁钟：
// 初始采样这一段是多线程跑的，8 个 chunk 的壁钟重叠但 CPU 秒相加才是
// 进程真实用量，只有前者能和 GetProcessTimes 的 user 秒对上账。
double threadCpuSeconds()
{
    FILETIME c, e, k, u;
    if (!GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u)) { return 0.0; }
    const auto toSec = [](const FILETIME& ft) {
        ULARGE_INTEGER v;
        v.LowPart = ft.dwLowDateTime;
        v.HighPart = ft.dwHighDateTime;
        return static_cast<double>(v.QuadPart) / 1.0e7;
    };
    return toSec(u) + toSec(k);
}
#else
double threadCpuSeconds() { return 0.0; }
#endif

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

// 点到 AABB 的最近距离平方。盒子里装着三角形的全部顶点，而三角形是顶点的凸包，
// 所以这个值 ≤ 盒内任何三角形到 p 的真实距离 —— 它是可以拿来剪枝的下界。
double boxDistanceSquared(const osg::Vec3d& bmin, const osg::Vec3d& bmax, const osg::Vec3d& p)
{
    double d = 0.0;
    for (int k = 0; k < 3; ++k)
    {
        const double lo = (k == 0) ? bmin.x() : (k == 1) ? bmin.y() : bmin.z();
        const double hi = (k == 0) ? bmax.x() : (k == 1) ? bmax.y() : bmax.z();
        const double v = (k == 0) ? p.x() : (k == 1) ? p.y() : p.z();
        if (v < lo) { d += (lo - v) * (lo - v); }
        else if (v > hi) { d += (v - hi) * (v - hi); }
    }
    return d;
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

    // 建树的重排只作用在 order 上，而叶区间存的是 order 的下标：不把这层置换
    // 真正落到 _tris 上，叶子就会按位置读到不属于这个盒子的三角形——查询结果
    // 只会偏大（读到的是同 soup 里别的面片，不会偏小），但实测能把最近距离放大
    // 3 倍，于是 geometricError、errorMax、p95 全部虚高，代理会被判得比实际粗糙，
    // 收敛判定也不成立。数组排序后 _tris[i] 与盒子覆盖的三角形集合一致。
    std::vector<std::array<std::size_t, 3>> sorted(n);
    for (std::size_t i = 0; i < n; ++i) { sorted[i] = _tris[order[i]]; }
    _tris.swap(sorted);
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
    if (boxDistanceSquared(node.bmin, node.bmax, p) > best) { return best; }

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
    const double dl0 = boxDistanceSquared(_nodes[node.left].bmin, _nodes[node.left].bmax, p);
    const double dr0 = boxDistanceSquared(_nodes[node.right].bmin, _nodes[node.right].bmax, p);
    const bool leftFirst = (dl0 <= dr0);
    const int nearIdx = leftFirst ? node.left : node.right;
    const int farIdx  = leftFirst ? node.right : node.left;
    const double farBound = leftFirst ? dr0 : dl0;

    // 近孩子优先（性能杠杆 L1）：先探盒子下界更近的一侧，best 会更早收紧，
    // 远侧往往在入口检查里直接被剪掉。
    //
    // 遍历顺序不改变结果，三条理由缺一不可：
    //   1) 盒子下界有效（见 boxDistanceSquared 注释），被剪掉的子树里不可能
    //      藏着比当前 best 更小的真实距离；
    //   2) 沿"真正最近那个三角形"的路径，盒子下界 ≤ 该三角形距离 ≤ 当时的 best，
    //      这条路径永远不会被剪，返回值必然取到全局最小；
    //   3) 两种顺序取到的是同一批 double 里的同一个最小元素，与顺序无关。
    // ⇒ 与旧的"固定左先"逐位相同，不需要产物对照也能在单测里证死。
    best = distanceSquaredRecursive(nearIdx, p, best);
    if (farBound <= best) { best = distanceSquaredRecursive(farIdx, p, best); }
    return best;
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
namespace {

std::atomic<int>& activeTileTasks()
{
    static std::atomic<int> n{1};
    return n;
}

// 节点内并行度。tile 之间已经有 WB_JOBS 路并行，单节点只吃剩下的核，
// 否则 8 个 tile job 各自再开满核线程会严重过订阅。
// WB_EVAL_THREADS 可显式覆盖（0/1 = 关闭），便于同时段 A/B。
unsigned intraNodeThreads()
{
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) { hw = 4; }
    const unsigned active = static_cast<unsigned>(
        std::max(1, activeTileTasks().load(std::memory_order_relaxed)));
    unsigned t = hw / active;
    if (const char* e = std::getenv("WB_EVAL_THREADS"))
    {
        const int v = std::atoi(e);
        if (v >= 0) { t = static_cast<unsigned>(v); }
    }
    return t < 1 ? 1u : t;
}

} // namespace

namespace osgGISPlugins {
namespace production {

TileTaskScope::TileTaskScope() { activeTileTasks().fetch_add(1, std::memory_order_relaxed); }
TileTaskScope::~TileTaskScope() { activeTileTasks().fetch_sub(1, std::memory_order_relaxed); }

} // namespace production
} // namespace osgGISPlugins

GeometryErrorEvaluator::DirectionResult
GeometryErrorEvaluator::evaluateDirection(const TriangleSoup& from, const TriangleSoup& to) const
{
    DirectionResult res;
    MeshBVH target;
    const auto tBvh = std::chrono::steady_clock::now();
    target.build(to);
    res.bvhBuildSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - tBvh).count();
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

    auto longestEdge = [](const osg::Vec3d& a, const osg::Vec3d& b, const osg::Vec3d& c) {
        const double l0 = (b - a).length();
        const double l1 = (c - b).length();
        const double l2 = (a - c).length();
        return std::max(std::max(l0, l1), l2);
    };

    std::priority_queue<HeapItem> heap;
    std::size_t samples = 0;
    double globalLower = 0.0;

    // 初始采样密度自适应，但三条底线不能让：
    //  1) 每个三角形的 3 个顶点必须查。上界靠的是 d(x,P) ≤ max_v d(v,P) + R(T)，
    //     R(T) = 三角形内点到顶点集的最大覆盖半径 = 外接半径（钝角时取最长边中点，
    //     值 ≤ L/2），而外接半径 = L/(2 sin θ_max) ≤ L/√3（θ_max ≥ 60°）。
    //  2) 只有 7 个点（3 顶点 + 3 边中点 + 重心）全采了，才允许沿用 L/3 那个
    //     更紧的覆盖半径；少采任何一个点 L/3 都不成立（旧实现在密度退化到 1~6
    //     时仍然用 L/3，上界会被写小）。
    //  3) 顶点预算本身超上限时也照采——正确性优先于 maxSamplesPerNode。
    //     大 soup 的目标面是万级三角形的代理，BVH 缓存友好，3 次/三角形可接受
    //     （卢沟桥 465 个代理瓦片子树合计 4.2e7 三角形，实测代价远小于原实现
    //      在预算里反复细分"没采样的洞"）。
    std::size_t perTri = _cfg.initialSamplesPerTriangle;
    if (perTri > 7) { perTri = 7; }
    const std::size_t budget = (_cfg.maxSamplesPerNode > 0) ? _cfg.maxSamplesPerNode : 1;
    const std::size_t cap = budget / 2;
    const std::size_t triCount = from.size();
    const std::size_t wantExtra = (perTri > 3) ? perTri - 3 : 0;   // 3 边中点 + 重心
    std::size_t extra = wantExtra;
    while (extra > 0 && triCount * (3 + extra) > cap) { --extra; }
    perTri = 3 + extra;
    // 覆盖半径：全采样集合（7 点）才是 L/3；只有顶点集合时只保证 L/√3。
    const double radiusFactor = (extra == 4) ? (1.0 / 3.0) : (1.0 / std::sqrt(3.0));

    auto patchRadius = [&](const osg::Vec3d& a, const osg::Vec3d& b, const osg::Vec3d& c) {
        return longestEdge(a, b, c) * radiusFactor;
    };

    // 顶点距离查询彼此完全独立，是大 soup 上的主要热开销：按块并行查询，块内结果
    // 写回固定下标缓冲，随后仍按原三角形顺序串行入堆——与单线程逐位一致（剪枝
    // 依赖 globalLower 的演化顺序，不能并行）。分块同时给缓冲封顶：330 万三角形
    // 的根节点整份存距离要 80 MB，十几个瓦片同时在算就是 GB 级峰值内存。
    const std::size_t block = 65536;
    // 分位数统计（p95/p99）只是报告口径，不参与判定。顶点强制采样后大 soup 的
    // 样本数可以到千万级，全留下来一个方向就是几十 MB、十几个瓦片并行就是 GB 级
    // 峰值，所以超过阈值时按固定间隔抽留（确定性抽样，同一输入同一结果）。
    // 样本数在阈值以内的节点仍然全留，与旧实现的分位数完全一致。
    const std::size_t quantileKeep = 262144;
    const std::size_t initialTotal = triCount * perTri;
    const std::size_t keepEvery = (initialTotal > quantileKeep)
        ? (initialTotal + quantileKeep - 1) / quantileKeep : 1;
    std::vector<double> initDist;
    double prunedUpper = 0.0;   // 被剪枝 patch 的 key，最终上界要把它算回去
    for (std::size_t base = 0; base < triCount; base += block)
    {
        const std::size_t hi = std::min(triCount, base + block);
        const std::size_t n = hi - base;
        initDist.assign(n * perTri, 0.0);

        auto queryBlock = [&](std::size_t lo, std::size_t stop, double* cpuOut) {
            const double cpu0 = threadCpuSeconds();
            for (std::size_t t = lo; t < stop; ++t)
            {
                const osg::Vec3d& a = from.v[t * 3];
                const osg::Vec3d& b = from.v[t * 3 + 1];
                const osg::Vec3d& c = from.v[t * 3 + 2];
                const osg::Vec3d pts[7] = {
                    a, b, c,
                    (a + b) * 0.5, (b + c) * 0.5, (c + a) * 0.5,
                    (a + b + c) / 3.0
                };
                double* out = &initDist[(t - base) * perTri];
                for (std::size_t k = 0; k < perTri; ++k)
                {
                    out[k] = target.distance(pts[k]);
                }
            }
            if (cpuOut != nullptr) { *cpuOut = threadCpuSeconds() - cpu0; }
        };
        const unsigned threads = (n * perTri >= 50000) ? intraNodeThreads() : 1u;
        if (threads > 1 && n >= 2)
        {
            const std::size_t chunk = (n + threads - 1) / threads;
            std::vector<std::size_t> starts;
            for (std::size_t lo = base; lo < hi; lo += chunk) { starts.push_back(lo); }
            // 只累加 worker 线程的 CPU 秒：父线程这段时间在 join，它自己的开销
            // 由调用侧的线程计时器计，两处不能重复加。
            std::vector<double> chunkCpu(starts.size(), 0.0);
            std::vector<std::thread> workers;
            for (std::size_t i = 0; i < starts.size(); ++i)
            {
                const std::size_t stop = std::min(hi, starts[i] + chunk);
                workers.emplace_back(queryBlock, starts[i], stop, &chunkCpu[i]);
            }
            for (std::thread& th : workers) { th.join(); }
            for (double s : chunkCpu) { res.workerCpuSeconds += s; }
        }
        else
        {
            queryBlock(base, hi, nullptr);
        }

        for (std::size_t t = base; t < hi; ++t)
        {
            const osg::Vec3d& a = from.v[t * 3];
            const osg::Vec3d& b = from.v[t * 3 + 1];
            const osg::Vec3d& c = from.v[t * 3 + 2];
            Patch p{a, b, c, 0.0, 0.0, 0};
            const double* dd = &initDist[(t - base) * perTri];
            const bool keep = (t % keepEvery == 0);
            for (std::size_t k = 0; k < perTri; ++k)
            {
                const double d = dd[k];
                p.maxSampleDist = std::max(p.maxSampleDist, d);
                if (keep) { res.all.push_back(d); }
                ++samples;
            }
            globalLower = std::max(globalLower, p.maxSampleDist);
            p.radius = patchRadius(a, b, c);
            const double key = p.maxSampleDist + p.radius;
            // key 已经不高于当时的全局下界 + 容差，而最终上界恒 ≥ 全局下界，所以它
            // 不可能决定最终上界，不必进堆——旧实现把它们全留下，大节点上是百万级的
            // 堆开销。key 仍然累进 prunedUpper：剪枝只能省算力，不能改数值口径
            // （否则就是"私自降低上界"）。
            if (key > globalLower + _cfg.toleranceMeters) { heap.push({key, p}); }
            else { prunedUpper = std::max(prunedUpper, key); }
        }
    }

    // 出账口径：最终上界 = 堆里剩余 patch 的最大 key ∪ 被剪枝/被深度上限截断的
    // patch 的 key。子 patch 精确覆盖父 patch，所以父 patch 一旦展开就不再计入
    // 上界（这是细分能把上界收紧的原因）；反过来，没展开就被丢弃的 patch 的 key
    // 必须留下，否则等于凭空降低上界。
    res.samples = samples;
    double upper = prunedUpper;

    // 自适应细分：优先处理上界最高的 patch
    unsigned iterations = 0;
    bool depthCapped = false;
    while (!heap.empty() && iterations < _cfg.maxRefineIterations
           && samples < _cfg.maxSamplesPerNode)
    {
        const HeapItem top = heap.top();
        if (top.key <= globalLower + _cfg.toleranceMeters) { break; }
        heap.pop();

        const Patch& p = top.patch;
        if (p.depth > 24)
        {
            depthCapped = true;
            upper = std::max(upper, top.key);
            continue;
        }
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
            // 子 patch 只查了自己的 3 个顶点，覆盖半径只能按顶点集合口径算（L/√3）。
            np.radius = longestEdge(np.a, np.b, np.c) / std::sqrt(3.0);
            heap.push({np.maxSampleDist + np.radius, np});
        }
        ++iterations;
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
    // 堆排空=细分完整收敛（或已无待办），此时预算耗尽与否都不影响上界；
    // 旧判据 samples>=max 在堆已空时会把完全收敛的评估误标 BOUND_LOOSE。
    // 触到细分深度上限的 patch 带的是没收敛完的保守 key，也算未收敛。
    res.budgetExhausted = depthCapped
        || (!heap.empty() && (samples >= _cfg.maxSamplesPerNode
                              || iterations >= _cfg.maxRefineIterations));

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
    out.bvhBuildSeconds = s2p.bvhBuildSeconds + p2s.bvhBuildSeconds;
    out.workerCpuSeconds = s2p.workerCpuSeconds + p2s.workerCpuSeconds;
    out.boundLoose = s2p.budgetExhausted || p2s.budgetExhausted;

    const double gap = out.upperBoundM - out.lowerBoundM;
    out.certified = (gap <= _cfg.toleranceMeters)
                 || (out.upperBoundM > 0.0 && (gap / out.upperBoundM) < _cfg.certifyRatio);
    out.status = out.certified ? "CERTIFIED_BOUND_WITHIN_TOLERANCE"
                               : (out.boundLoose ? "BOUND_LOOSE" : "BOUND_NOT_CONVERGED");
    return out;
}
