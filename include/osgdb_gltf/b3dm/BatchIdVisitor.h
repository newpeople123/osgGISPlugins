#ifndef OSG_GIS_PLUGINS_B3DM_BATCHID_H
#define OSG_GIS_PLUGINS_B3DM_BATCHID_H 1
#include <osg/NodeVisitor>
#include <stack>
#include <unordered_map>
#include <nlohmann/json.hpp>
using namespace nlohmann;
class BatchIdVisitor : public osg::NodeVisitor {
public:
    BatchIdVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), _batchId(0),_currentParentBatchId(-1){}

    void apply(osg::Node& node) override;
    void apply(osg::Drawable& drawable) override;

    const int getBatchId()const { return _batchId; };

private:
    int _currentParentBatchId;
    std::map<int, int> _nodeHierarchy;
    int _batchId;
};
#endif // !OSG_GIS_PLUGINS_B3DM_BATCHID_H
