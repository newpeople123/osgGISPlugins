#include "3dtiles/production/TileFeatureMapper.h"
#include <algorithm>
#include <set>
using namespace osgGISPlugins::production;

void TileFeatureMapper::addTile(const std::string& uri,
                                const std::vector<FeatureId>& featuresInOrder,
                                bool isProxy)
{
    TileFeatureRecord rec;
    rec.uri = uri;
    rec.isProxy = isProxy;
    rec.batchLength = static_cast<unsigned>(featuresInOrder.size());
    for (std::size_t i = 0; i < featuresInOrder.size(); ++i)
    {
        BatchEntry e;
        e.localBatchId = static_cast<std::uint32_t>(i);
        e.globalFeatureId = featuresInOrder[i];
        e.name = std::string();   // 由 manifestJson 用 SourceModel 补齐
        rec.entries.push_back(e);
    }
    _records.push_back(rec);
}

void TileFeatureMapper::addInstanceTile(const std::string& uri,
                                        const std::vector<FeatureId>& prototypeFeatures,
                                        const std::vector<std::uint32_t>& instanceIds)
{
    TileFeatureRecord rec;
    rec.uri = uri;
    rec.isProxy = false;
    rec.batchLength = static_cast<unsigned>(prototypeFeatures.size());
    for (std::size_t i = 0; i < prototypeFeatures.size(); ++i)
    {
        BatchEntry e;
        e.localBatchId = static_cast<std::uint32_t>(i);
        e.globalFeatureId = prototypeFeatures[i];
        rec.entries.push_back(e);
    }
    rec.entries.reserve(rec.entries.size());
    (void)instanceIds;
    _records.push_back(rec);
}

std::size_t TileFeatureMapper::sourceOccurrenceCount(const SourceModel& model) const
{
    // 与 manifestJson 共用这一个定义：落到未知 feature id 上的条目
    // （聚合代理身份）不进 manifest 的 features，也就不能算源出现次数。
    // 报表里另算一份会让 conversion-report 和 manifest 两个数对不上
    // （实测 longxiang：report 623 / manifest 571）。
    std::size_t n = 0;
    for (const TileFeatureRecord& r : _records)
    {
        for (const BatchEntry& e : r.entries)
        {
            if (model.objectOf(e.globalFeatureId)) { ++n; }
        }
    }
    return n;
}

std::vector<std::string> TileFeatureMapper::validate() const
{
    std::vector<std::string> errors;
    for (const TileFeatureRecord& r : _records)
    {
        if (r.batchLength != r.entries.size())
        {
            errors.push_back(r.uri + ": batchLength " + std::to_string(r.batchLength)
                             + " != entries " + std::to_string(r.entries.size()));
        }
        std::set<std::uint32_t> seen;
        for (const BatchEntry& e : r.entries)
        {
            if (e.localBatchId >= r.batchLength)
            {
                errors.push_back(r.uri + ": _BATCHID " + std::to_string(e.localBatchId)
                                 + " out of range [0," + std::to_string(r.batchLength) + ")");
            }
            if (!seen.insert(e.localBatchId).second)
            {
                errors.push_back(r.uri + ": duplicate localBatchId " + std::to_string(e.localBatchId));
            }
        }
    }
    return errors;
}

json TileFeatureMapper::manifestJson(const SourceModel& model) const
{
    json features = json::object();
    for (const SourceObject& o : model.objects)
    {
        json fj;
        fj["name"] = o.name;
        fj["nodePath"] = o.nodePath;
        fj["parentFeatureId"] = o.parentFeatureId == INVALID_FEATURE ? json(nullptr) : json(o.parentFeatureId);
        fj["instanced"] = o.instanced;
        json props = json::object();
        for (const SourceProperty& p : o.properties)
        {
            props[p.key] = {{"value", p.value}, {"type", p.typeName}};
        }
        fj["properties"] = props;
        fj["occurrences"] = json::array();
        features[std::to_string(o.globalFeatureId)] = fj;
    }

    // 出现次数只统计能落到源 feature 上的条目：聚合代理条目用的是代理身份，
    // 由 writeManifest 写进 aggregateProxies，不混进 features。
    // 报表与 manifest 共用 sourceOccurrenceCount，两个数不会各算一套
    // （实测 longxiang：report 623 / manifest 571）。
    for (const TileFeatureRecord& r : _records)
    {
        for (const BatchEntry& e : r.entries)
        {
            const std::string key = std::to_string(e.globalFeatureId);
            if (!features.contains(key)) { continue; }
            features[key]["occurrences"].push_back({
                {"tileUri", r.uri},
                {"localBatchId", e.localBatchId},
                {"isProxy", r.isProxy}
            });
        }
    }
    const std::size_t occ = sourceOccurrenceCount(model);

    json instances = json::array();
    for (const SourceInstance& inst : model.instances)
    {
        json ij;
        ij["instanceId"] = inst.instanceId;
        ij["globalFeatureId"] = inst.globalFeatureId;
        ij["prototypeFeatureId"] = inst.prototypeFeatureId;
        ij["name"] = inst.name;
        const double* m = inst.matrix.ptr();
        ij["transform"] = {m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                           m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]};
        // i3dm 特征表里的矩阵是 `inverse(原型世界) · 实例世界`（原型几何已按世界
        // 矩阵烘好）。不把这个记下来，产物自身无法判位姿对不对——只能拿绝对平移去
        // 比，而绝对平移恰好是"双变换"错误口径的特征。
        const SourceObject* proto = model.objectOf(inst.prototypeFeatureId);
        if (proto)
        {
            const double* p = proto->worldMatrix.ptr();
            ij["prototypeTransform"] = {p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                                        p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]};
        }
        instances.push_back(ij);
    }

    return json{
        {"schema", "osgGISPlugins/model23dtiles feature manifest v1.1"},
        {"sourceFeatureCount", model.objects.size()},
        {"occurrenceCount", occ},
        {"features", features},
        // 占位：内容由 ProductionPipeline::writeManifest 按 tileUri 填入
        {"aggregateProxies", json::object()},
        {"instances", instances}
    };
}
