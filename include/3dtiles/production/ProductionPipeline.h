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
#include "3dtiles/production/ObjIntegrityPrecheck.h"
#include "utils/GltfOptimizer.h"
#include <osg/Node>
#include <osg/BoundingBox>
#include <osgDB/Options>
#include <map>
#include <string>
#include <vector>
#include <cstdint>

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
// 单瓦片子阶段计时（AUDIT A-47）
//
// 阶段级 micro_* 是全体瓦片的累加，看得到"评估占 97 %"但看不出关键路径：
// 一个 1 200 s 的 straggler 和 600 个 2 s 的瓦片在累加里无法区分，
// 而"最低能压到多少小时"只由前者决定。这里按瓦片落一份，纯观测，
// 不参与任何验收判定。
// ---------------------------------------------------------------------------
struct TileTimings {
    double heavySec = 0.0;        // processTileHeavy 全程
    double materializeSec = 0.0;  // 叶：_model.materialize
    double proxyBuildSec = 0.0;   // 内节点：ProxyMeshBuilder::build 全程
    double collectSec = 0.0;
    double evaluateSec = 0.0;
    double bvhBuildSec = 0.0;
    double writeSec = 0.0;        // 主线程 writeContent
    double finalizeSec = 0.0;     // 主线程 finalizeTileFromWork 全程
    // CPU 秒口径（本线程 GetThreadTimes 的 user+kernel）。只有这套能和进程
    // CPU 秒对账：壁钟那套在 8 路并行时把 8 份工作量记成 1 份。
    double heavyCpuSec = 0.0;
    double materializeCpuSec = 0.0;
    double proxyBuildCpuSec = 0.0;
    double collectCpuSec = 0.0;
    double evaluateCpuSec = 0.0;  // 含 worker 线程 CPU
    double evaluateWorkerCpuSec = 0.0;  // 只有 worker 线程，heavyCpuSec 之外的一份
    double writeCpuSec = 0.0;
    double finalizeCpuSec = 0.0;
    std::uint64_t evaluateSamples = 0;
    // 评估内部四段（A-55）：micro_evaluate 只说明"评估占九成"，分不清是
    // 距离查询、入堆剪枝、细分循环还是分位数排序在吃时间；重复覆盖同一批
    // 源三角形的工作量也只有 evalSourceTriangles 能直接看出来。
    double evalInitSec = 0.0;
    double evalPushSec = 0.0;
    double evalRefineSec = 0.0;
    double evalQuantileSec = 0.0;
    std::uint64_t evalRefineIterations = 0;
    std::uint64_t evalSourceTriangles = 0;
    std::uint64_t evalProxyTriangles = 0;

    json toJson() const;
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
    TileTimings timing;

    std::vector<int> children;
    std::string refine = "REPLACE";
    std::vector<FeatureId> featureOrder;
    std::map<FeatureId, std::vector<FeatureId>> aggregateSources;
    // 权威源身份范围：本 tile 分区单元（含切分后的新单元）里出现的
    // globalFeatureId 去重集合。featureOrder 是从 Geode 名字回查出来的，
    // 重名/聚合都会让它退化，所以覆盖校验必须用这一份而不是它。
    std::vector<FeatureId> sourceScope;
};

// ---------------------------------------------------------------------------
// 主流程
// ---------------------------------------------------------------------------
class ProductionPipeline {
public:
    struct Options {
        std::string outputDir;
        // 只用于报告身份（ConversionReport::inputFile/inputBytes）：管线自己不读这个路径，
        // 几何由 run(input, …) 的节点带进来。留空时报告如实记"未知"，不填假值。
        std::string sourceFile;
        // 读模**之前**的原始 OBJ 索引预检结果（AUDIT A-57）。管线不重扫文件，只负责把
        // 结果记进 conversion-report.json；ran=false 表示这次输入不是 OBJ。
        ObjPrecheckResult objPrecheck;
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

    // 超限瓦片的纹理降档阶梯（从 256 折到 proxyTextureFloorEdgePx）。
    // textureMaxEdgePx 是本瓦片首轮已经用过的边长上限：代理瓦片传 512 时档位只剩
    // 256/128/64，叶瓦片传 0 时一档都不能少（少了就等于废掉超限兜底）。
    // 单独成函数是为了让"叶瓦片必须保留全部档位"这条能被单元测试钉住——
    // 集成用例走的是三角形超限分支，观察不到字节阶梯。
    static std::vector<unsigned> textureDownscaleLadder(unsigned textureMaxEdgePx);

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

    // proxy_encode_error 阶段内细分仪表（手册 13.3"各阶段耗时"）：
    // 数值随 TileHeavyWork 带回，主线程收尾时串行累加；只观测，不参与任何判定。
    double _microProxySimplifySec = 0.0;
    double _microProxyAggregateSec = 0.0;
    double _microHeavySec = 0.0;
    double _microMaterializeSec = 0.0;
    double _microProxyBuildSec = 0.0;
    double _microWriteSec = 0.0;
    double _microEvaluateSec = 0.0;
    double _microCollectSec = 0.0;
    std::uint64_t _microAggregateCandidates = 0;
    std::uint64_t _microEvaluateSamples = 0;
    double _microBvhBuildSec = 0.0;
    // 评估内部四段与重复覆盖口径（A-55）
    double _microEvalInitSec = 0.0;
    double _microEvalPushSec = 0.0;
    double _microEvalRefineSec = 0.0;
    double _microEvalQuantileSec = 0.0;
    std::uint64_t _microEvalRefineIterations = 0;
    std::uint64_t _microEvalSourceTriangles = 0;
    std::uint64_t _microEvalProxyTriangles = 0;
    // 真实查询计数与 BVH 工作量（本轮 §6.1）：与 microEvaluateSamples 是两个口径
    std::uint64_t _microEvalDistanceCalls = 0;
    std::uint64_t _microEvalInitDistanceCalls = 0;
    std::uint64_t _microEvalRefineDistanceCalls = 0;
    std::uint64_t _microEvalBvhNodeVisits = 0;
    std::uint64_t _microEvalBvhTriangleTests = 0;
    double _microEvalInitParallelSec = 0.0;   // 含 worker 查询时间，不是纯开销
    double _microEvalInitThreadCreateSec = 0.0;
    double _microFinalizeSec = 0.0;
    // CPU 秒版累加器（AUDIT A-47：闭合"仪表 vs 进程 CPU 秒"这条算式）
    double _microHeavyCpuSec = 0.0;
    double _microMaterializeCpuSec = 0.0;
    double _microProxyBuildCpuSec = 0.0;
    double _microCollectCpuSec = 0.0;
    double _microEvaluateCpuSec = 0.0;
    double _microEvaluateWorkerCpuSec = 0.0;
    double _microWriteCpuSec = 0.0;
    double _microFinalizeCpuSec = 0.0;

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
        bool texturesDropped = false;  // 内节点代理连下限分辨率纹理都装不下，构建时去纹理
        unsigned texturesCappedPx = 0; // 内节点代理写出时纹理边长上限（0 = 不额外降分辨率）
        std::map<FeatureId, std::vector<FeatureId>> aggregateSources;
        std::vector<FeatureId> sourceScope;
        double measuredProxyErrorM = 0.0;
        double errorLowerM = 0.0;
        double errorUpperM = 0.0;
        std::string errorStatus;
        bool certified = false;
        bool boundLoose = false;
        double p95M = 0.0;
        std::string failure;   // proxy 生成失败原因
        // 细分仪表（随工作带回主线程累加）
        // 壁钟（Sec）与 CPU 秒（CpuSec）是两套量：瓦片之间 8 路并行时
        // Σ 壁钟 = 1× 工作量，Σ CPU 秒才等于进程 user 秒。对账用后者。
        double microHeavySec = 0.0;
        double microHeavyCpuSec = 0.0;
        double microMaterializeSec = 0.0;
        double microMaterializeCpuSec = 0.0;
        double microProxyBuildSec = 0.0;
        double microProxyBuildCpuSec = 0.0;
        double microProxySimplifySec = 0.0;
        double microProxyAggregateSec = 0.0;
        std::uint64_t microAggregateCandidates = 0;
        double microCollectSec = 0.0;
        double microCollectCpuSec = 0.0;
        double microEvaluateSec = 0.0;
        double microEvaluateCpuSec = 0.0;   // 调用线程 + worker 线程
        double microEvaluateWorkerCpuSec = 0.0;  // 只有 worker 线程（micro_total_cpu 加这一份）
        std::uint64_t microEvaluateSamples = 0;
        double microBvhBuildSec = 0.0;
        // 评估内部成本分解（A-55）：把 micro_evaluate 拆成四段，纯观测不参与判定
        double microEvalInitSec = 0.0;
        double microEvalPushSec = 0.0;
        double microEvalRefineSec = 0.0;
        double microEvalQuantileSec = 0.0;
        std::uint64_t microEvalRefineIterations = 0;
        std::uint64_t microEvalSourceTriangles = 0;  // 子树源三角形（重复覆盖口径）
        std::uint64_t microEvalProxyTriangles = 0;
        // 真实 distance() 调用次数与 BVH 工作量（本轮）：samples 是逻辑采样记录，
        // 这里才是"算过多少个点"。initThreadOverheadSec 是 initQuerySeconds 里
        // 可分离出来的建线程/join 墙钟。
        std::uint64_t microEvalDistanceCalls = 0;
        std::uint64_t microEvalInitDistanceCalls = 0;
        std::uint64_t microEvalRefineDistanceCalls = 0;
        std::uint64_t microEvalBvhNodeVisits = 0;
        std::uint64_t microEvalBvhTriangleTests = 0;
        double microEvalInitParallelSec = 0.0;
        double microEvalInitThreadCreateSec = 0.0;
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
    // textureMaxEdgePx：本瓦片单张纹理的最大边长（0 = 用命令行 -tw/-th 的全局值）。
    // 只给内节点代理传，叶瓦片保持原始分辨率。
    // foldTextureTransform：写出前把图集子矩形的 KHR_texture_transform 折进
    // TEXCOORD_0（A-51 方案 A，共用同一张贴图的 drawable 才能合并材质）。
    // 同样只给代理开：叶瓦片按手册 7.4 只允许无损重建，改写 UV 属有损重编码。
    WriteOutcome writeContent(osg::Node* node, const std::string& relUri, bool isInstanceTile,
                              unsigned textureMaxEdgePx = 0, bool foldTextureTransform = false);

    std::uint64_t _peakRss = 0;
};

} // namespace production
} // namespace osgGISPlugins
#endif
