#ifndef OSG_GIS_PLUGINS_PRODUCTION_SPATIAL_PARTITIONER_H
#define OSG_GIS_PLUGINS_PRODUCTION_SPATIAL_PARTITIONER_H
// 确定性自适应二叉空间 BVH（手册第 6 节）
//
// * 不再"整个模型固定选 KD/Quad/Octree 之一"，每个节点独立选轴与切分位置；
// * 优先对象级切分；单个对象超预算才降到 primitive 级；单个 primitive 仍超预算
//   才做三角形级切分（默认不沿平面切开三角形，只把整三角形按质心归属某个 child）；
// * 代价函数与 tie-break 全部确定，保证同样输入得到同样树（手册 6.4）。
#include "3dtiles/production/ProductionProfile.h"
#include "3dtiles/production/SourceModel.h"
#include <osg/BoundingBox>
#include <cstdint>
#include <string>
#include <vector>

namespace osgGISPlugins {
namespace production {

struct PartitionNode {
    int parent = -1;
    std::vector<int> children;
    // 本节点代表的全部源单元（内部节点也保留，用于生成子树代理，手册 7.1）
    std::vector<std::uint32_t> items;
    osg::BoundingBox bounds;
    unsigned depth = 0;

    // 资源负载
    std::uint64_t triangles = 0;
    std::uint64_t vertices = 0;
    std::uint64_t primitiveCount = 0;
    std::uint64_t materialCount = 0;
    std::uint64_t byteEstimate = 0;

    bool overHard = false;
    int splitAxis = -1;
    double splitPos = 0.0;
    int splitLevel = -1;   // 0 object / 1 primitive / 2 triangle
};

class SpatialPartitioner {
public:
    // primTable 是**外部持有**的可写单元表：初值是 SourceModel::prims 的副本，
    // 三角形级切分会往里面追加新单元。
    // 关键：这些新单元的 id 只在这张表里有效，因此 materialize / proxy / 误差评估
    // 全都必须用同一张表解析 item id，不能再用 SourceModel::prims（它只有原始单元）。
    SpatialPartitioner(const SourceModel& model, const ProductionProfile& profile,
                       std::vector<PrimRef>& primTable);

    // 返回 root 下标，失败返回 -1（失败原因写入 failures）
    int build();

    // 权威单元表（外部持有，这里是引用）
    std::vector<PrimRef>& prims;

    std::vector<PartitionNode> nodes;
    std::vector<std::string> failures;
    std::vector<std::string> warnings;

    // 手册 6.5：子节点单元集合并 == 父节点集合；无空节点
    bool verifyCoverage();

    // 该节点子树所有单元
    void subtreeItems(int node, std::vector<std::uint32_t>& out) const;

private:
    struct Load {
        std::uint64_t tri = 0, vert = 0, prim = 0, mat = 0, bytes = 0;
        double maxRatio(const ResourceBudget& b) const;
    };

    struct Candidate {
        int axis = 0;
        double plane = 0.0;
        int level = 0;             // 0/1/2
        std::vector<std::uint32_t> left;
        std::vector<std::uint32_t> right;
        double cost = 0.0;
        double maxRatioAfter = 0.0;
        bool valid = false;
        // tie-break 键
        std::uint64_t tieKey = 0;
    };

    const SourceModel& _model;
    ProductionProfile _profile;

    Load computeLoad(const std::vector<std::uint32_t>& items) const;
    osg::BoundingBox computeBounds(const std::vector<std::uint32_t>& items) const;
    double surfaceArea(const osg::BoundingBox& bb) const;

    void evaluateCandidate(const std::vector<std::uint32_t>& items,
                           const osg::BoundingBox& parentBounds,
                           int axis, double plane, int level,
                           const std::vector<int>& groupOf,   // 每 item 所属"不可分单位"
                           Candidate& out) const;

    void generateCandidates(const std::vector<std::uint32_t>& items,
                            const osg::BoundingBox& bounds,
                            int level,
                            std::vector<Candidate>& out) const;

    // 三角形级：把单个 item 的三角形集合切成两半，返回新追加的两个 item id
    bool splitItemByTriangles(std::uint32_t itemId, int axis,
                              std::uint32_t& outLeft, std::uint32_t& outRight);

    int recursiveBuild(const std::vector<std::uint32_t>& items, unsigned depth, int parent);

    // 子节点建完后回填本节点的 items/bounds/负载（保证"子集合并 == 父集合"）
    void refreshItemsFromChildren(int self);

    int _nodeCounter = 0;
};

} // namespace production
} // namespace osgGISPlugins
#endif
