#include "3dtiles/Tileset.h"
#include <osg/CoordinateSystemNode>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/ConvertUTF>
using namespace osgGISPlugins;

void Tileset::computeTransform(const double lng, const double lat, const double alt)
{
	const osg::EllipsoidModel ellipsoidModel;
	osg::Matrixd localToWorld;
	ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(lat), osg::DegreesToRadians(lng), alt, localToWorld);

	const double* ptr = localToWorld.ptr();
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

Tileset Tileset::fromFile(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) {
		throw std::runtime_error("Unable to open file: " + filePath);
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
	j["asset"]["gltfUpAxis"] = gltfUpAxis;
	j["asset"]["generator"] = generator;
	j["geometricError"] = geometricError;
	j["root"] = root->toJson();
	return j;
}

bool Tileset::write() {
	this->computeGeometricError();

	if (!this->valid())
		return false;

	this->root->write();
	this->computeTransform(config.longitude, config.latitude, config.altitude);
	const std::string filePath = config.tileConfig.path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + "tileset.json";
	std::setlocale(LC_ALL, "zh_CN.UTF-8");
	std::ofstream file(filePath);
	if (!file.is_open()) {
		return false;
	}
#ifdef _WIN32
#else
	setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32

	json j = toJson();
#ifndef NDEBUG
	const std::string str = j.dump(4);//格式化输出
#else
	const std::string str = j.dump();
#endif
	file << str;
	file.close();
	return true;
}

void Tileset::computeGeometricError()
{
	osg::ComputeBoundsVisitor cbv;
	_node->accept(cbv);
	osg::BoundingBox bb = cbv.getBoundingBox();
	double radius = bb.radius();
	this->geometricError = Tile::getCesiumGeometricErrorByPixelSize(InitPixelSize, radius);


	if (this->root->node.valid())
	{
		double totalVolume = 0.0; // 用于计算加权平均
		double weightedErrorSum = 0.0; // 加权误差总和
		osg::ref_ptr<osg::Group> group = this->root->node->asGroup();
		const GltfOptimizer::GltfTextureOptimizationOptions textureOptions = config.tileConfig.gltfTextureOptions;
		const int maxTextureResolution = textureOptions.maxTextureWidth > textureOptions.maxTextureHeight ? textureOptions.maxTextureWidth : textureOptions.maxTextureHeight;
		for (size_t i = 0; i < group->getNumChildren(); ++i)
		{
			cbv.reset();
			osg::ref_ptr<osg::Node> node = group->getChild(i);
			node->accept(cbv);
			bb = cbv.getBoundingBox();
			const double diagonalLength = (bb._max - bb._min).length();

			// 计算体积
			const double volume = (bb._max.x() - bb._min.x()) *
				(bb._max.y() - bb._min.y()) *
				(bb._max.z() - bb._min.z());

			Utils::MaxGeometryVisitor mgv;
			node->accept(mgv);
			const osg::BoundingBox clusterMaxBound = mgv.maxBB;

			// 计算聚类体积
			const double clusterVolume = (clusterMaxBound._max.x() - clusterMaxBound._min.x()) *
				(clusterMaxBound._max.y() - clusterMaxBound._min.y()) *
				(clusterMaxBound._max.z() - clusterMaxBound._min.z());

			// 计算聚类体积的对角线长度
			const double clusterMaxDiagonalLength = (clusterMaxBound._min - clusterMaxBound._max).length();

			// 选择合适的误差范围
			const double range = diagonalLength > 2 * clusterMaxDiagonalLength ? clusterMaxDiagonalLength : radius;

			// 计算该子节点的几何误差
			const double childNodeGeometricError = Tile::getCesiumGeometricErrorByPixelSize(maxTextureResolution / 4, range * 0.7);

			// 加权误差计算
			weightedErrorSum += childNodeGeometricError * (diagonalLength > 2 * clusterMaxDiagonalLength ? volume : clusterVolume);
			totalVolume += (diagonalLength > 2 * clusterMaxDiagonalLength ? volume : clusterVolume);
		}

		// 如果总的体积大于0，计算加权平均几何误差
		if (totalVolume > 0.0)
		{
			this->geometricError = osg::maximum(this->geometricError, weightedErrorSum / totalVolume);
		}
	}

	this->geometricError = osg::maximum(this->geometricError, this->root->geometricError * 1.5);
}
