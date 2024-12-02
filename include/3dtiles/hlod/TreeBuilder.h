#ifndef OSG_GIS_PLUGINS_TREE_BUILDER_H
#define OSG_GIS_PLUGINS_TREE_BUILDER_H
#include <osg/NodeVisitor>
#include <osg/PrimitiveSet>
#include <osg/Geometry>
#include "3dtiles/B3DMTile.h"
#include "3dtiles/I3DMTile.h"
#include "utils/Utils.h"
namespace osgGISPlugins
{

    class TreeBuilder :public osg::NodeVisitor
	{
    public:
        META_NodeVisitor(osgGISPlugins, TreeBuilder)

        TreeBuilder() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {};

        virtual osg::ref_ptr<B3DMTile> build();

        std::map<osg::Geode*, osg::MatrixList> _geodeMatrixMap;
        std::map<osg::Geode*, std::vector<osg::ref_ptr<osg::UserDataContainer>>> _geodeUserDataMap;


    protected:
        osg::ref_ptr<osg::Group> _groupsToDivideList = new osg::Group;

        unsigned int _maxLevel = -1;

        size_t _maxTriangleCount = 6e4;

        unsigned int _maxTextureCount = 35;

        osg::Matrixd _currentMatrix;

        virtual void apply(osg::Group& group) override;

        virtual void apply(osg::Geode& geode) override;

        virtual osg::ref_ptr<B3DMTile> divide(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<Tile> parent = nullptr, const unsigned int x = 0, const unsigned int y = 0, const unsigned int z = 0, const unsigned int level = 0);

        virtual osg::ref_ptr<B3DMTile> generateB3DMTile();

        virtual osg::ref_ptr<I3DMTile> generateI3DMTile();

	};
}

#endif // !OSG_GIS_PLUGINS_TREE_BUILDER_H
