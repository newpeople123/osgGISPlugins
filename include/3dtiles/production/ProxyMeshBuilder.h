#ifndef OSG_GIS_PLUGINS_PRODUCTION_PROXY_MESH_BUILDER_H
#define OSG_GIS_PLUGINS_PRODUCTION_PROXY_MESH_BUILDER_H
// 自底向上 HLOD 代理生成（手册第 7 节）
//
// * 代理永远从该节点的"完整源几何"生成，绝不拿已简化的子代理继续简化（7.1）；
// * 先尝试身份保持型代理：每个 globalFeatureId 一个 Geode，禁止跨 feature 折叠；
// * 简化用 meshoptimizer，按三角形预算逐步加压，锁边界保护跨瓦片接缝；
// * 预算仍不满足时启用空间聚类聚合代理，产生明确的 proxyFeatureId 与源集合映射；
// * 严格身份模式下不允许聚合，直接返回失败（8.2）。
#include "3dtiles/production/ProductionProfile.h"
#include "3dtiles/production/SourceModel.h"
#include <osg/Node>
#include <map>
#include <vector>

namespace osgGISPlugins {
namespace production {

// 代理节点的内容字节估算，纹理按 osg::Image 指针去重（同一张纹理被 N 个
// primitive 引用只算一次）。纹理边长下限用 ProductionProfile.h 的
// proxyTextureFloorEdgePx。
struct ProxyBytes {
    std::uint64_t geo = 0;      // 几何：200 B/三角形 + 48 B/顶点
    std::uint64_t texFull = 0;  // 纹理按原始分辨率（解码 RGBA8 的 1/8 折算）
    std::uint64_t texFloor = 0; // 同一批纹理压到 proxyTextureFloorEdgePx 后
};

struct ProxyResult {
    osg::ref_ptr<osg::Node> node;
    bool identity = false;         // 未做简化（原样几何）
    bool aggregate = false;        // 使用了聚合代理
    // 纹理降到 proxyTextureFloorEdgePx 后代理是否仍装得下硬字节上限。
    // 装不下时调用方才把代理建成无纹理版（最后手段）；装得下就保纹理，
    // 由写出阶段按 proxyTextureMaxEdgePx 降分辨率。
    bool fitsWithFloorTextures = true;
    unsigned triangles = 0;
    unsigned vertices = 0;
    double appliedRatio = 1.0;
    std::string failure;
    // aggregate 时：proxyFeatureId -> 源 globalFeatureId 集合
    std::map<FeatureId, std::vector<FeatureId>> aggregateSources;
    // 阶段内细分仪表（手册 13.3"各阶段耗时"）：不参与任何判定，纯观测
    struct Metrics {
        double simplifySeconds = 0.0;   // 身份保持型代理的 simplifyNode 累计
        double aggregateSeconds = 0.0;  // 聚合 fallback 全部候选累计（含每候选简化）
        unsigned aggregateCandidates = 0;
    } metrics;
};

class ProxyMeshBuilder {
public:
    // primTable 必须是与 SpatialPartitioner 共用的那张单元表（含切分出来的新单元）
    ProxyMeshBuilder(const SourceModel& model, const ProductionProfile& profile,
                     const std::vector<PrimRef>& primTable);

    ProxyResult build(const std::vector<std::uint32_t>& items,
                      unsigned targetTriangles,
                      bool allowAggregate) const;

    static unsigned countTriangles(osg::Node* node);

    // 去重后的代理内容字节估算。build() 用它决定是否还能保住纹理，
    // 单元测试用它验证共享纹理不会被按 primitive 重复计数。
    static ProxyBytes estimateBytes(osg::Node* node);

private:
    // 按目标三角形数简化（每个 drawable 独立，误差上限递增直到达标）
    // lockBorders=false 用于聚合代理的整体挤压：聚合已经放弃对象身份，
    // 再锁 cell 边界会让"单个巨型 mesh 被切成多 cell"这类输入完全压不动。
    double simplifyNode(osg::Node* node, unsigned targetTriangles,
                        double targetErrorMeters, bool lockBorders) const;

    // 空间聚类聚合代理：按 cell 抽稀源 feature 后生成低面代理
    // mergeIntoSingleDrawable=true 时把同材质几何并成一个 drawable（drawcall 更少），
    // false 时保留 feature 级几何（顶点不重合，简化行为更可预期）
    // triangleBudget：本代理允许的三角形总量（已按字节硬限折算过，见 build()）
    osg::ref_ptr<osg::Node> buildAggregate(const std::vector<std::uint32_t>& items,
                                           double cellSize,
                                           std::map<FeatureId, std::vector<FeatureId>>& sources,
                                           bool mergeIntoSingleDrawable,
                                           unsigned triangleBudget) const;

    const SourceModel& _model;
    const std::vector<PrimRef>& _prims;
    ProductionProfile _profile;
};

} // namespace production
} // namespace osgGISPlugins
#endif
