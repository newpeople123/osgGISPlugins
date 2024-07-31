#ifndef OSG_GIS_PLUGINS_B3DM_BATCHID_H
#define OSG_GIS_PLUGINS_B3DM_BATCHID_H 1
#include <osg/NodeVisitor>
#include <set>
#include <unordered_map>
#include <nlohmann/json.hpp>
using namespace nlohmann;

class BatchIdVisitor : public osg::NodeVisitor {
public:
    BatchIdVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), _currentBatchId(0), _currentParentBatchId(0) {}

    void apply(osg::Geode& geode) override;
    void apply(osg::Group& group) override;
    void apply(osg::Drawable& drawable) override;

    const unsigned int getMaxBatchId() const { return _currentBatchId; }

    const std::map<std::vector<std::string>, std::vector<unsigned int>> getAttributeNameBatchIdsMap() const {
        return _attributeNameBatchIdsMap;
    }

    const std::map<unsigned int, std::map<std::string, std::string>> getBatchIdAttributesMap() const {
        return _batchIdAttributesMap;
    }

    const std::map<unsigned int, unsigned int> getBatchParentIdMap() const {
        return _batchParentIdMap;
    }

private:
    unsigned int _currentParentBatchId;
    std::map<unsigned int, unsigned int> _nodeHierarchyMap;
    unsigned int _currentBatchId;
    typedef std::map<std::string, std::string> Attributes;
    std::map<unsigned int, Attributes> _batchIdAttributesMap;
    std::map<unsigned int, unsigned int> _batchParentIdMap;
    typedef std::vector<std::string> StringVector;

    bool areAttributeNamesEqual(const Attributes& attributes, const StringVector& attributeNames);
    StringVector convertAttributesToVector(const Attributes& attributes);

    std::map<std::vector<std::string>, std::vector<unsigned int>> _attributeNameBatchIdsMap;
};
#endif // !OSG_GIS_PLUGINS_B3DM_BATCHID_H
