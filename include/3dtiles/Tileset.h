#ifndef OSG_GIS_PLUGINS_TILESET_H
#define OSG_GIS_PLUGINS_TILESET_H
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <osg/Node>
#include <memory>
#include <osg/ComputeBoundsVisitor>
#include "utils/ObbVisitor.h"

using json = nlohmann::json;
using namespace std;
namespace osgGISPlugins
{
	constexpr double InitialPixelSize = 25.0;
	constexpr double DetailPixelSize = InitialPixelSize * 5;
	constexpr double CesiumCanvasClientWidth = 1920;
	constexpr double CesiumCanvasClientHeight = 931;
	constexpr double CesiumFrustumAspectRatio = CesiumCanvasClientWidth / CesiumCanvasClientHeight;
	const double CesiumFrustumFov = osg::PI / 3;
	const double CesiumFrustumFovy = CesiumFrustumAspectRatio <= 1 ? CesiumFrustumFov : atan(tan(CesiumFrustumFov * 0.5) / CesiumFrustumAspectRatio) * 2.0;
	constexpr double CesiumFrustumNear = 0.1;
	constexpr double CesiumFrustumFar = 10000000000.0;
	constexpr double CesiumCanvasViewportWidth = CesiumCanvasClientWidth;
	constexpr double CesiumCanvasViewportHeight = CesiumCanvasClientHeight;
	const double CesiumSSEDenominator = 2.0 * tan(0.5 * CesiumFrustumFovy);
	constexpr double CesiumMaxScreenSpaceError = 16.0;
	const double CesiumGeometricErrorOperator = CesiumSSEDenominator * CesiumMaxScreenSpaceError / (CesiumCanvasClientHeight * tan(osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight * InitialPixelSize / 2));


	class TraingleAreaVisitor : public osg::NodeVisitor {
	public:
		double area = 0.0;
		TraingleAreaVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
		double computeTriangleArea(const osg::Vec3 a, const osg::Vec3 b, const osg::Vec3 c) {
			osg::Vec3 ab = b - a;
			osg::Vec3 ac = c - a;
			return (ab ^ ac).length() * 0.5;
		}
		double computeTriangleAreaXY(const osg::Vec3& A, const osg::Vec3& B, const osg::Vec3& C) {
			osg::Vec3 A_xy(A.x(), A.y(), 0.0);
			osg::Vec3 B_xy(B.x(), B.y(), 0.0);
			osg::Vec3 C_xy(C.x(), C.y(), 0.0);

			osg::Vec3 AB = B_xy - A_xy;
			osg::Vec3 AC = C_xy - A_xy;

			return (AB ^ AC).length() * 0.5;
		}

		void apply(osg::Drawable& drawable) override
		{
			osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
			const unsigned int psetCount = geom->getNumPrimitiveSets();
			const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
			if (psetCount <= 0) return;
			for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
				osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(primIndex);
				if (typeid(*pset.get()) == typeid(osg::DrawElementsUShort)) {
					osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
					for (size_t i = 0; i < drawElementsUShort->size(); i += 3) {
						const unsigned short aIndex = drawElementsUShort->at(i + 0);
						const unsigned short bIndex = drawElementsUShort->at(i + 1);
						const unsigned short cIndex = drawElementsUShort->at(i + 2);

						const osg::Vec3 a = positions->at(aIndex);
						const osg::Vec3 b = positions->at(bIndex);
						const osg::Vec3 c = positions->at(cIndex);
						area += computeTriangleAreaXY(a, b, c);
					}
				}
				else if (typeid(*pset.get()) == typeid(osg::DrawElementsUInt)) {
					osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
					for (size_t i = 0; i < drawElementsUInt->size(); i += 3) {
						const unsigned int aIndex = drawElementsUInt->at(i + 0);
						const unsigned int bIndex = drawElementsUInt->at(i + 1);
						const unsigned int cIndex = drawElementsUInt->at(i + 2);

						const osg::Vec3 a = positions->at(aIndex);
						const osg::Vec3 b = positions->at(bIndex);
						const osg::Vec3 c = positions->at(cIndex);
						area += computeTriangleAreaXY(a, b, c);
					}
				}
			}
		}

	};

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

		virtual osg::Object* cloneType() const { return new BoundingVolume(); }

		virtual osg::Object* clone(const osg::CopyOp& copyop) const { return new BoundingVolume(*this, copyop); }

		virtual const char* libraryName() const { return "osgGISPlugins"; }

		virtual const char* className() const { return "BoundingVolume"; }

		void computeBox(osg::ref_ptr<osg::Node> node)
		{
			osg::ComputeBoundsVisitor cbv;
			node->accept(cbv);
			const osg::BoundingBox boundingBox = cbv.getBoundingBox();
			const osg::Matrixd mat = osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0f, 0.0f, 0.0f);
			const osg::Vec3f size = boundingBox._max * mat - boundingBox._min * mat;
			const osg::Vec3f cesiumBoxCenter = boundingBox.center() * mat;

			this->box.push_back(cesiumBoxCenter.x());
			this->box.push_back(cesiumBoxCenter.y());
			this->box.push_back(cesiumBoxCenter.z());

			this->box.push_back(size.x() / 2);
			this->box.push_back(0);
			this->box.push_back(0);

			this->box.push_back(0);
			this->box.push_back(size.y() / 2);
			this->box.push_back(0);

			this->box.push_back(0);
			this->box.push_back(0);
			this->box.push_back(size.z() / 2);
		}

		void computeSphere(osg::ref_ptr<osg::Node> node)
		{
			osg::ComputeBoundsVisitor cbv;
			node->accept(cbv);
			const osg::BoundingBox boundingBox = cbv.getBoundingBox();

			const osg::Matrixd mat = osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0f, 0.0f, 0.0f);
			const osg::Vec3f center = boundingBox.center() * mat;
			const float radius = boundingBox.radius();

			this->sphere = { center.x(), center.y(), center.z(), radius };
		}

		void computeRegion(osg::ref_ptr<osg::Node> node)
		{
			OBBVisitor ov;
			node->accept(ov);
			osg::ref_ptr<osg::Vec3Array> corners = ov.getOBBCorners();

			if (corners->size() != 8)
				return; // 确保 OBB 有 8 个角点

			// 初始化 min 和 max 值为第一个角点的坐标
			double minLon = corners->at(0).x();
			double maxLon = corners->at(0).x();
			double minLat = corners->at(0).y();
			double maxLat = corners->at(0).y();
			double minHeight = corners->at(0).z();
			double maxHeight = corners->at(0).z();

			// 遍历其他角点，更新 min 和 max 值
			for (size_t i = 1; i < corners->size(); ++i)
			{
				const osg::Vec3& corner = corners->at(i);
				minLon = osg::minimum(minLon, (double)corner.x());
				maxLon = osg::maximum(maxLon, (double)corner.x());
				minLat = osg::minimum(minLat, (double)corner.y());
				maxLat = osg::maximum(maxLat, (double)corner.y());
				minHeight = osg::minimum(minHeight, (double)corner.z());
				maxHeight = osg::maximum(maxHeight, (double)corner.z());
			}

			// 将计算得到的 region 信息存储到 this->region 向量中
			this->region = { minLon, maxLon, minLat, maxLat, minHeight, maxHeight };
		}
	};

	class Tile :public osg::Object {
	public:
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


		Tile(osg::ref_ptr<osg::Node> node,osg::ref_ptr<Tile> parent)
			: parent(parent) , node(node) {}

		Tile(osg::ref_ptr<Tile> parent)
			: parent(parent) {}

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
					osg::ref_ptr<Tile> childTile = new Tile;
					childTile->fromJson(child);
					children.push_back(childTile);
				}
			}
		}

		json toJson() {
			json j;
			boundingVolume.computeBox(node);
			j["boundingVolume"] = boundingVolume.toJson();
			j["geometricError"] = geometricError;
			j["refine"] = (refine == Refinement::REPLACE) ? "REPLACE" : "ADD";

			contentUri = to_string(level) + "/" + "Tile" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + ".b3dm";
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

		int getMaxLevel() const
		{
			int currentLevel = this->level;
			for (auto& child : this->children)
			{
				currentLevel = osg::maximum(child->getMaxLevel(), currentLevel);
			}
			return currentLevel;
		}

		virtual osg::Object* cloneType() const { return new Tile(); }

		virtual osg::Object* clone(const osg::CopyOp& copyop) const { return new Tile(*this, copyop); }

		virtual const char* libraryName() const { return "osgGISPlugins"; }

		virtual const char* className() const { return "Tile"; }

		void buildHlod();

		void computeGeometricError();

		bool geometricErrorValid();

		void write(const string& path);
	};

	class Tileset :public osg::Object {
	public:
		string assetVersion = "1.0";
		string tilesetVersion;
		double geometricError;
		osg::ref_ptr<Tile> root;

		void fromJson(const json& j) {
			if (j.contains("asset")) {
				if (j.at("asset").contains("version"))
					j.at("asset").at("version").get_to(assetVersion);
				if (j.at("asset").contains("tilesetVersion"))
					j.at("asset").at("tilesetVersion").get_to(tilesetVersion);
			}
			if (j.contains("geometricError")) j.at("geometricError").get_to(geometricError);
			if (j.contains("root")) root->fromJson(j.at("root"));
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
			j["root"] = root->toJson();
			return j;
		}

		void toFile(const string& file_path) const {
			ofstream file(file_path);
			if (!file.is_open()) {
				throw runtime_error("Unable to open file: " + file_path);
			}

			json j = toJson();
			const string str = j.dump(4);
			file << str;  // 格式化输出，缩进4个空格
			file.close();
		}

		Tileset() : root(new Tile),geometricError(0.0) {}

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

		void computeGeometricError(osg::ref_ptr<osg::Node> node);

		bool geometricErrorValid();
	};
}
#endif // !OSG_GIS_PLUGINS_TILESET_H
