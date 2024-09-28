#include "3dtiles/Tileset.h"
#include <osg/CoordinateSystemNode>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/ConvertUTF>
using namespace osgGISPlugins;

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
	j["asset"]["gltfUpAxis"] = gltfUpAxis;
	j["asset"]["generator"] = generator;
	j["geometricError"] = geometricError;
	j["root"] = root->toJson();
	return j;
}

bool Tileset::toFile(Config config) {
	this->computeGeometricError();

	if (!this->valid())
		return false;

	config.validate();
	this->root->write(config.path, config.simplifyRatio, config.gltfTextureOptions, config.options);
	this->computeTransform(config.longitude, config.latitude, config.height);

	const string filePath = config.path + "\\tileset.json";

	std::setlocale(LC_ALL, "zh_CN.UTF-8");
	ofstream file(filePath);
	if (!file.is_open()) {
		return false;
	}
#ifdef _WIN32
#else
	setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32

	json j = toJson();
#ifndef NDEBUG
	const string str = j.dump();
#else
	const string str = j.dump(4);//格式化输出
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
	this->geometricError = radius * CesiumGeometricErrorOperator;


	if (this->root->node.valid())
	{
		double groupGeometricErros = 0.0;
		osg::ref_ptr<osg::Group> group = this->root->node->asGroup();
		for (size_t i = 0; i < group->getNumChildren(); ++i)
		{
			cbv.reset();
			osg::ref_ptr<osg::Node> node = group->getChild(i);
			node->accept(cbv);
			bb = cbv.getBoundingBox();
			radius = bb.radius();
			groupGeometricErros += std::pow(radius * CesiumGeometricErrorOperator, 2);
		}
		this->geometricError = groupGeometricErros / this->geometricError;
	}
	if (this->geometricError <= this->root->geometricError)
		this->geometricError = this->root->geometricError * 1.5;
}
