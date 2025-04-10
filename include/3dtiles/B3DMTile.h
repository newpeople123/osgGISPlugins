#ifndef OSG_GIS_PLUGINS_B3DM_TILE_H
#define OSG_GIS_PLUGINS_B3DM_TILE_H
#include "3dtiles/Tile.h"
using namespace std;
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	class B3DMTile :public Tile {
	public:

		B3DMTile() {
			type = "b3dm";
		}

		B3DMTile(const osg::ref_ptr<osg::Node>& node, const osg::ref_ptr<B3DMTile>& parent)
			: Tile(node, parent,"b3dm") {}

		B3DMTile(const osg::ref_ptr<B3DMTile>& parent)
			: Tile(parent, "b3dm") {}

		B3DMTile(const B3DMTile& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: Tile(other, copyop) {}

		osg::Object* cloneType() const override { return new B3DMTile(); }

		osg::Object* clone(const osg::CopyOp& copyop) const override { return new B3DMTile(*this, copyop); }

		const char* libraryName() const override { return "osgGISPlugins"; }

		const char* className() const override { return "B3DMTile"; }

		void optimizeNode(osg::ref_ptr<osg::Node>& nodeCopy, const GltfOptimizer::GltfTextureOptimizationOptions& options) override;

		string getOutputPath() const override;

		string getFullPath() const override;

		string getTextureCachePath(string textureCachePath) const override;

		void setContentUri() override;

		B3DMTile* createTileOfSameType(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent) override;
	};
}
#endif // !OSG_GIS_PLUGINS_B3DM_TILE_H
