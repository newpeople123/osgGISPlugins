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

struct ProxyResult {
    osg::ref_ptr<osg::Node> node;
    bool identity = false;         // 未做简化（原样几何）
    bool aggregate = false;        // 使用了聚合代理
    unsigned triangles = 0;
    unsigned vertices = 0;
    double appliedRatio = 1.0;
    std::string failure;
    // aggregate 时：proxyFeatureId -> 源 globalFeatureId 集合
    std::map<FeatureId, std::vector<FeatureId>> aggregateSources;
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

private:
    // 按目标三角形数简化（每个 drawable 独立，锁边界），返回实际比例
    double simplifyNode(osg::Node* node, unsigned targetTriangles, double targetErrorMeters) const;

    // 空间聚类聚合代理
    osg::ref_ptr<osg::Node> buildAggregate(const std::vector<std::uint32_t>& items,
                                           double cellSize,
                                           std::map<FeatureId, std::vector<FeatureId>>& sources) const;

    const SourceModel& _model;
    const std::vector<PrimRef>& _prims;
    ProductionProfile _profile;
};

} // namespace production
} // namespace osgGISPlugins
#endif
