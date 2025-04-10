#ifndef OSG_GIS_PLUGINS_I3DM_TILE_H
#define OSG_GIS_PLUGINS_I3DM_TILE_H
#include "3dtiles/Tile.h"
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	class I3DMTile :public Tile
	{
	public:
		I3DMTile() {
			type = "i3dm";
		}

		I3DMTile(const osg::ref_ptr<osg::Node>& node, const osg::ref_ptr<Tile>& parent)
			: Tile(node, parent, "i3dm") {}

		I3DMTile(const osg::ref_ptr<Tile>& parent)
			: Tile(parent, "i3dm") {}

		I3DMTile(const I3DMTile& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: Tile(other, copyop) {}

		osg::Object* cloneType() const override { return new I3DMTile(); }

		osg::Object* clone(const osg::CopyOp& copyop) const override { return new I3DMTile(*this, copyop); }

		const char* libraryName() const override { return "osgGISPlugins"; }

		const char* className() const override { return "I3DMTile"; }

		void optimizeNode(osg::ref_ptr<osg::Node>& nodeCopy, const GltfOptimizer::GltfTextureOptimizationOptions& options) override;

		string getOutputPath() const override;

		string getFullPath() const override;

		string getTextureCachePath(string textureCachePath) const override;

		void setContentUri() override;

		I3DMTile* createTileOfSameType(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent) override;

		void computeDiagonalLengthAndVolume() override;
	};
}
#endif // !OSG_GIS_PLUGINS_I3DM_TILE_H
