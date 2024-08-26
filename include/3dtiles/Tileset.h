#ifndef OSG_GIS_PLUGINS_TILESET_H
#define OSG_GIS_PLUGINS_TILESET_H
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <osg/Node>
#include <memory>

using json = nlohmann::json;
using namespace std;
namespace osgGISPlugins
{
	enum class Refinement {
		REPLACE,
		ADD
	};

	class BoundingVolume {
	public:
		vector<double> region;
		vector<double> box;
		vector<double> sphere;
		void fromJson(const json& j) {
			if (j.contains("region")) j.at("region").get_to(region);
			if (j.contains("box")) j.at("box").get_to(box);
			if (j.contains("sphere")) j.at("sphere").get_to(sphere);
		}

		json toJson() const {
			json j;
			if (!region.empty()) j["region"] = region;
			if (!box.empty()) j["box"] = box;
			if (!sphere.empty()) j["sphere"] = sphere;
			return j;
		}
	};

	class Tile {
	private:
		shared_ptr<Tile> _parent;
		osg::ref_ptr<osg::Node> _node;
	public:
		BoundingVolume boundingVolume;
		double geometricError = 0.0;
		Refinement refine = Refinement::REPLACE;
		string contentUri;
		vector<shared_ptr<Tile>> children;
		vector<double> transform;

		Tile() = default;

		Tile(shared_ptr<Tile> parent)
			: _parent(parent) {}

		void fromJson(const json& j) {
			if (j.contains("boundingVolume")) boundingVolume.fromJson(j.at("boundingVolume"));
			if (j.contains("geometricError")) j.at("geometricError").get_to(geometricError);
			if (j.contains("refine")) {
				string refineStr;
				j.at("refine").get_to(refineStr);
				if (refineStr == "REPLACE") {
					refine = Refinement::REPLACE;
				}
				else if (refineStr == "ADD") {
					refine = Refinement::ADD;
				}
			}
			if (j.contains("content") && j.at("content").contains("uri")) {
				j.at("content").at("uri").get_to(contentUri);
			}

			if (j.contains("transform")) {
				j.at("transform").get_to(transform);
			}

			if (j.contains("children")) {
				for (const auto& child : j.at("children")) {
					shared_ptr<Tile> childTile = make_shared<Tile>();
					childTile->fromJson(child);
					children.push_back(childTile);
				}
			}
		}

		json toJson() const {
			json j;
			j["boundingVolume"] = boundingVolume.toJson();
			j["geometricError"] = geometricError;
			j["refine"] = (refine == Refinement::REPLACE) ? "REPLACE" : "ADD";

			if (!contentUri.empty()) {
				j["content"]["uri"] = contentUri;
			}

			if (!transform.empty()) {
				j["transform"] = transform;
			}

			if (!children.empty()) {
				j["children"] = json::array();
				for (const auto& child : children) {
					j["children"].push_back(child->toJson());
				}
			}
			return j;
		}
	};

	class Tileset {
	public:
		string assetVersion = "1.0";
		string tilesetVersion;
		double geometricError;
		Tile root;

		void fromJson(const json& j) {
			if (j.contains("asset")) {
				if (j.at("asset").contains("version"))
					j.at("asset").at("version").get_to(assetVersion);
				if (j.at("asset").contains("tilesetVersion"))
					j.at("asset").at("tilesetVersion").get_to(tilesetVersion);
			}
			if (j.contains("geometricError")) j.at("geometricError").get_to(geometricError);
			if (j.contains("root")) root.fromJson(j.at("root"));
		}

		static Tileset fromFile(const string& file_path) {
			ifstream file(file_path);
			if (!file.is_open()) {
				throw runtime_error("Unable to open file: " + file_path);
			}

			json j;
			file >> j;
			file.close();

			Tileset tileset;
			tileset.fromJson(j);
			return tileset;
		}

		json toJson() const {
			json j;
			j["asset"]["version"] = assetVersion;
			j["asset"]["tilesetVersion"] = tilesetVersion;
			j["geometricError"] = geometricError;
			j["root"] = root.toJson();
			return j;
		}

		void toFile(const string& file_path) const {
			ofstream file(file_path);
			if (!file.is_open()) {
				throw runtime_error("Unable to open file: " + file_path);
			}

			json j = toJson();
			file << j.dump(4);  // 格式化输出，缩进4个空格
			file.close();
		}
	};
}
#endif // !OSG_GIS_PLUGINS_TILESET_H
