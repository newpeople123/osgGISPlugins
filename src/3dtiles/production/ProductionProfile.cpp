#include "3dtiles/production/ProductionProfile.h"
#include <stdexcept>
#include <cmath>
using namespace osgGISPlugins::production;

json ResourceBudget::toJson() const
{
    return json{
        {"maxTileContentBytes", maxTileContentBytes},
        {"targetTileContentBytes", targetTileContentBytes},
        {"maxTrianglesPerTile", maxTrianglesPerTile},
        {"targetTrianglesPerTile", targetTrianglesPerTile},
        {"maxPrimitivesPerTile", maxPrimitivesPerTile},
        {"targetPrimitivesPerTile", targetPrimitivesPerTile},
        {"maxMaterialsPerTile", maxMaterialsPerTile},
        {"targetMaterialsPerTile", targetMaterialsPerTile},
        {"maxAtlasDimension", maxAtlasDimension},
        {"targetAtlasDimension", targetAtlasDimension},
        {"maxTreeDepth", maxTreeDepth},
        {"maxInstancesPerTile", maxInstancesPerTile},
        {"maxVerticesPerTile", maxVerticesPerTile},
        {"maxIndicesPerTile", maxIndicesPerTile}
    };
}

json PartitionWeights::toJson() const
{
    return json{
        {"wLoad", wLoad}, {"wSAH", wSAH}, {"wOverlap", wOverlap},
        {"wBalance", wBalance}, {"wMaterial", wMaterial}, {"wSemantic", wSemantic},
        {"bins", bins}, {"minImprovement", minImprovement}
    };
}

json ErrorEvalConfig::toJson() const
{
    return json{
        {"maxSamplesPerNode", maxSamplesPerNode},
        {"initialSamplesPerTriangle", initialSamplesPerTriangle},
        {"toleranceMeters", toleranceMeters},
        {"certifyRatio", certifyRatio},
        {"maxRefineIterations", maxRefineIterations},
        {"evaluateLeaves", evaluateLeaves}
    };
}

json ProductionProfile::toJson() const
{
    return json{
        {"name", name}, {"version", version},
        {"budget", budget.toJson()},
        {"weights", weights.toJson()},
        {"error", error.toJson()},
        {"maximumScreenSpaceError", maximumScreenSpaceError},
        {"strictIdentity", strictIdentity},
        {"allowTriangleClip", allowTriangleClip},
        {"lockSeamBorder", lockSeamBorder},
        {"maxProxyAttempts", maxProxyAttempts}
    };
}

void ProductionProfile::validate() const
{
    auto require = [](bool ok, const char* what) {
        if (!ok) { throw std::invalid_argument(std::string("ProductionProfile invalid: ") + what); }
    };
    require(!name.empty(), "name empty");
    require(budget.maxTileContentBytes > 0, "maxTileContentBytes must be > 0");
    require(budget.targetTileContentBytes <= budget.maxTileContentBytes,
            "target content bytes must be <= hard limit");
    require(budget.maxTrianglesPerTile > 0, "maxTrianglesPerTile must be > 0");
    require(budget.targetTrianglesPerTile <= budget.maxTrianglesPerTile,
            "target triangles must be <= hard limit");
    require(budget.maxPrimitivesPerTile > 0, "maxPrimitivesPerTile must be > 0");
    require(budget.maxMaterialsPerTile > 0, "maxMaterialsPerTile must be > 0");
    require(budget.maxAtlasDimension >= 256, "maxAtlasDimension too small");
    require(budget.targetAtlasDimension <= budget.maxAtlasDimension, "atlas target > hard limit");
    require(budget.maxTreeDepth >= 1 && budget.maxTreeDepth <= 48,
            "maxTreeDepth must be within [1,48]");
    require(budget.maxInstancesPerTile > 0, "maxInstancesPerTile must be > 0");
    require(error.maxSamplesPerNode > 0, "maxSamplesPerNode must be > 0");
    require(error.toleranceMeters > 0.0, "toleranceMeters must be > 0");
    require(std::isfinite(maximumScreenSpaceError) && maximumScreenSpaceError > 0.0,
            "maximumScreenSpaceError must be finite and > 0");
    require(weights.bins >= 2, "bins must be >= 2");
}

json ConversionReport::toJson() const
{
    json js = json::array();
    for (const auto& s : stages) { js.push_back(s.toJson()); }
    return json{
        {"inputFile", inputFile},
        {"outputDir", outputDir},
        {"profile", profile.toJson()},
        {"stages", js},
        {"totalSeconds", totalSeconds},
        {"peakRssBytes", peakRssBytes},
        {"inputBytes", inputBytes},
        {"outputBytes", outputBytes},
        {"tileCount", tileCount},
        {"contentTileCount", contentTileCount},
        {"maxDepth", maxDepth},
        {"errorMaxMeters", errorMax},
        {"errorMeanMeters", errorMean},
        {"errorP95Meters", errorP95},
        {"nodesCertified", nodesCertified},
        {"nodesBoundLoose", nodesBoundLoose},
        {"maxContentBytes", maxContentBytes},
        {"maxTrianglesInTile", maxTrianglesInTile},
        {"aggregateProxyCount", aggregateProxyCount},
        {"instanceTileCount", instanceTileCount},
        {"instanceCount", instanceCount},
        {"sourceFeatureCount", sourceFeatureCount},
        {"manifestOccurrences", manifestOccurrences},
        {"featuresMissingFromLeaves", featuresMissingFromLeaves},
        {"proxyDroppedFeatureCount", proxyDroppedFeatureCount},
        {"proxySkippedCount", proxySkippedCount},
        {"warnings", warnings},
        {"errors", errors}
    };
}
