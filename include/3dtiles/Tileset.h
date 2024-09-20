#ifndef OSG_GIS_PLUGINS_TILESET_H
#define OSG_GIS_PLUGINS_TILESET_H
#include "utils/GltfOptimizer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <osg/Node>
#include <memory>
#include <osgDB/Options>
#include <osgDB/FileUtils>
using json = nlohmann::json;
using namespace std;
namespace osgGISPlugins
{
	constexpr double InitialPixelSize = 5.0;
	constexpr double DetailPixelSize = InitialPixelSize * 5;
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
	const double CesiumDistanceOperator = tan(osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight * InitialPixelSize / 2);
	const double CesiumGeometricErrorOperator = CesiumSSEDenominator * CesiumMaxScreenSpaceError / (CesiumCanvasClientHeight * tan(osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight * InitialPixelSize / 2));

	enum class Refinement {
		REPLACE,
		ADD
	};

	class BoundingVolume :public osg::Object {
	public:
		vector<double> region;
		vector<double> box;
		vector<double> sphere;
		BoundingVolume() = default;

		BoundingVolume(const BoundingVolume& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: osg::Object(other, copyop),
			region(other.region),
			box(other.box),
			sphere(other.sphere) {}

		void fromJson(const json& j);

		json toJson() const;

		virtual osg::Object* cloneType() const { return new BoundingVolume(); }

		virtual osg::Object* clone(const osg::CopyOp& copyop) const { return new BoundingVolume(*this, copyop); }

		virtual const char* libraryName() const { return "osgGISPlugins"; }

		virtual const char* className() const { return "BoundingVolume"; }

		void computeBox(osg::ref_ptr<osg::Node> node);

		void computeSphere(osg::ref_ptr<osg::Node> node);

		void computeRegion(osg::ref_ptr<osg::Node> node);
	};

	class Tile :public osg::Object {
	private:
		void buildBaseHlodAndComputeGeometricError();

		void rebuildHlodAndComputeGeometricErrorByRefinement();

		bool descendantNodeIsEmpty() const;

		void setContentUri();

		void computeBoundingVolumeBox();

		osg::ref_ptr<osg::Group> getAllDescendantNodes() const;
	public:
		int axis = -1;
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

		Tile() = default;


		Tile(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent)
			: parent(parent), node(node) {}

		Tile(osg::ref_ptr<Tile> parent)
			: parent(parent) {}

		void fromJson(const json& j);

		json toJson() const;

		Tile(const Tile& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: osg::Object(other, copyop),
			parent(other.parent),
			node(other.node),
			boundingVolume(other.boundingVolume, copyop),
			geometricError(other.geometricError),
			refine(other.refine),
			contentUri(other.contentUri),
			children(other.children),
			transform(other.transform) {}

		int getMaxLevel() const;

		virtual osg::Object* cloneType() const { return new Tile(); }

		virtual osg::Object* clone(const osg::CopyOp& copyop) const { return new Tile(*this, copyop); }

		virtual const char* libraryName() const { return "osgGISPlugins"; }

		virtual const char* className() const { return "Tile"; }

		void buildHlod();

		bool valid() const;

		void write(const string& path, const float simplifyRatio, const GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options);

		static double computeRadius(const osg::BoundingBox& bbox, int axis);
	};

	class Tileset :public osg::Object {
	private:
		void computeTransform(const double lng, const double lat, const double height);

		void computeGeometricError(osg::ref_ptr<osg::Node> originNode);
	public:
		struct Config {
			//存储3dtiles的文件夹
			string path = "";
			//纬度，角度制
			double latitude = 30.0;
			//经度，角度制
			double longitude = 116.0;
			//高度，单位米
			double height = 100.0;
			//简化比例，[0-1]
			float simplifyRatio = 0.5f;
			//纹理优化设置
			GltfOptimizer::GltfTextureOptimizationOptions gltfTextureOptions;
			//b3dm、i3dm瓦片导出设置
			osg::ref_ptr<osgDB::Options> options = new osgDB::Options;

			void validate() {
				if (this->gltfTextureOptions.cachePath.empty())
					this->gltfTextureOptions.cachePath = this->path + "\\textures";
				osgDB::makeDirectory(this->gltfTextureOptions.cachePath);

				osg::clampTo(this->simplifyRatio, 0.f, 1.f);

				osg::clampTo(this->latitude, -90.0, 90.0);
				osg::clampTo(this->longitude, -180.0, 180.0);
			}
		};
		string assetVersion = "1.0";
		string tilesetVersion;
		double geometricError;
		osg::ref_ptr<Tile> root;

		void fromJson(const json& j);

		static Tileset fromFile(const string& filePath);

		json toJson() const;

		bool toFile(Config config, osg::ref_ptr<osg::Node> originNode);

		Tileset() : root(new Tile), geometricError(0.0) {}

		Tileset(osg::ref_ptr<Tile> rootTile) : root(rootTile), geometricError(0.0) {}

		Tileset(const Tileset& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: osg::Object(other, copyop),
			assetVersion(other.assetVersion),
			tilesetVersion(other.tilesetVersion),
			geometricError(other.geometricError),
			root(static_cast<Tile*>(other.root->clone(copyop))) {}

		virtual osg::Object* cloneType() const { return new Tileset(); }

		virtual osg::Object* clone(const osg::CopyOp& copyop) const { return new Tileset(*this, copyop); }

		virtual const char* libraryName() const { return "osgGISPlugins"; }

		virtual const char* className() const { return "Tileset"; }

		bool valid() const;
	};
}
#endif // !OSG_GIS_PLUGINS_TILESET_H
