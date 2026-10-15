#ifndef OSG_GIS_PLUGINS_PRODUCTION_TILE_FEATURE_MAPPER_H
#define OSG_GIS_PLUGINS_PRODUCTION_TILE_FEATURE_MAPPER_H
// globalFeatureId -> tile 本地 _BATCHID 的映射与数据集级 manifest（手册 5.4 / 8.1）
//
// globalFeatureId 是跨 tile、跨 LOD 的稳定身份；_BATCHID 只在单个 b3dm 内连续。
// 两者不能混用。manifest 记录：
//   * globalFeatureId -> 源节点 / 父节点 / 原始属性
//   * 该 feature 出现在哪些 tile URI 的哪个 localBatchId
//   * 聚合代理的 proxyFeatureId 代表的源 feature 集合
#include "3dtiles/production/SourceModel.h"
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <vector>

namespace osgGISPlugins {
namespace production {

using json = nlohmann::json;

struct BatchEntry {
    std::uint32_t localBatchId = 0;
    FeatureId globalFeatureId = 0;
    std::string name;
};

struct TileFeatureRecord {
    std::string uri;                 // 相对 tileset.json 的内容 URI
    unsigned batchLength = 0;        // 该 tile 的 BATCH_LENGTH
    std::vector<BatchEntry> entries; // 按 localBatchId 顺序
    bool isProxy = false;            // true 表示该 tile 内容是代理
};

class TileFeatureMapper {
public:
    // 按 node 里 Geode 的顺序登记（必须与 BatchIdVisitor 的编号顺序一致）
    void addTile(const std::string& uri, const std::vector<FeatureId>& featuresInOrder, bool isProxy);

    // 聚合代理：每个 cluster 一个 proxyFeatureId
    void addAggregateTile(const std::string& uri,
                          const std::vector<std::pair<FeatureId, std::vector<FeatureId>>>& clusters);

    void addInstanceTile(const std::string& uri,
                         const std::vector<FeatureId>& prototypeFeatures,
                         const std::vector<std::uint32_t>& instanceIds);

    // 数据集级 manifest
    json manifestJson(const SourceModel& model) const;

    // 自检：_BATCHID 落在 [0, batchLength)、batchLength == entries.size()
    std::vector<std::string> validate() const;

    const std::vector<TileFeatureRecord>& records() const { return _records; }
    std::size_t occurrenceCount() const;

private:
    std::vector<TileFeatureRecord> _records;
    std::map<FeatureId, std::vector<FeatureId>> _aggregateSources;
};

} // namespace production
} // namespace osgGISPlugins
#endif
