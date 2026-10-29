#ifndef OSG_GIS_PLUGINS_PRODUCTION_PROFILE_H
#define OSG_GIS_PLUGINS_PRODUCTION_PROFILE_H
// 生产级改造：集中式资源 profile / 代价权重 / 误差评估参数 / 转换报告。
// 手册第 3 节要求把这些数字从源码常量里拿出来集中配置并做范围校验，
// 且「硬上限不得在转换中自动放宽」。
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace osgGISPlugins {
namespace production {

using json = nlohmann::json;

// 内节点代理纹理的最低可接受边长：写出阶段的降采样阶梯到此为止，构建期
// "降到下限仍装不下才去纹理"的判据用同一个值，两边不能各说各话。
constexpr unsigned proxyTextureFloorEdgePx = 64;

// ---------------------------------------------------------------------------
// 硬上限 / 软目标
// ---------------------------------------------------------------------------
struct ResourceBudget {
    // 编码后 tile 内容字节
    std::uint64_t maxTileContentBytes = 8u * 1024u * 1024u;      // 硬上限 8 MiB
    std::uint64_t targetTileContentBytes = 4u * 1024u * 1024u;   // 软目标 4 MiB
    // 每 tile 三角形
    unsigned maxTrianglesPerTile = 120000;
    unsigned targetTrianglesPerTile = 80000;
    // glTF primitive / drawcall
    unsigned maxPrimitivesPerTile = 24;
    unsigned targetPrimitivesPerTile = 12;
    // 材质
    unsigned maxMaterialsPerTile = 24;
    unsigned targetMaterialsPerTile = 12;
    // 图集
    unsigned maxAtlasDimension = 4096;
    unsigned targetAtlasDimension = 2048;
    // 内节点代理（远景 LOD）纹理最大边长：按预算降分辨率，而不是整张删纹理。
    // 装不下 maxTileContentBytes 时逐级折半，直到 proxyTextureFloorEdgePx
    // 仍不达标才去纹理。
    unsigned proxyTextureMaxEdgePx = 512;
    // 空间树
    unsigned maxTreeDepth = 48;
    // i3dm
    unsigned maxInstancesPerTile = 512;
    // 顶点 / 索引数量（防止整数溢出与 accessor 超限）
    std::uint64_t maxVerticesPerTile = 4000000;
    std::uint64_t maxIndicesPerTile = 4000000;

    json toJson() const;
};

// ---------------------------------------------------------------------------
// 空间切分代价权重（版本化，写入报告）
// ---------------------------------------------------------------------------
struct PartitionWeights {
    float wLoad = 1.0f;
    float wSAH = 0.5f;
    float wOverlap = 0.5f;
    float wBalance = 0.25f;
    float wMaterial = 0.25f;
    float wSemantic = 0.75f;
    unsigned bins = 16;
    // 停止分裂所需的最小代价改善比例
    float minImprovement = 0.02f;

    json toJson() const;
};

// ---------------------------------------------------------------------------
// 几何误差评估参数
// ---------------------------------------------------------------------------
struct ErrorEvalConfig {
    std::size_t maxSamplesPerNode = 400000;
    std::size_t initialSamplesPerTriangle = 7;   // 3 顶点 + 3 边中点 + 重心
    double toleranceMeters = 1.0e-3;             // 上下界收敛判据
    double certifyRatio = 0.02;                  // 区间/上界 < 2% 视为收敛
    unsigned maxRefineIterations = 200000;
    bool evaluateLeaves = false;                 // 叶默认保真，不评估

    json toJson() const;
};

// ---------------------------------------------------------------------------
// 完整 profile
// ---------------------------------------------------------------------------
struct ProductionProfile {
    std::string name = "desktop-web-high-quality";
    std::string version = "1.0.0";
    ResourceBudget budget;
    PartitionWeights weights;
    ErrorEvalConfig error;

    // 视觉基线
    double maximumScreenSpaceError = 4.0;

    // 业务策略
    bool strictIdentity = false;      // true: 禁止 aggregate proxy，无法达标即失败
    bool allowTriangleClip = false;   // true: 允许沿平面切开三角形（默认不切）
    // 内节点 far-LOD 代理超 primitive/材质预算时，是否允许降级为“无内容内节点”。
    // 默认 true（既有发布策略）；-noProxyDegrade 置 false 时改为失败关闭，
    // 用于需要显式拒绝 LOD 空洞的资产。叶/实例瓦片始终失败关闭，不受此开关影响。
    bool allowInnerProxyDegrade = true;
    bool lockSeamBorder = true;       // 简化时锁定边界
    // 代理简化加压阶梯的重试次数上限，由 ProxyMeshBuilder::simplifyNode 读取
    // （0 = 不进入阶梯，代理保持原始面数）。默认 8 是本次改动前的硬编码值，
    // 因此默认配置下产物逐位不变。当前没有命令行开关，只能改代码或 profile 构造处。
    std::size_t maxProxyAttempts = 8;

    // 内部：写 tileset 用
    std::string generator = "osgGISPlugins/production";

    json toJson() const;
    // 范围校验，非法值直接抛 std::invalid_argument（不允许静默夹取硬上限）
    void validate() const;
};

// ---------------------------------------------------------------------------
// 转换报告（手册 13.3）
// ---------------------------------------------------------------------------
struct StageTiming {
    std::string name;
    double seconds = 0.0;
    json toJson() const { return json{{"name", name}, {"seconds", seconds}}; }
};

// 计数量（AUDIT A-47）：以前塞在 stages 里，秒数与个数共用一个 "seconds" 字段，
// 汇总脚本按秒求和就会把 4 亿个候选当成 4 亿秒加进去。计数单独走 counters。
struct StageCount {
    std::string name;
    std::uint64_t value = 0;
    json toJson() const { return json{{"name", name}, {"value", value}}; }
};

struct ConversionReport {
    std::string inputFile;
    std::string outputDir;
    ProductionProfile profile;

    std::vector<StageTiming> stages;
    std::vector<StageCount> counters;
    double totalSeconds = 0.0;
    // 进程自己的读数（AUDIT A-47）：totalSeconds 只覆盖 ProductionPipeline::run()，
    // 而 main.cpp 里的 osgDB::readNodeFile 完全不在这段里——335 MB 档"32 % CPU 秒
    // 无归属"的缺口就藏在这里。没有这两行，归属比例永远算不平，外部监视器还会
    // 带进 1 s 级的采样误差。
    double processCpuSeconds = 0.0;    // 自进程启动累计 user+kernel CPU 秒
    double processUserSeconds = 0.0;   // 其中 user 态：壁钟仪表能对上的是这一项
    double processKernelSeconds = 0.0; // 其中 kernel 态：缺页/文件 IO，壁钟仪表看不见
    double processWallSeconds = 0.0;   // 自进程启动累计墙钟秒
    std::uint64_t peakRssBytes = 0;

    std::uint64_t inputBytes = 0;
    std::uint64_t outputBytes = 0;
    unsigned tileCount = 0;
    unsigned contentTileCount = 0;
    unsigned maxDepth = 0;

    // 误差统计
    double errorMax = 0.0;
    double errorMean = 0.0;
    double errorP95 = 0.0;
    unsigned nodesCertified = 0;
    unsigned nodesBoundLoose = 0;

    // 资源
    std::uint64_t maxContentBytes = 0;
    unsigned maxTrianglesInTile = 0;
    unsigned aggregateProxyCount = 0;
    unsigned instanceTileCount = 0;
    unsigned instanceCount = 0;

    // feature
    unsigned sourceFeatureCount = 0;
    unsigned manifestOccurrences = 0;
    // 手册 9.5：组件丢失必须单独报告，不能藏进平均误差
    unsigned featuresMissingFromLeaves = 0;   // 叶瓦片覆盖缺口（真丢数据）
    unsigned proxyDroppedFeatureCount = 0;    // 聚合代理被抽稀掉的源 feature 数
    unsigned proxySkippedCount = 0;           // 建不出达标远景代理、退化为"仅子树覆盖"的内节点数
    // 写出时纹理被压到 proxyTextureMaxEdgePx 的内节点代理瓦片数（保住纹理、只降
    // 分辨率）。连下限都装不下、退化成无纹理的那批另有逐瓦片 warning。
    unsigned proxyTextureCappedCount = 0;

    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    void addStage(const std::string& n, double s) { stages.push_back({n, s}); }
    void addCounter(const std::string& n, std::uint64_t v) { counters.push_back({n, v}); }
    json toJson() const;
};

} // namespace production
} // namespace osgGISPlugins
#endif
