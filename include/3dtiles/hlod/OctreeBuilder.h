#ifndef OSG_GIS_PLUGINS_OCTREE_BUILDER_H
#define OSG_GIS_PLUGINS_OCTREE_BUILDER_H
#include "3dtiles/hlod/TreeBuilder.h"
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	class OctreeBuilder :public TreeBuilder
	{
	public:
		META_NodeVisitor(osgGISPlugins, OctreeBuilder)

			OctreeBuilder() :TreeBuilder() {}

	private:
		osg::ref_ptr<B3DMTile> divide(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<Tile> parent = nullptr, const unsigned int x = 0, const unsigned int y = 0, const unsigned int z = 0, const unsigned int level = 0) override;
	};
}
#endif // !OSG_GIS_PLUGINS_OCTREE_BUILDER_H
