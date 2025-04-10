#ifndef OSG_GIS_PLUGINS_TILESET_H
#define OSG_GIS_PLUGINS_TILESET_H
#ifndef OSG_GIS_PLUGINS_PATH_SPLIT_STRING

#ifdef _WIN32
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "\\"
#else
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "/"
#endif
#endif // !OSG_GIS_PLUGINS_PATH_SPLIT_STRING
#include "3dtiles/hlod/TreeBuilder.h"
#include "3dtiles/Tile.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <osg/Node>
#include <osg/NodeVisitor>

using json = nlohmann::json;
using namespace std;
using namespace osgGISPlugins;
namespace osgGISPlugins
{
	class Tileset :public osg::Object {
	private:
		void computeTransform(double lng, double lat, double height) const;

		void computeGeometricError();

		osg::ref_ptr<osg::Node> _node;

	public:
		struct Config {
			//纬度，角度制
			double latitude = 30.0;
			//经度，角度制
			double longitude = 116.0;
			//高度，单位米
			double height = 100.0;

			Tile::Config tileConfig;

			void validate() {
				osg::clampTo(this->latitude, -90.0, 90.0);
				osg::clampTo(this->longitude, -180.0, 180.0);

				tileConfig.validate();
			}
		};
		string assetVersion = "1.0";
		string tilesetVersion = "1.0.0";
		const string generator = "osgGISPlugins";
		string gltfUpAxis = "Y";
		double geometricError;
		osg::ref_ptr<Tile> root;

		Config config;

		void fromJson(const json& j);

		static Tileset fromFile(const string& filePath);

		json toJson() const;

		bool write();

		Tileset() = default;

		Tileset(osg::ref_ptr<osg::Node> node, TreeBuilder& builder,Config iConfig) :_node(node), geometricError(0.0), config(iConfig){
			config.validate();
			osgUtil::Optimizer optimizer;
			optimizer.optimize(_node, osgUtil::Optimizer::INDEX_MESH);
			_node->accept(builder);
			root = builder.build();
			root->config = config.tileConfig;
			root->build();
		}

		Tileset(const Tileset& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: osg::Object(other, copyop),
			assetVersion(other.assetVersion),
			tilesetVersion(other.tilesetVersion),
			geometricError(other.geometricError),
			root(static_cast<Tile*>(other.root->clone(copyop))) {}

		osg::Object* cloneType() const override { return new Tileset(); }

		osg::Object* clone(const osg::CopyOp& copyop) const override { return new Tileset(*this, copyop); }

		const char* libraryName() const override { return "osgGISPlugins"; }

		const char* className() const override { return "Tileset"; }

		bool valid() const;
	};

};
#endif // !OSG_GIS_PLUGINS_TILESET_H
