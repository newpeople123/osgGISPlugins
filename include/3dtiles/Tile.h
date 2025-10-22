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
#ifdef USE_TBB_PARALLEL
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#endif // USE_TBB_PARALLEL
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	constexpr float InitPixelSize = 25.0;
	constexpr int CesiumCanvasClientWidth = 1920;
	constexpr int CesiumCanvasClientHeight = 936;// if fullscreen,set 1080
	constexpr float CesiumFrustumAspectRatio = CesiumCanvasClientWidth / CesiumCanvasClientHeight;
	const float CesiumFrustumFov = osg::PI / 3.0;
	const double CesiumFrustumFovy = CesiumFrustumAspectRatio <= 1 ? CesiumFrustumFov : atan(tan(CesiumFrustumFov * 0.5) / CesiumFrustumAspectRatio) * 2.0;
	constexpr float CesiumFrustumNear = 0.1;
	constexpr double CesiumFrustumFar = 10000000000.0;
	constexpr int CesiumCanvasViewportWidth = CesiumCanvasClientWidth;
	constexpr int CesiumCanvasViewportHeight = CesiumCanvasClientHeight;
	const float CesiumSSEDenominator = 2.0 * tan(0.5 * CesiumFrustumFovy);
	const float DPP = osg::maximum(CesiumFrustumFovy, 1.0e-17) / CesiumCanvasViewportHeight;
	constexpr float CesiumMaxScreenSpaceError = 16.0;
	constexpr float MIN_GEOMETRIC_ERROR_DIFFERENCE = 0.1;  // 父子节点几何误差最小差值
	constexpr float DIAGONAL_SCALE_FACTOR = 0.7;    // 几何误差缩放因子
	constexpr float LOD_FACTOR = 1.0;

	enum class Refinement {
		REPLACE,
		ADD
	};

	class Tile :public osg::Object
	{
	public:
		struct Config
		{
			//存储3dtiles的文件夹
			std::string path = "";
			//简化比例，[0-1]
			float simplifyRatio = 0.5f;
			//纹理优化设置
			GltfOptimizer::GltfTextureOptimizationOptions gltfTextureOptions;
			//b3dm、i3dm瓦片导出设置
			osg::ref_ptr<osgDB::Options> options = new osgDB::Options;

			unsigned int gltfOptimizerOptions = GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS | GltfOptimizer::FLATTEN_TRANSFORMS;

			void validate() {
				osg::clampTo(this->simplifyRatio, 0.f, 0.9f);

				this->gltfTextureOptions.maxTextureWidth = osg::Image::computeNearestPowerOfTwo(this->gltfTextureOptions.maxTextureWidth);
				this->gltfTextureOptions.maxTextureHeight = osg::Image::computeNearestPowerOfTwo(this->gltfTextureOptions.maxTextureHeight);
				this->gltfTextureOptions.maxTextureAtlasWidth = osg::Image::computeNearestPowerOfTwo(this->gltfTextureOptions.maxTextureAtlasWidth);
				this->gltfTextureOptions.maxTextureAtlasHeight = osg::Image::computeNearestPowerOfTwo(this->gltfTextureOptions.maxTextureAtlasHeight);

				osg::clampTo(this->gltfTextureOptions.maxTextureWidth, 128, 8192);
				osg::clampTo(this->gltfTextureOptions.maxTextureHeight, 128, 8192);
				osg::clampTo(this->gltfTextureOptions.maxTextureAtlasWidth, 256, 8192);
				osg::clampTo(this->gltfTextureOptions.maxTextureAtlasHeight, 256, 8192);

				std::string ext = std::string(this->gltfTextureOptions.ext.begin(), this->gltfTextureOptions.ext.end());
				std::transform(ext.begin(), ext.end(), ext.begin(),
					[](unsigned char c) { return std::tolower(c); });
				if (ext != ".jpg" && ext != ".png" && ext != ".webp" && ext != ".ktx2")
				{
					this->gltfTextureOptions.ext = ".ktx2";
				}
			}
		};

		Config config;
		std::string type = "";
		osg::ref_ptr<Tile> parent;
		osg::ref_ptr<osg::Node> node;
		BoundingVolume boundingVolume;
		double geometricError = 0.0;
		Refinement refine = Refinement::ADD;
		string contentUri;
		vector<osg::ref_ptr<Tile>> children;
		vector<double> transform;
		int level = -1;
		int x = -1;
		int y = -1;
		int z = -1;
		int lod = -1;
		float lodError = 0.0;
		double diagonalLength = 0.0;
		double volume = 0.0;

		Tile() = default;
		Tile(osg::ref_ptr<Tile> parent, const std::string& type)
			: parent(parent), type(type) {}
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
			type(other.type) {}
		Tile(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent, const std::string& type)
			: parent(parent), node(node), type(type) {}

		virtual const char* libraryName() const { return "osgGISPlugins"; }
		virtual const char* className() const { return "Tile"; }

		json toJson() const;

		int getMaxLevel() const;

		bool valid() const;

		virtual void build();

		virtual void write();

		virtual void fromJson(const json& j);

		static double getCesiumGeometricErrorByPixelSize(const float pixelSize, const float radius);

		static double getCesiumGeometricErrorByLodError(const float lodError, const double radius);

		static double getCesiumGeometricErrorByDistance(const float distance);

		static double getDistanceByPixelSize(const float pixelSize, const float radius);

		static void getVolumeWeightedDiagonalLength(osg::ref_ptr<osg::Group> group, double& outDiagonalLength, double& outVolume);
	protected:
		void cleanupEmptyNodes();

		bool descendantNodeIsEmpty() const;

		osg::ref_ptr<osg::Group> getAllDescendantNodes() const;

		void computeBoundingVolumeBox();

		void computeDiagonalLengthAndVolume(const osg::ref_ptr<osg::Node>& node);

		void optimizeNode(const unsigned int options);

		virtual void optimizeNode();

		virtual void buildLOD();

		virtual void computeGeometricError();

		virtual void computeDiagonalLengthAndVolume();

		virtual void writeChildren();

		virtual void applyLODStrategy();

		virtual bool writeNode();

		virtual void writeToFile();

		osg::ref_ptr<Tile> createLODTile(osg::ref_ptr<Tile> parent, osg::ref_ptr<osg::Node> waitCopyNode, int lodLevel);
		osg::ref_ptr<Tile> createLODTileWithoutNode(osg::ref_ptr<Tile> parent, int lodLevel);

		virtual Tile* createTileOfSameType(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent) = 0;
		virtual string getOutputPath() const = 0;
		virtual string getFullPath() const = 0;
		virtual string getTextureCachePath(const string textureCachePath) const = 0;
		virtual void setContentUri() = 0;
	private:
		void applyLODStrategy(const float simplifyRatioFactor, const float textureFactor);
	};
}
#endif // !OSG_GIS_PLUGINS_TILE_H
