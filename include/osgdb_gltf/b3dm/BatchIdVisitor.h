#ifndef OSG_GIS_PLUGINS_B3DM_BATCHID_H
#define OSG_GIS_PLUGINS_B3DM_BATCHID_H 1
#include <osg/NodeVisitor>
namespace osgGISPlugins {
    class BatchIdVisitor : public osg::NodeVisitor {
    public:
        META_NodeVisitor(osgGISPlugins, BatchIdVisitor)

        BatchIdVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), _currentBatchId(0) {}

        void apply(osg::Drawable& drawable) override;
        void apply(osg::Geode& geode) override;
        const unsigned int getBatchLength() const { return _currentBatchId; }

    private:
        unsigned int _currentBatchId;
        bool _bAdd = false;
    };
}
#endif // !OSG_GIS_PLUGINS_B3DM_BATCHID_H
