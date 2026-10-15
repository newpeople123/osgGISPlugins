#ifndef OSG_GIS_PLUGINS_PRODUCTION_SOURCE_MODEL_H
#define OSG_GIS_PLUGINS_PRODUCTION_SOURCE_MODEL_H
// SourceFeatureRegistry + GeometryIR + GeometryMetricsCache（手册 2 / 5.1 / 5.2 / 6.1）
//
// 职责：
//   * 遍历 OSG 场景树，为每个业务对象分配确定性 globalFeatureId；
//   * 把几何烘焙到统一的"规范化模型坐标"（世界矩阵乘进顶点），并切成
//     mesh / primitiveSet / triangle 三级可引用单元；
//   * 缓存每个单元的包围盒、质心、三角形数、材质 ID、字节估算；
//   * 识别重复实例（i3dm 用）。
//
// 不使用 OSG 指针地址作为持久 ID（手册 5.1）。ID 由遍历顺序 + 内容签名决定，
// 同一输入在同一配置下稳定。
#include <osg/Node>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Matrixd>
#include <osg/NodeVisitor>
#include <osg/BoundingBox>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace osgGISPlugins {
namespace production {

using FeatureId = std::uint32_t;
const FeatureId INVALID_FEATURE = 0xFFFFFFFFu;

struct SourceProperty {
    std::string key;
    std::string value;      // 统一序列化为字符串，保留原始类型名
    std::string typeName;   // "string"/"int"/"double"/... 便于回查
};

// ---------------------------------------------------------------------------
// 一个 primitiveSet 的三角形列表（下标指向 SourceMesh::positions）
// ---------------------------------------------------------------------------
struct SourcePrimSet {
    unsigned materialId = 0;
    std::vector<unsigned> tris;      // 每 3 个为一個三角形
};

// ---------------------------------------------------------------------------
// 归一化后的几何：顶点已烘焙世界变换；保存所有连续属性与通用顶点属性
// ---------------------------------------------------------------------------
struct SourceMesh {
    osg::ref_ptr<osg::Vec3Array> positions;
    osg::ref_ptr<osg::Vec3Array> normals;
    osg::ref_ptr<osg::Vec2Array> uv0;
    osg::ref_ptr<osg::Vec4Array> colors;
    // 通用顶点属性槽（下标即 OSG 的 vertex attrib index；空槽为 nullptr）
    std::vector<osg::ref_ptr<osg::Array>> attribs;
    std::vector<SourcePrimSet> primSets;
    osg::BoundingBox aabb;
    // 源材质/纹理引用（物化时必须带回去，否则 atlas/KTX2 全部丢失）
    osg::ref_ptr<osg::StateSet> stateSet;

    bool empty() const {
        if (!positions.valid() || positions->empty()) { return true; }
        for (const auto& ps : primSets) { if (!ps.tris.empty()) { return false; } }
        return true;
    }
};

// ---------------------------------------------------------------------------
// 业务对象（FBX 的一个节点 / OSG 的一个 Geode）
// ---------------------------------------------------------------------------
struct SourceObject {
    FeatureId globalFeatureId = INVALID_FEATURE;
    FeatureId parentFeatureId = INVALID_FEATURE;
    std::string name;
    std::string nodePath;
    osg::Matrixd worldMatrix;
    std::vector<unsigned> meshIds;
    std::vector<SourceProperty> properties;
    unsigned totalTris = 0;
    osg::BoundingBox aabb;
    bool instanced = false;          // 该对象被识别为重复实例的原型
};

// ---------------------------------------------------------------------------
// 重复实例
// ---------------------------------------------------------------------------
struct SourceInstance {
    std::uint32_t instanceId = 0;
    FeatureId globalFeatureId = INVALID_FEATURE;   // 实例自身的身份
    FeatureId prototypeFeatureId = INVALID_FEATURE;
    osg::Matrixd matrix;                            // 原型局部 -> 世界
    std::string name;
    std::vector<SourceProperty> properties;
};

// ---------------------------------------------------------------------------
// 可分割单位（手册 6.1 的 PartitionItem）
// ---------------------------------------------------------------------------
struct PrimRef {
    unsigned objectId = 0;
    unsigned meshId = 0;
    unsigned primSetIndex = 0;
    unsigned materialId = 0;
    FeatureId globalFeatureId = INVALID_FEATURE;
    // 该 primitiveSet 内被本单元持有的三角形下标（默认全部）
    std::vector<std::uint32_t> tris;

    // 缓存指标
    unsigned triCount = 0;
    unsigned vertexCount = 0;
    osg::BoundingBox aabb;
    osg::Vec3d centroid;
    std::uint64_t byteEstimate = 0;
    int splitLevel = 0;              // 0=object 1=primitive 2=triangle
};

// ---------------------------------------------------------------------------
// 源模型
// ---------------------------------------------------------------------------
class SourceModel {
public:
    std::vector<SourceObject> objects;
    std::vector<SourceMesh> meshes;
    std::vector<SourceInstance> instances;
    std::vector<PrimRef> prims;                 // 所有 primitive 级单元
    std::map<FeatureId, unsigned> featureToObject;
    osg::BoundingBox bounds;

    // 从 OSG 场景树构建（会烘焙世界变换）
    bool build(osg::Node* root);

    // 统计
    unsigned totalTriangles() const;
    unsigned totalVertices() const;
    unsigned materialCount() const;
    FeatureId featureCount() const { return static_cast<FeatureId>(objects.size()); }

    // 取某个 PrimRef 的第 t 个三角形的三个顶点下标
    void triangleIndices(const PrimRef& pr, std::uint32_t t, unsigned out[3]) const;

    // 取某个 PrimRef 覆盖的所有三角形顶点下标（3 每三角形）
    void collectTriangleIndices(const PrimRef& pr, std::vector<unsigned>& out) const;

    // 从一组 PrimRef 物化出 OSG 节点（每个 globalFeatureId 一个 Geode）
    // perFeatureGeodes=true 时按 feature 分 Geode，保证 _BATCHID 与 feature 一一对应
    //
    // primTable：item id 的解析表。默认是 this->prims（只有原始单元）。
    // 一旦调用方做过三角形级切分，就必须把那张"已追加新单元"的表传进来，
    // 否则新 id 会越界读到垃圾数据（历史 bug：t03 巨型单体网格在这里崩过）。
    osg::ref_ptr<osg::Node> materialize(const std::vector<PrimRef>& primTable,
                                        const std::vector<std::uint32_t>& itemIds,
                                        bool perFeatureGeodes) const;
    osg::ref_ptr<osg::Node> materialize(const std::vector<std::uint32_t>& itemIds,
                                        bool perFeatureGeodes = true) const
    {
        return materialize(prims, itemIds, perFeatureGeodes);
    }

    // 从一组 PrimRef 收集三角形（用于误差评估 / 代理构建）
    struct Triangle {
        osg::Vec3d a, b, c;
    };
    void collectTriangles(const std::vector<PrimRef>& primTable,
                          const std::vector<std::uint32_t>& itemIds,
                          std::vector<Triangle>& out) const;
    void collectTriangles(const std::vector<std::uint32_t>& itemIds,
                          std::vector<Triangle>& out) const
    {
        collectTriangles(prims, itemIds, out);
    }

    // 字节估算
    std::uint64_t estimateBytes(const std::vector<PrimRef>& primTable,
                                const std::vector<std::uint32_t>& itemIds) const;
    std::uint64_t estimateBytes(const std::vector<std::uint32_t>& itemIds) const
    {
        return estimateBytes(prims, itemIds);
    }

    const SourceObject* objectOf(FeatureId fid) const {
        auto it = featureToObject.find(fid);
        return it == featureToObject.end() ? nullptr : &objects[it->second];
    }
};

} // namespace production
} // namespace osgGISPlugins
#endif
