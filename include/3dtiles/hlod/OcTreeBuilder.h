#ifndef OSG_GIS_PLUGINS_OCTREE_BUILDER_H
#define OSG_GIS_PLUGINS_OCTREE_BUILDER_H
#include "3dtiles/hlod/TreeBuilder.h"
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	class OcTreeBuilder :public TreeBuilder
	{
	public:
		META_NodeVisitor(osgGISPlugins, OcTreeBuilder)

		OcTreeBuilder() :TreeBuilder() {}

		OcTreeBuilder(BuilderConfig config) :TreeBuilder(config) {}

	private:
		osg::ref_ptr<B3DMTile> divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent = nullptr, const int x = 0, const int y = 0, const int z = 0, const int level = 0) override;

		void divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile) override;
	};
}
#endif // !OSG_GIS_PLUGINS_OCTREE_BUILDER_H
