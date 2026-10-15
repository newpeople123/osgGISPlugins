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

void TileFeatureMapper::addAggregateTile(const std::string& uri,
    const std::vector<std::pair<FeatureId, std::vector<FeatureId>>>& clusters)
{
    TileFeatureRecord rec;
    rec.uri = uri;
    rec.isProxy = true;
    rec.batchLength = static_cast<unsigned>(clusters.size());
    for (std::size_t i = 0; i < clusters.size(); ++i)
    {
        BatchEntry e;
        e.localBatchId = static_cast<std::uint32_t>(i);
        e.globalFeatureId = clusters[i].first;
        rec.entries.push_back(e);
        _aggregateSources[clusters[i].first] = clusters[i].second;
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

std::size_t TileFeatureMapper::occurrenceCount() const
{
    std::size_t n = 0;
    for (const TileFeatureRecord& r : _records) { n += r.entries.size(); }
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

    std::size_t occ = 0;
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
            ++occ;
        }
    }

    json agg = json::object();
    for (auto& kv : _aggregateSources)
    {
        agg[std::to_string(kv.first)] = kv.second;
    }

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
        instances.push_back(ij);
    }

    return json{
        {"schema", "osgGISPlugins/model23dtiles feature manifest v1"},
        {"sourceFeatureCount", model.objects.size()},
        {"occurrenceCount", occ},
        {"features", features},
        {"aggregateProxies", agg},
        {"instances", instances}
    };
}
