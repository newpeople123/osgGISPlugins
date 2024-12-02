#ifndef OSG_GIS_PLUGINS_B3DM_TILE_H
#define OSG_GIS_PLUGINS_B3DM_TILE_H
#include "3dtiles/Tile.h"
using namespace std;
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	class B3DMTile :public Tile {
	private:
		void buildBaseHlod();

		void rebuildHlod();
	public:
		osg::BoundingBox bb, maxClusterBb;
		double diagonalLength = 0.0, maxClusterDiagonalLength = 0.0;
		double volume = 0.0, maxClusterVolume = 0.0;

		B3DMTile() {
			type = "b3dm";
		}

		B3DMTile(osg::ref_ptr<osg::Node> node, osg::ref_ptr<B3DMTile> parent)
			: Tile(node, parent,"b3dm") {}

		B3DMTile(osg::ref_ptr<B3DMTile> parent)
			: Tile(parent, "b3dm") {}

		B3DMTile(const B3DMTile& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: Tile(other, copyop) {}

		virtual osg::Object* cloneType() const { return new B3DMTile(); }

		virtual osg::Object* clone(const osg::CopyOp& copyop) const { return new B3DMTile(*this, copyop); }

		virtual const char* libraryName() const { return "osgGISPlugins"; }

		virtual const char* className() const { return "B3DMTile"; }

		void buildHlod();

		void write(const string& path, const float simplifyRatio, const GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options) override;

		static double computeRadius(const osg::BoundingBox& bbox, int axis);

		void computeGeometricError() override;

		void computeBoundingBox();
	};
}
#endif // !OSG_GIS_PLUGINS_B3DM_TILE_H
