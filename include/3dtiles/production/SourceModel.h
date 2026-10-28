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
// 越界顶点索引的一次丢弃记录（一个 primitiveSet 一条）
//
// 索引由外部读取器直接写进 DrawElements（osgdb_obj 的 `f` 行就是 1 基索引，
// 越界面在历史上真实存在），而 SourceModel 之后每一个 (*pos)[idx] 解引用都不
// 做边界比较：A-56 实测一个越界三角形把一个垃圾顶点送进包围盒 / 质心 /
// geometricError，切分树随之漂移（t06 31→33 瓦片、t11 半轴 -2→-6.83）。
//
// 语义（用户裁定）：丢弃越界三角形 + 显式记账 + 计入报告。不钳位、不伪造几何、
// 不静默丢弃。
// ---------------------------------------------------------------------------
struct IndexViolation
{
    FeatureId globalFeatureId = INVALID_FEATURE;
    std::string nodeName;
    std::string nodePath;
    unsigned drawableIndex = 0;        // 该 Geode 内的 drawable 序号
    unsigned primitiveSetIndex = 0;    // 该 Geometry 内的 primitiveSet 序号
    unsigned vertexCount = 0;          // 顶点数组长度（合法上界）
    unsigned droppedTriangles = 0;     // 因引用越界索引而被丢弃的三角形数
    unsigned maxOffendingIndex = 0;    // 观测到的最大越界索引
};

// ---------------------------------------------------------------------------
// 非有限 / 浮点溢出顶点的一次丢弃记录（一个 drawable 一条）
//
// 世界变换在 double 里做（`osg::Vec3d`），写回顶点数组时降到 float。两者不是
// 同一件事：1e39 在 double 里是有限值，转成 float 就是 inf。旧实现只查了
// double 这一侧（`std::isfinite(w.x())`），所以"double 有限 → float 溢出"的
// 顶点能一路带着 inf 进包围盒、切分器、BVH 与 glTF POSITION。
//
// 语义（与 IndexViolation 一致）：整条 drawable 丢弃 + 显式记账 + 计入报告。
// 不做裁剪、不做替换、不伪造几何。
// ---------------------------------------------------------------------------
struct NonFiniteGeometry
{
    FeatureId globalFeatureId = INVALID_FEATURE;
    std::string nodeName;
    std::string nodePath;
    unsigned drawableIndex = 0;
    unsigned vertexCount = 0;        // 该 drawable 声明的顶点数
    unsigned droppedTriangles = 0;   // 随整条 drawable 一起丢掉的三角形数
    unsigned nonFiniteDoubles = 0;   // 世界变换后就是 NaN/Inf 的顶点
    unsigned floatOverflows = 0;     // double 有限、转 float 后非有限的顶点
    unsigned nonFiniteNormals = 0;   // 法线变换/归一化/降精度后非有限的条目
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
    // build() 期间丢弃的越界索引记录，按遍历顺序（确定性）追加；
    // ProductionPipeline 把它们折进 conversion-report 的 counters + warnings。
    std::vector<IndexViolation> indexViolations;
    // build() 期间丢弃的非有限顶点 drawable（同上，逐条追加）
    std::vector<NonFiniteGeometry> nonFiniteGeometry;
    // 非有限法线（未导致 drawable 丢弃）：置零后交给 A-43 的零法线重建，
    // 计数进报告，不静默吞掉。
    unsigned nonFiniteNormalsZeroed = 0;


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

    // A-43：源 FBX 的顶点法线数组里带零长度向量，osg::Vec3::normalize() 对它无解，
    // 忠实透传进 glTF 后官方 3d-tiles-validator 判 severity=ERROR（发布闸不过）。
    // 这里在物化出三角形拓扑之后重建：取该顶点入射非退化面的面积加权法线；顶点自身
    // 入射面全退化时退到本图元的面积加权平均法线；两者都取不到才保持原值（不编造）。
    // "非退化"用与 scripts/zero_normal_triage.js 主判据同一条尺子：面积 > 1e-6 ×
    // 本批三角形中位面积（绝对阈值跨尺度会翻转结论，A-43 里实测过）。
    struct NormalRepairCounts
    {
        int fromFaces = 0;       // 由入射非退化面修复
        int fromPrimitive = 0;   // 由本图元平均面法线兜底
        int leftZero = 0;        // 仍然不可用：产物会被 validator 判 ERROR
    };
    static NormalRepairCounts repairZeroNormals(const osg::Vec3Array* pos,
                                                osg::Vec3Array* nrm,
                                                const std::vector<unsigned>& tris);

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
