#ifndef OSG_GIS_PLUGINS_TILESET_H
#define OSG_GIS_PLUGINS_TILESET_H
#include "3dtiles/hlod/TreeBuilder.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <osg/Node>
#include <memory>
#include <osgDB/Options>
#include <osgDB/FileUtils>
#include <osg/NodeVisitor>
using json = nlohmann::json;
using namespace std;
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	class Tileset :public osg::Object {
	private:
		void computeTransform(const double lng, const double lat, const double height);

		void computeGeometricError();

		osg::ref_ptr<osg::Node> _node;

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
		string tilesetVersion = "1.0.0";
		const string generator = "osgGISPlugins";
		string gltfUpAxis = "Y";
		double geometricError;
		osg::ref_ptr<Tile> root;

		void fromJson(const json& j);

		static Tileset fromFile(const string& filePath);

		json toJson() const;

		bool toFile(Config config);

		Tileset() = default;

		Tileset(osg::ref_ptr<osg::Node> node, TreeBuilder& builder) :geometricError(0.0), _node(node) {
			osgUtil::Optimizer optimizer;
			optimizer.optimize(_node, osgUtil::Optimizer::INDEX_MESH);
			_node->accept(builder);
			root = builder.build();
			osg::ref_ptr<B3DMTile> b3dmNode = dynamic_cast<B3DMTile*>(root.get());
			if(b3dmNode.valid())
				b3dmNode->buildHlod();
		}

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

};
#endif // !OSG_GIS_PLUGINS_TILESET_H
