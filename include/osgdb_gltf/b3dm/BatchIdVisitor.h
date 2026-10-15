#ifndef OSG_GIS_PLUGINS_B3DM_BATCHID_H
#define OSG_GIS_PLUGINS_B3DM_BATCHID_H 1
#include <osg/NodeVisitor>
#include <set>
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
        // 记录本 visitor 自己写进去的 _BATCHID 数组，避免重复运行时把
        // 源属性 0 误判成"已有数据"而搬来搬去。
        std::set<const osg::Array*> _written;
    };
}
#endif // !OSG_GIS_PLUGINS_B3DM_BATCHID_H
