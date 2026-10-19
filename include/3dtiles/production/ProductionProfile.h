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
    bool lockSeamBorder = true;       // 简化时锁定边界
    std::size_t maxProxyAttempts = 8; // 代理简化尝试次数上限

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

struct ConversionReport {
    std::string inputFile;
    std::string outputDir;
    ProductionProfile profile;

    std::vector<StageTiming> stages;
    double totalSeconds = 0.0;
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

    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    void addStage(const std::string& n, double s) { stages.push_back({n, s}); }
    json toJson() const;
};

} // namespace production
} // namespace osgGISPlugins
#endif
