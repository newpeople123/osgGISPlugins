#include "3dtiles/hlod/TreeBuilder.h"
#include "3dtiles/Tileset.h"
#include "utils/ObbVisitor.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include "utils/Simplifier.h"
#include <osgDB/WriteFile>
#include <osg/CoordinateSystemNode>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <osg/ComputeBoundsVisitor>
using namespace osgGISPlugins;
////////////////////////////////////////////////////////////////////////////
// BoundingVolume
////////////////////////////////////////////////////////////////////////////
void BoundingVolume::fromJson(const json& j)
{
	if (j.contains("region")) j.at("region").get_to(region);
	if (j.contains("box")) j.at("box").get_to(box);
	if (j.contains("sphere")) j.at("sphere").get_to(sphere);
}

json BoundingVolume::toJson() const {
	json j;
	if (!region.empty())
	{
		j["region"] = region;
		return j;
	}
	if (!box.empty())
	{
		j["box"] = box;
		return j;
	}
	if (!sphere.empty())
	{
		j["sphere"] = sphere;
		return j;
	}
	return j;
}

void BoundingVolume::computeBox(osg::ref_ptr<osg::Node> node)
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

void BoundingVolume::computeSphere(osg::ref_ptr<osg::Node> node)
{
	osg::ComputeBoundsVisitor cbv;
	node->accept(cbv);
	const osg::BoundingBox boundingBox = cbv.getBoundingBox();

	const osg::Matrixd mat = osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0f, 0.0f, 0.0f);
	const osg::Vec3f center = boundingBox.center() * mat;
	const float radius = boundingBox.radius();

	this->sphere = { center.x(), center.y(), center.z(), radius };
}

void BoundingVolume::computeRegion(osg::ref_ptr<osg::Node> node)
{
	const osg::EllipsoidModel ellipsoidModel;
	osg::Matrixd localToWorldMatrix;
	ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(30.0), osg::DegreesToRadians(116.0), 100, localToWorldMatrix);

	OBBVisitor ov;
	node->accept(ov);
	osg::ref_ptr<osg::Vec3Array> corners = ov.getOBBCorners();

	if (corners->size() != 8)
		return; // 确保 OBB 有 8 个角点

	// 初始化 min 和 max 值为第一个角点的坐标
	double minX = corners->at(0).x();
	double maxX = corners->at(0).x();
	double minY = corners->at(0).y();
	double maxY = corners->at(0).y();
	double minZ = corners->at(0).z();
	double maxZ = corners->at(0).z();

	// 遍历其他角点，更新 min 和 max 值
	for (size_t i = 1; i < corners->size(); ++i)
	{
		const osg::Vec3& corner = corners->at(i);
		minX = osg::minimum(minX, (double)corner.x());
		maxX = osg::maximum(maxX, (double)corner.x());
		minY = osg::minimum(minY, (double)corner.y());
		maxY = osg::maximum(maxY, (double)corner.y());
		minZ = osg::minimum(minZ, (double)corner.z());
		maxZ = osg::maximum(maxZ, (double)corner.z());

	}

	const osg::Vec3d minWorldCorner = osg::Vec3d(minX, minY, minZ) * osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0f, 0.0f, 0.0f) * localToWorldMatrix;
	double minLatitude, minLongitude, minHeight;
	ellipsoidModel.convertXYZToLatLongHeight(minWorldCorner.x(), minWorldCorner.y(), minWorldCorner.z(), minLatitude, minLongitude, minHeight);
	//minLatitude = osg::RadiansToDegrees(minLatitude);
	//minLongitude = osg::RadiansToDegrees(minLongitude);

	const osg::Vec3d maxWorldCorner = osg::Vec3d(maxX, maxY, maxZ) * osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0f, 0.0f, 0.0f) * localToWorldMatrix;
	double maxLatitude, maxLongitude, maxHeight;
	ellipsoidModel.convertXYZToLatLongHeight(maxWorldCorner.x(), maxWorldCorner.y(), maxWorldCorner.z(), maxLatitude, maxLongitude, maxHeight);
	//maxLatitude = osg::RadiansToDegrees(maxLatitude);
	//maxLongitude = osg::RadiansToDegrees(maxLongitude);
	this->region = {
		minLongitude > maxLongitude ? maxLongitude : minLongitude,//west
		minLatitude > maxLatitude ? maxLatitude : minLatitude,//south
		minLongitude > maxLongitude ? minLongitude : maxLongitude,//east
		minLatitude > maxLatitude ? minLatitude : maxLatitude,//north
		minHeight,
		maxHeight
	};
}

////////////////////////////////////////////////////////////////////////////
// Tile
////////////////////////////////////////////////////////////////////////////
void Tile::fromJson(const json& j) {
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

json Tile::toJson() const {
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

int Tile::getMaxLevel() const
{
	int currentLevel = this->level;
	for (auto& child : this->children)
	{
		currentLevel = osg::maximum(child->getMaxLevel(), currentLevel);
	}
	return currentLevel;
}

void Tile::buildBaseHlodAndComputeGeometricError()
{
	if (this->node.valid())
	{
		if (!this->parent.valid()) return;
		if (!this->parent->node.valid())
		{
			this->parent->node = new osg::Group;
		}
		osg::ref_ptr<osg::Group> parentGroup = this->parent->node->asGroup();

		osg::ref_ptr<osg::Group> group = this->node->asGroup();
		osg::ComputeBoundsVisitor cbv;
		this->node->accept(cbv);
		const osg::BoundingBox boundingBox = cbv.getBoundingBox();
		const double currentRadius = computeRadius(boundingBox, this->axis) * 0.7;

		for (size_t i = 0; i < group->getNumChildren(); ++i)
		{
			osg::ref_ptr<osg::Node> childNode = group->getChild(i);
			osg::ComputeBoundsVisitor childCbv;
			childNode->accept(childCbv);
			const osg::BoundingBox childBoundingBox = childCbv.getBoundingBox();
			const double childRadius = computeRadius(childBoundingBox, this->axis);

			if (childRadius >= currentRadius)
			{
				parentGroup->addChild(childNode);
			}
			else {
				const double childNodeGeometricError = childRadius * CesiumGeometricErrorOperator;
				this->parent->geometricError = osg::maximum(this->parent->geometricError, childNodeGeometricError);
			}
		}
		for (size_t i = 0; i < this->children.size(); ++i)
		{
			const double childGeometricError = this->children[i]->geometricError;
			if (this->geometricError <= childGeometricError)
			{
				this->geometricError = childGeometricError + 0.0001;
			}
		}
	}
	else
	{
		for (size_t i = 0; i < children.size(); ++i)
		{
			this->children[i]->buildHlod();
		}

		buildBaseHlodAndComputeGeometricError();
	}
}

void Tile::rebuildHlodAndComputeGeometricErrorByRefinement()
{
	if (this->node.valid())
	{

		osg::ref_ptr<osg::Group> group = new osg::Group;
		for (size_t i = 0; i < this->children.size(); ++i)
		{
			if (this->children[i]->node.valid())
			{
				osg::ref_ptr<osg::Group> currentNodeAsGroup = this->node->asGroup();
				osg::ref_ptr<osg::Group> childNodeAsGroup = this->children[i]->node->asGroup();
				for (size_t k = 0; k < childNodeAsGroup->getNumChildren(); ++k)
				{
					osg::ref_ptr<osg::Node> childNode = childNodeAsGroup->getChild(k);
					bool bFindNode = false;
					for (size_t j = 0; j < currentNodeAsGroup->getNumChildren(); ++j)
					{
						if (currentNodeAsGroup->getChild(j) == childNode)
						{
							bFindNode = true;
							break;
						}
					}
					if (!bFindNode)
						group->addChild(childNode);
				}
			}
		}

		TriangleCountVisitor tcv;
		this->node->accept(tcv);
		TriangleCountVisitor childTcv;
		group->accept(childTcv);
		if (childTcv.count * 2 < tcv.count)
		{
			osg::ref_ptr<osg::Group> currentNodeAsGroup = this->node->asGroup();
			for (size_t i = 0; i < this->children.size(); ++i)
			{
				if (this->children[i]->node.valid())
				{
					for (size_t j = 0; j < currentNodeAsGroup->getNumChildren(); ++j)
					{
						osg::ref_ptr<osg::Node> node = currentNodeAsGroup->getChild(j);
						osg::ref_ptr<osg::Group> childNodeAsGroup = this->children[i]->node->asGroup();
						for (size_t k = 0; k < childNodeAsGroup->getNumChildren(); ++k)
						{
							childNodeAsGroup->removeChild(node);
						}
					}
				}
			}
			this->refine = Refinement::ADD;
		}

		if (descendantNodeIsEmpty())
		{
			this->children.clear();
			this->geometricError = 0.0;
		}
	}

	for (size_t i = 0; i < this->children.size(); ++i)
	{

		if (this->children[i]->node->asGroup()->getNumChildren() == 0)
		{
			if (this->children[i]->children.size() == 0)
			{
				this->children.erase(this->children.begin() + i);
				i--;
				continue;
			}
		}

		this->children[i]->rebuildHlodAndComputeGeometricErrorByRefinement();
	}
}

bool Tile::descendantNodeIsEmpty() const
{
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		if (this->children[i]->node->asGroup()->getNumChildren() > 0)
			return false;
		if (!this->children[i]->descendantNodeIsEmpty())
			return false;
	}
	return true;
}

void Tile::setContentUri()
{
	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		contentUri = to_string(level) + "/" + "Tile_" + to_string(level) + "_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + ".b3dm";
	}
}

void Tile::computeBoundingVolumeBox()
{
	osg::ref_ptr<osg::Group> group = getAllDescendantNodes();
	boundingVolume.computeBox(group);
}

osg::ref_ptr<osg::Group> Tile::getAllDescendantNodes() const
{
	osg::ref_ptr<osg::Group> group = new osg::Group;
	osg::ref_ptr<osg::Group> currentNodeAsGroup = this->node->asGroup();
	for (size_t i = 0; i < currentNodeAsGroup->getNumChildren(); ++i)
	{
		group->addChild(currentNodeAsGroup->getChild(i));
	}
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		osg::ref_ptr<osg::Group> childGroup = this->children[i]->getAllDescendantNodes();
		for (size_t j = 0; j < childGroup->getNumChildren(); ++j)
		{
			group->addChild(childGroup->getChild(j));
		}
	}
	return group;
}

void Tile::buildHlod()
{
	buildBaseHlodAndComputeGeometricError();
	this->valid();
	rebuildHlodAndComputeGeometricErrorByRefinement();
}

bool Tile::valid() const
{
	const size_t size = this->children.size();
	if (size)
	{
		bool isValid = true;
		for (size_t i = 0; i < size; ++i)
		{
			if (this->geometricError <= this->children[i]->geometricError)
			{
				isValid = false;
				break;
			}
			if (!this->children[i]->valid())
			{
				isValid = false;
				break;
			}
		}
		return isValid;
	}
	else
		return this->geometricError == 0.0;
}

void Tile::write(const string& str, const float simplifyRatio, const GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options)
{
	computeBoundingVolumeBox();
	setContentUri();

	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		osg::ref_ptr<osg::Node> nodeCopy = osg::clone(node.get(), osg::CopyOp::DEEP_COPY_ALL);

		if (simplifyRatio < 1.0) {
			if (this->refine == Refinement::REPLACE)
			{
				Simplifier simplifier(pow(simplifyRatio, getMaxLevel() - level + 1), true);
				nodeCopy->accept(simplifier);
			}
			else
			{
				Simplifier simplifier(simplifyRatio);
				nodeCopy->accept(simplifier);
			}
		}
		GltfOptimizer gltfOptimzier;
		gltfOptimzier.setGltfTextureOptimizationOptions(gltfTextureOptions);
		gltfOptimzier.optimize(nodeCopy.get(), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);

		BatchIdVisitor biv;
		nodeCopy->accept(biv);

		const string path = str + "\\" + to_string(level);
		osgDB::makeDirectory(path);
		osgDB::writeNodeFile(*nodeCopy.get(), path + "\\" + "Tile_" + to_string(level) + "_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + ".b3dm", options);
		//osgDB::writeNodeFile(*nodeCopy.get(), path + "\\" + "Tile_" + to_string(level) + "_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + ".gltf", options);

	}
	/* single thread */
	//for (size_t i = 0; i < this->children.size(); ++i)
	//{
	//	this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
	//}

	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r) {
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
			}
		});

}

double Tile::computeRadius(const osg::BoundingBox& bbox, int axis)
{
	switch (axis) {
	case 0: // X-Y轴分割
		return (osg::Vec2(bbox._max.x(), bbox._max.y()) - osg::Vec2(bbox._min.x(), bbox._min.y())).length() / 2;
	case 1: // Y-Z轴分割
		return (osg::Vec2(bbox._max.z(), bbox._max.y()) - osg::Vec2(bbox._min.z(), bbox._min.y())).length() / 2;
	case 2: // Z-X轴分割
		return (osg::Vec2(bbox._max.z(), bbox._max.x()) - osg::Vec2(bbox._min.z(), bbox._min.x())).length() / 2;
	default:
		return 0.0;
	}
}

////////////////////////////////////////////////////////////////////////////
// Tileset
////////////////////////////////////////////////////////////////////////////
void Tileset::computeTransform(const double lng, const double lat, const double h)
{
	const osg::EllipsoidModel ellipsoidModel;
	osg::Matrixd matrix;
	ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(lat), osg::DegreesToRadians(lng), h, matrix);

	const double* ptr = matrix.ptr();
	for (unsigned i = 0; i < 16; ++i)
		this->root->transform.push_back(*ptr++);
}

bool Tileset::valid() const
{
	if (!this->root) return false;
	return this->geometricError > this->root->geometricError && this->root->valid();
}

void Tileset::fromJson(const json& j) {
	if (j.contains("asset")) {
		if (j.at("asset").contains("version"))
			j.at("asset").at("version").get_to(assetVersion);
		if (j.at("asset").contains("tilesetVersion"))
			j.at("asset").at("tilesetVersion").get_to(tilesetVersion);
	}
	if (j.contains("geometricError")) j.at("geometricError").get_to(geometricError);
	if (j.contains("root")) root->fromJson(j.at("root"));
}

Tileset Tileset::fromFile(const string& filePath) {
	ifstream file(filePath);
	if (!file.is_open()) {
		throw runtime_error("Unable to open file: " + filePath);
	}

	json j;
	file >> j;
	file.close();

	Tileset tileset;
	tileset.fromJson(j);
	return tileset;
}

json Tileset::toJson() const {
	json j;
	j["asset"]["version"] = assetVersion;
	j["asset"]["tilesetVersion"] = tilesetVersion;
	j["geometricError"] = geometricError;
	j["root"] = root->toJson();
	return j;
}

bool Tileset::toFile(Config config, osg::ref_ptr<osg::Node> originNode) {
	this->computeGeometricError(originNode);

	if (!this->valid())
		return false;

	config.validate();
	this->root->write(config.path, config.simplifyRatio, config.gltfTextureOptions, config.options);
	this->computeTransform(config.longitude, config.latitude, config.height);

	const string filePath = config.path + "\\tileset.json";
	ofstream file(filePath);
	if (!file.is_open()) {
		return false;
	}

	json j = toJson();
	const string str = j.dump(4);
	file << str;  // 格式化输出，缩进4个空格
	file.close();
	return true;
}

void Tileset::computeGeometricError(osg::ref_ptr<osg::Node> originNode)
{
	osg::ComputeBoundsVisitor cbv;
	originNode->accept(cbv);
	const osg::BoundingBox bb = cbv.getBoundingBox();
	const double radius = bb.radius();
	this->geometricError = radius * CesiumGeometricErrorOperator;
}
