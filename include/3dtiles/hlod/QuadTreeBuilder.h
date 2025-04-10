#ifndef OSG_GIS_PLUGINS_QUADTREE_BUILDER_H
#define OSG_GIS_PLUGINS_QUADTREE_BUILDER_H
#include "3dtiles/hlod/TreeBuilder.h"
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	class QuadTreeBuilder :public TreeBuilder
	{
	public:
		META_NodeVisitor(osgGISPlugins, QuadTreeBuilder)

		QuadTreeBuilder() {}

		QuadTreeBuilder(const BuilderConfig config) :TreeBuilder(config) {}

	private:
		static int chooseSplitAxis(const osg::BoundingBox& bounds);

		static osg::BoundingBox computeChildBounds(const osg::BoundingBox& bounds, int axis, int a, int b);

		osg::ref_ptr<B3DMTile> divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent = nullptr, int x = 0, int y = 0, int z = 0, int level = 0) override;

		void divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile) override;
	};
}
#endif // !OSG_GIS_PLUGINS_QUADTREE_BUILDER_H
