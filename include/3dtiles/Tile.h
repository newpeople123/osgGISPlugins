#ifndef OSG_GIS_PLUGINS_TILE_H
#define OSG_GIS_PLUGINS_TILE_H
#ifdef _WIN32
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "\\"
#else
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "/"
#endif // !OSG_GIS_PLUGINS_PATH_SPLIT_STRING
#include <osg/Node>
#include "3dtiles/BoundingVolume.h"
#include "utils/GltfOptimizer.h"
#include <osgDB/WriteFile>
// 控制导出3dtiles时是否为单线程(启用该宏则为单线程)
//#define OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	constexpr double PixelSize = 5.0;
	constexpr double CesiumCanvasClientWidth = 1920;
	constexpr double CesiumCanvasClientHeight = 1080;
	constexpr double CesiumFrustumAspectRatio = CesiumCanvasClientWidth / CesiumCanvasClientHeight;
	const double CesiumFrustumFov = osg::PI / 3;
	const double CesiumFrustumFovy = CesiumFrustumAspectRatio <= 1 ? CesiumFrustumFov : atan(tan(CesiumFrustumFov * 0.5) / CesiumFrustumAspectRatio) * 2.0;
	constexpr double CesiumFrustumNear = 0.1;
	constexpr double CesiumFrustumFar = 10000000000.0;
	constexpr double CesiumCanvasViewportWidth = CesiumCanvasClientWidth;
	constexpr double CesiumCanvasViewportHeight = CesiumCanvasClientHeight;
	const double CesiumSSEDenominator = 2.0 * tan(0.5 * CesiumFrustumFovy);
	constexpr double CesiumMaxScreenSpaceError = 16.0;
	const double CesiumDistanceOperator = tan(osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight * PixelSize / 2);
	const double CesiumGeometricErrorOperator = CesiumSSEDenominator * CesiumMaxScreenSpaceError / (CesiumCanvasClientHeight * tan(osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight * PixelSize / 2));


	enum class Refinement {
		REPLACE,
		ADD
	};

	class Tile :public osg::Object
	{
	public:
		std::string type = "";
		osg::ref_ptr<Tile> parent;
		osg::ref_ptr<osg::Node> node;

		BoundingVolume boundingVolume;
		double geometricError = 0.0;
		Refinement refine = Refinement::REPLACE;
		string contentUri;

		vector<osg::ref_ptr<Tile>> children;
		vector<double> transform;

		unsigned int level = 0;
		unsigned int x = 0;
		unsigned int y = 0;
		unsigned int z = 0;
		int axis = -1;

		Tile() = default;

		Tile(osg::ref_ptr<Tile> parent,const std::string& type)
			: parent(parent), type(type){}

		Tile(const Tile& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: osg::Object(other, copyop),
			parent(other.parent),
			node(other.node),
			boundingVolume(other.boundingVolume, copyop),
			geometricError(other.geometricError),
			refine(other.refine),
			contentUri(other.contentUri),
			children(other.children),
			transform(other.transform),
			type(other.type){}

		Tile(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent, const std::string& type)
			: parent(parent), node(node), type(type) {}

		virtual const char* libraryName() const { return "osgGISPlugins"; }

		virtual const char* className() const { return "Tile"; }

		void fromJson(const json& j);

		json toJson() const;

		int getMaxLevel() const;

		virtual void setContentUri();

		void computeBoundingVolumeBox();

		virtual void computeGeometricError() = 0;

		bool descendantNodeIsEmpty() const;

		osg::ref_ptr<osg::Group> getAllDescendantNodes() const;

		bool valid() const;

		virtual void write(const string& path, const float simplifyRatio, const GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options) = 0;
	};
}
#endif // !OSG_GIS_PLUGINS_TILE_H
