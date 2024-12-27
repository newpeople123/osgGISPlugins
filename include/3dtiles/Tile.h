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
#include <tbb/spin_mutex.h>
// 控制导出3dtiles时是否为单线程(启用该宏则为单线程)
#define OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
using namespace osgGISPlugins;
namespace osgGISPlugins
{

	constexpr double InitPixelSize = 10.0;
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
	constexpr double MIN_GEOMETRIC_ERROR_DIFFERENCE = 0.1;  // 父子节点几何误差最小差值
	constexpr double GEOMETRIC_ERROR_SCALE_FACTOR = 0.7;    // 几何误差缩放因子

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

			void validate() {
				osg::clampTo(this->simplifyRatio, 0.f, 1.f);

				this->gltfTextureOptions.maxTextureWidth = osg::Image::computeNearestPowerOfTwo(this->gltfTextureOptions.maxTextureWidth);
				this->gltfTextureOptions.maxTextureHeight = osg::Image::computeNearestPowerOfTwo(this->gltfTextureOptions.maxTextureHeight);
				this->gltfTextureOptions.maxTextureAtlasWidth = osg::Image::computeNearestPowerOfTwo(this->gltfTextureOptions.maxTextureAtlasWidth);
				this->gltfTextureOptions.maxTextureAtlasHeight = osg::Image::computeNearestPowerOfTwo(this->gltfTextureOptions.maxTextureAtlasHeight);

				osg::clampTo(this->gltfTextureOptions.maxTextureWidth, 2, 8192);
				osg::clampTo(this->gltfTextureOptions.maxTextureHeight, 2, 8192);
				osg::clampTo(this->gltfTextureOptions.maxTextureAtlasWidth, 2, 8192);
				osg::clampTo(this->gltfTextureOptions.maxTextureAtlasHeight, 2, 8192);

				std::string ext = std::string(this->gltfTextureOptions.ext.begin(), this->gltfTextureOptions.ext.end());
				std::transform(ext.begin(), ext.end(), ext.begin(),
					[](unsigned char c) { return std::tolower(c); });
				if (ext != ".jpg" && ext != ".png" && ext != ".webp" && ext != ".ktx2")
				{
					this->gltfTextureOptions.ext = ".jpg";
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

		virtual void computeGeometricError();

		bool descendantNodeIsEmpty() const;

		osg::ref_ptr<osg::Group> getAllDescendantNodes() const;

		bool valid() const;
		/**
		 * 计算基于像素大小的几何误差系数
		 * @param pixelSize 目标像素大小
		 * @return 几何误差系数
		 * 
		 * 计算原理：
		 * 1. 首先计算每像素对应的视角(dpp)
		 * 2. 然后计算给定像素大小对应的视角
		 * 3. 最后根据屏幕空间误差要求计算几何误差系数
		 */
		static double getCesiumGeometricErrorOperatorByPixelSize(const float pixelSize);

		virtual void write() = 0;

		void validate();

		virtual void buildLOD() = 0;
	protected:
#ifndef OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
		tbb::spin_mutex writeMutex;
#endif // !OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD

	};
}
#endif // !OSG_GIS_PLUGINS_TILE_H
