#ifndef OSG_GIS_PLUGINS_PRODUCTION_PRODUCTION_PIPELINE_H
#define OSG_GIS_PLUGINS_PRODUCTION_PRODUCTION_PIPELINE_H
// 生产级主流程（手册第 2 节数据流）
//
// FBX/OBJ -> SourceFeatureRegistry + GeometryIR
//         -> GeometryMetricsCache + SeamRegistry
//         -> SpatialPartitioner (top-down)
//         -> ProxyMeshBuilder (bottom-up)
//         -> GeometryErrorEvaluator
//         -> TileFeatureMapper + atlas/KTX2 + GLB
//         -> b3dm/i3dm
//         -> TileBoundsAggregator + tileset.json
//         -> Validator + report
#include "3dtiles/production/ProductionProfile.h"
#include "3dtiles/production/SourceModel.h"
#include "3dtiles/production/SpatialPartitioner.h"
#include "3dtiles/production/ProxyMeshBuilder.h"
#include "3dtiles/production/GeometryErrorEvaluator.h"
#include "3dtiles/production/TileFeatureMapper.h"
#include "utils/GltfOptimizer.h"
#include <osg/Node>
#include <osg/BoundingBox>
#include <osgDB/Options>
#include <map>
#include <string>
#include <vector>

namespace osgGISPlugins {
namespace production {

// ---------------------------------------------------------------------------
// SeamRegistry：跨瓦片边界顶点登记与简化后一致性校验（手册 8.3）
// ---------------------------------------------------------------------------
class SeamRegistry {
public:
    // 量化容差（米）：用于把浮点坐标映射成稳定 key
    explicit SeamRegistry(double tolerance = 1.0e-4) : _tol(tolerance) {}

    // 从源模型登记所有顶点位置
    void build(const SourceModel& model);

    // 给定每个 item 所属叶节点，找出"公共边的两个三角形被分到不同叶"的顶点
    // 返回跨瓦片 seam 顶点数
    std::size_t detectCrossTileSeams(const SourceModel& model,
                                     const std::map<std::uint32_t, int>& itemToLeaf);

    // 校验：proxy 里是否仍然包含这些 seam 位置（在容差内）
    // 返回 (保留数, 总数)
    std::pair<std::size_t, std::size_t> verifyPreserved(osg::Node* proxy,
                                                        const std::vector<std::uint32_t>& items) const;

    std::size_t seamVertexCount() const { return _seamPositions.size(); }

private:
    double _tol;
    std::vector<osg::Vec3d> _seamPositions;
    std::map<std::uint32_t, std::vector<std::size_t>> _itemSeams;  // item -> seam index
    std::int64_t key(const osg::Vec3d& p) const;
};

// ---------------------------------------------------------------------------
// 最终输出树的一个节点
// ---------------------------------------------------------------------------
struct ProductionTile {
    int id = 0;
    std::string uri;
    bool hasContent = false;
    bool isLeaf = false;
    bool isProxy = false;
    bool aggregate = false;

    osg::BoundingBox contentBounds;   // Y-up 模型坐标
    osg::BoundingBox subtreeBounds;   // 含后代，后序聚合
    osg::BoundingBox declaredBounds;  // 写入 JSON 的包围体（= subtreeBounds ∪ content）

    double measuredProxyErrorM = 0.0;
    double errorLowerM = 0.0;
    double errorUpperM = 0.0;
    double geometricErrorM = 0.0;
    std::string errorStatus = "NOT_EVALUATED";

    unsigned triangles = 0;
    std::uint64_t contentBytes = 0;

    std::vector<int> children;
    std::string refine = "REPLACE";
    std::vector<FeatureId> featureOrder;
    std::map<FeatureId, std::vector<FeatureId>> aggregateSources;
};

// ---------------------------------------------------------------------------
// 主流程
// ---------------------------------------------------------------------------
class ProductionPipeline {
public:
    struct Options {
        std::string outputDir;
        // 暂存目录：所有瓦片先写到这里，自校验通过后才搬到 outputDir。
        // 留空表示用 <outputDir>/.wb-staging。
        std::string stagingDir;
        double latitude = 30.0;
        double longitude = 116.0;
        double altitude = 300.0;
        osg::ref_ptr<osgDB::Options> writeOptions = new osgDB::Options;
        unsigned gltfOptimizerOptions = GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS
                                      | GltfOptimizer::FLATTEN_TRANSFORMS;
        GltfOptimizer::GltfTextureOptimizationOptions textureOptions;
        std::string textureCachePath;
        bool vertexCompression = false;
    };

    explicit ProductionPipeline(const ProductionProfile& profile);

    bool run(osg::Node* input, const Options& opt);

    // 手册 14.17：瓦片全部写进暂存目录后，写 tileset/manifest/report 并做自校验，
    // 通过才把内容搬到 outputDir；任何一步失败都丢弃暂存内容，
    // 绝不在 outputDir 里留下半成品 tileset.json。
    bool finalize();

    // 丢弃暂存目录（失败路径调用）
    void discardStaging();

    const ConversionReport& report() const { return _report; }
    const std::vector<ProductionTile>& tiles() const { return _tiles; }
    const std::vector<std::string>& failures() const { return _failures; }
    const std::vector<std::string>& warnings() const { return _warnings; }
    int rootIndex() const { return _root; }

    // 输出 tileset.json（1.0）
    bool writeTilesetJson(const std::string& path) const;
    bool writeManifest(const std::string& path) const;
    bool writeReport(const std::string& path) const;

    // 自校验：读回刚写出的 tileset.json，核对 URI 存在、预算、误差单调性。
    // 非 const：失败项要记进 _failures，供调用方打印。
    bool selfValidate(const std::string& tilesetJsonPath);

private:
    ProductionProfile _profile;
    Options _opt;
    SourceModel _model;
    // 权威源单元表：初值是 _model.prims 的副本，三角形级切分往里追加新单元。
    // 分区器、代理构建、物化、误差评估必须共用这一张表来解析 item id。
    std::vector<PrimRef> _prims;
    SpatialPartitioner* _partitioner = nullptr;
    SeamRegistry _seams;
    TileFeatureMapper _mapper;
    ConversionReport _report;
    std::vector<ProductionTile> _tiles;
    std::vector<std::string> _failures;
    std::vector<std::string> _warnings;
    int _root = -1;

    int buildB3dmTree(int partitionNode);
    std::map<int, int> _pnToTileId;   // partition 节点 -> tile id（并行预分配）
    int buildInstanceTree();
    void aggregateBounds(int tile);
    void assignGeometricError(int tile);

    // ---- tile 间并行（性能优化，结果与串行版一致）----
    // 原 buildB3dmTree 每个节点做三件重活：materialize/代理简化、
    // 双向 Hausdorff 误差评估、写出 b3dm。前两件只读共享源数据、
    // 输出独立节点，天然可并行；写出涉及纹理导出缓存与聚合统计，
    // 保持主线程串行。
    // 1) 递归预分配 tile id 与父子骨架（保证 id 语义与串行版一致）
    int assignTileIdsB3dm(int pn);
    // 2) 并行重活：几何构建 + 误差评估（无共享写）
    struct TileHeavyWork {
        osg::ref_ptr<osg::Node> content;
        bool isProxy = false;
        bool aggregate = false;
        bool leaf = false;
        bool texturesDropped = false;  // 内节点代理预估超限，构建时即去纹理
        std::map<FeatureId, std::vector<FeatureId>> aggregateSources;
        double measuredProxyErrorM = 0.0;
        double errorLowerM = 0.0;
        double errorUpperM = 0.0;
        std::string errorStatus;
        bool certified = false;
        bool boundLoose = false;
        double p95M = 0.0;
        std::string failure;   // proxy 生成失败原因
    };
    TileHeavyWork processTileHeavy(int pn, int id);
    // 3) 主线程串行收尾：writeContent + 状态聚合 + seam 校验
    void finalizeTileFromWork(int pn, int id, TileHeavyWork&& w);

    // 写出内容并测量真实字节数
    struct WriteOutcome {
        bool ok = false;
        std::uint64_t bytes = 0;
        unsigned triangles = 0;
        unsigned batchLength = 0;
        std::vector<FeatureId> featureOrder;
        std::string error;
    };
    WriteOutcome writeContent(osg::Node* node, const std::string& relUri, bool isInstanceTile);

    std::uint64_t _peakRss = 0;
};

} // namespace production
} // namespace osgGISPlugins
#endif
