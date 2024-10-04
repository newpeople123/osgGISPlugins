#include "3dtiles/BoundingVolume.h"
#include <osg/ComputeBoundsVisitor>
#include <osg/CoordinateSystemNode>
#include "utils/ObbVisitor.h"
using namespace osgGISPlugins;
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

void BoundingVolume::computeRegion(osg::ref_ptr<osg::Node> node, const double latitude, const double longitude, const double height)
{
	const osg::EllipsoidModel ellipsoidModel;
	osg::Matrixd localToWorldMatrix;
	ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(latitude), osg::DegreesToRadians(longitude), height, localToWorldMatrix);

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
