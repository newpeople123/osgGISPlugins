#ifndef OSG_GIS_PLUGINS_PRODUCTION_GEOMETRY_ERROR_EVALUATOR_H
#define OSG_GIS_PLUGINS_PRODUCTION_GEOMETRY_ERROR_EVALUATOR_H
// 双向表面距离评估器（手册第 9 节）
//
// E = max( sup_{x in S} d(x,P), sup_{y in P} d(y,S) )
//
// * 输入是米制规范化坐标下的两组三角形（源表面 S 与代理表面 P）；
// * 用 AABB BVH 做精确点到三角形最近距离查询；
// * 初始采样（顶点/边中点/重心）给出下界，patch 覆盖半径给出保守上界；
//   对误差热区沿最长边递归细分，直到区间收敛或达到采样预算；
// * 输出 errorLowerBoundM / errorUpperBoundM / P95 / P99 / 状态标记。
#include "3dtiles/production/ProductionProfile.h"
#include <osg/Vec3>
#include <osg/Vec3d>
#include <array>
#include <cstddef>
#include <vector>

namespace osgGISPlugins {
namespace production {

// 节点内并行预算。tile 之间已经有 WB_JOBS 路并行，评估器只有在并行度
// 掉下来（大瓦片 straggler 独占机器）时才该往节点内扩线程，否则就是
// 过订阅。管线每进入/退出一个瓦片任务登记一次，评估器按剩余空闲核分配。
class TileTaskScope {
public:
    TileTaskScope();
    ~TileTaskScope();
    TileTaskScope(const TileTaskScope&) = delete;
    TileTaskScope& operator=(const TileTaskScope&) = delete;
};

struct TriangleSoup {
    std::vector<osg::Vec3d> v;   // 每 3 个为一个三角形
    std::size_t size() const { return v.size() / 3; }
    bool empty() const { return v.empty(); }
    void add(const osg::Vec3d& a, const osg::Vec3d& b, const osg::Vec3d& c)
    {
        v.push_back(a); v.push_back(b); v.push_back(c);
    }
    void clear() { v.clear(); }
};

// 轻量 AABB BVH，支持点到三角形最近距离
class MeshBVH {
public:
    void build(const TriangleSoup& soup);
    bool empty() const { return _tris.empty(); }

    // 返回 soup 中最近三角形的距离平方；未构建返回 inf
    double distanceSquared(const osg::Vec3d& p) const;
    double distance(const osg::Vec3d& p) const;

    std::size_t triangleCount() const { return _tris.size(); }

private:
    struct Node {
        osg::Vec3d bmin, bmax;
        int left = -1, right = -1;      // -1 表示叶
        std::size_t begin = 0, end = 0; // 叶区间
    };
    std::vector<Node> _nodes;
    std::vector<std::array<std::size_t, 3>> _tris;  // 索引到 _verts
    std::vector<osg::Vec3d> _verts;

    int buildRecursive(std::vector<std::size_t>& order, std::size_t begin, std::size_t end, int depth);
    double distanceSquaredRecursive(int node, const osg::Vec3d& p, double best) const;
};

struct ErrorResult {
    double lowerBoundM = 0.0;      // 采样得到的下界
    double upperBoundM = 0.0;      // 保守上界（含 patch 覆盖半径）
    double p95M = 0.0;
    double p99M = 0.0;
    double maxSampleM = 0.0;
    std::size_t samplesUsed = 0;
    double bvhBuildSeconds = 0.0;  // 仪表：双向目标侧建树总耗时
    double workerCpuSeconds = 0.0; // 仪表：节点内并行 worker 线程 CPU 秒（不含调用线程）
    bool certified = false;        // 区间收敛到容差内
    bool boundLoose = false;       // 达到采样预算，上界仍然保守
    const char* status = "UNKNOWN";

    // 写入 tileset 用的保守几何误差（永远是 upper bound，不允许写小）
    double geometricErrorM() const { return upperBoundM; }
};

class GeometryErrorEvaluator {
public:
    explicit GeometryErrorEvaluator(const ErrorEvalConfig& cfg) : _cfg(cfg) {}

    // 双向评估
    ErrorResult evaluate(const TriangleSoup& source, const TriangleSoup& proxy) const;

    // 单向：从 A 采样查询 B（供单元测试与报告用）
    struct DirectionResult {
        double lower = 0.0, upper = 0.0, p95 = 0.0, p99 = 0.0, maxSample = 0.0;
        std::size_t samples = 0;
        bool budgetExhausted = false;
        double bvhBuildSeconds = 0.0;   // 仪表：目标侧建树耗时，不参与判定
        double workerCpuSeconds = 0.0;  // 仪表：分块 worker 线程的 CPU 秒之和（不含父线程）
        std::vector<double> all;
    };
    DirectionResult evaluateDirection(const TriangleSoup& from, const TriangleSoup& to) const;

private:
    ErrorEvalConfig _cfg;

    struct Patch {
        osg::Vec3d a, b, c;          // 当前 patch 三角形（细分后为子三角形）
        double maxSampleDist = 0.0;  // patch 内已取样点的最大距离
        double radius = 0.0;         // patch 内任意点到最近取样点的保守上界
        int depth = 0;
    };
};

} // namespace production
} // namespace osgGISPlugins
#endif
