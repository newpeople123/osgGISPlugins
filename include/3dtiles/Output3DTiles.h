#ifndef OSG_GIS_PLUGINS_OUTPUT3DTILES
#define OSG_GIS_PLUGINS_OUTPUT3DTILES 1
#include "QuadTreeBuilder.h"
#include "OctreeBuilder.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include <osg/CoordinateSystemNode>
#include <osgUtil/CullVisitor>
#include <future>

const double CesiumCanvasClientWidth = 1920;
const double CesiumCanvasClientHeight = 931;
const double CesiumFrustumAspectRatio = CesiumCanvasClientWidth / CesiumCanvasClientHeight;
const double CesiumFrustumFov = osg::PI / 3;
const double CesiumFrustumFovy = CesiumFrustumAspectRatio <= 1 ? CesiumFrustumFov : atan(tan(CesiumFrustumFov * 0.5) / CesiumFrustumAspectRatio) * 2.0;
const double CesiumFrustumNear = 0.1;
const double CesiumFrustumFar = 10000000000.0;
const double CesiumCanvasViewportWidth = CesiumCanvasClientWidth;
const double CesiumCanvasViewportHeight = CesiumCanvasClientHeight;
const double CesiumSSEDenominator = 2.0 * tan(0.5 * CesiumFrustumFovy);
const double CesiumMaxScreenSpaceError = 16.0;
double getPixelSize(const double& distance, const double& radius) {

	const double angularSize = osg::RadiansToDegrees(2.0 * atan(radius / distance));
	const double dpp = osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight;
	double pixelSize = angularSize / dpp;
	return pixelSize;
}
/// <summary>
/// Calculate the distance between the model and the viewpoint based on the pixel size
/// </summary>
/// <param name="radius"></param>
/// <param name="hopePixelSize">20 pixels: visible;700 pixels: can see the outline clearly;1000 pixels : can see details clearly</param>
/// <returns></returns>
double getDistance(const double& radius, const double& hopePixelSize = 500.0f) {
	//const double pixelSizeRatio = 100.0 / 3.0;
	//const double pixelSize = hopePixelSize * pixelSizeRatio;

	const double pixelSize = hopePixelSize;

	const double dpp = osg::maximum(CesiumFrustumFov, 1.0e-17) / CesiumCanvasViewportHeight;
	const double angularSize = dpp * pixelSize;

	double distance = radius / tan(osg::DegreesToRadians(angularSize) / 2);
	return distance;
}
double getGeometricError(const TileNode& node) {

	//osgUtil::CullVisitor* t = new osgUtil::CullVisitor;
	//double pS = t->pixelSize(node.currentNodes->getBound().center(), node.currentNodes->getBound().radius());
	double geometricError = 0.0;
	double distance = 0.0;
	const double radius = node.nodes->getBound().radius();
	osg::ComputeBoundsVisitor cbv;
	node.nodes->accept(cbv);
	const osg::Vec3 max = cbv.getBoundingBox()._max;
	const osg::Vec3 min = cbv.getBoundingBox()._min;
	const double radius2 = (max - min).length() / 2;
	//distance = getDistance(radius2);
	if (node.level == 0) {
		distance = getDistance(radius);
	}
	else if (node.children->getNumChildren() > 0) {
		distance = getDistance(radius);
	}
	geometricError = distance * CesiumSSEDenominator * CesiumMaxScreenSpaceError / CesiumCanvasClientHeight;
	return geometricError;
}
void getAllTreeNodesGeometricError(TileNode& node) {
	for (unsigned int i = 0; i < node.children->getNumChildren(); ++i) {
		osg::ref_ptr<TileNode> child = dynamic_cast<TileNode*>(node.children->getChild(i));
		getAllTreeNodesGeometricError(*child);
	}
	if (node.children->getNumChildren() == 0) {
		if (node.level == 0) {
			node.geometricError = getGeometricError(node);
		}
		else {
			node.geometricError = 0.0;
		}
	}
	else {
		double geometricError = std::max(getGeometricError(node), 0.0);
		node.geometricError = geometricError;
	}
}

void outputTreeNode(const TileNode& node, const osg::ref_ptr<osgDB::Options>& option, const std::string& output, int level, std::vector<double> transform = std::vector<double>()) {

	std::string childOutput = "", tilesetPath = "", b3dmPath = "";
	if (level != 0) {
		childOutput = output + "/" + std::to_string(node.level - 1);
		osgDB::makeDirectory(childOutput);
		tilesetPath = childOutput + "/L" + std::to_string(node.level - 1) + "_" + std::to_string(node.x) + "_" + std::to_string(node.y) + "_" + std::to_string(node.z) + ".json";
		b3dmPath = childOutput + "/" + "L" + std::to_string(node.level - 1) + "_" + std::to_string(node.x) + "_" + std::to_string(node.y) + "_" + std::to_string(node.z) + ".b3dm";
	}
	else {
		tilesetPath = output + "/tileset.json";
	}

	json tileset;
	tileset["asset"]["version"] = "1.0";
	tileset["asset"]["tilesetVersion"] = "1.0.0";
	tileset["root"]["refine"] = node.refine;
	for (const double& i : node.box) {
		tileset["root"]["boundingVolume"]["box"].push_back(i);
	}
	for (double i : transform) {
		tileset["root"]["transform"].push_back(i);
	}

	if (level == 0) {
		if (node.children->getNumChildren()) {
			double maxGeometricError = 0.0;
			for (unsigned int i = 0; i < node.children->getNumChildren(); ++i) {
				osg::ref_ptr<TileNode> child = dynamic_cast<TileNode*>(node.children->getChild(i));
				json root;
				for (const double& i : child->box) {
					root["boundingVolume"]["box"].push_back(i);
				}
				const std::string uri = std::to_string(child->level - 1) + "/" + "L" + std::to_string(child->level - 1) + "_" + std::to_string(child->x) + "_" + std::to_string(child->y) + "_" + std::to_string(child->z) + ".json";
				root["content"]["uri"] = uri;
				root["geometricError"] = child->geometricError;
				root["refine"] = child->refine;
				tileset["root"]["children"].push_back(root);
				maxGeometricError = maxGeometricError > root["geometricError"] ? maxGeometricError : root["geometricError"];
			}
			tileset["geometricError"] = std::max(maxGeometricError, node.geometricError) * 1.2;
			tileset["root"]["geometricError"] = node.geometricError;
		}
		else {
			if (node.currentNodes != nullptr && node.currentNodes->getNumChildren()) {
				tileset["geometricError"] = node.geometricError * 1.2;
				tileset["root"]["geometricError"] = node.geometricError;
				b3dmPath = output + "/root.b3dm";
			}
		}
	}
	else if (node.children->getNumChildren() == 0) {
		tileset["root"]["content"]["uri"] = "L" + std::to_string(node.level - 1) + "_" + std::to_string(node.x) + "_" + std::to_string(node.y) + "_" + std::to_string(node.z) + ".b3dm";
		tileset["geometricError"] = node.geometricError;
		tileset["refine"] = node.refine;
		tileset["root"]["geometricError"] = tileset["geometricError"];
	}
	else {
		if (node.currentNodes != nullptr && node.currentNodes->getNumChildren()) {
			tileset["root"]["content"]["uri"] = "L" + std::to_string(node.level - 1) + "_" + std::to_string(node.x) + "_" + std::to_string(node.y) + "_" + std::to_string(node.z) + ".b3dm";
		}
		double maxGeometricError = 0;
		for (unsigned int i = 0; i < node.children->getNumChildren(); ++i) {
			osg::ref_ptr<TileNode> child = dynamic_cast<TileNode*>(node.children->getChild(i));
			json root;
			for (const double& i : child->box) {
				root["boundingVolume"]["box"].push_back(i);
			}
			root["content"]["uri"] = "../" + std::to_string(child->level - 1) + "/" + "L" + std::to_string(child->level - 1) + "_" + std::to_string(child->x) + "_" + std::to_string(child->y) + "_" + std::to_string(child->z) + ".json";

			root["geometricError"] = child->geometricError;
			root["refine"] = child->refine;
			tileset["root"]["children"].push_back(root);
			maxGeometricError = maxGeometricError > root["geometricError"] ? maxGeometricError : root["geometricError"];
		}
		tileset["root"]["geometricError"] = std::max(maxGeometricError, node.geometricError);
		tileset["geometricError"] = std::max(maxGeometricError, node.geometricError) * 1.2;
	}

	std::ofstream tilesetFile(tilesetPath);
	if (tilesetFile.is_open()) {
		tilesetFile << tileset.dump();
		tilesetFile.close();

		if (b3dmPath != "" && node.currentNodes != nullptr && node.currentNodes->getNumChildren()) {
			osg::ref_ptr<osg::Node> outputNode = node.currentNodes->asNode();
			std::cout << b3dmPath << std::endl;
			osgDB::writeNodeFile(*outputNode.get(), b3dmPath, option);
			outputNode = nullptr;
		}
	}
	std::vector<std::future<void>> futures;
	for (unsigned int i = 0; i < node.children->getNumChildren(); ++i) {
		osg::ref_ptr<TileNode> child = dynamic_cast<TileNode*>(node.children->getChild(i));
		futures.push_back(std::async(std::launch::async, outputTreeNode, *child.get(), option, output, level + 1, std::vector<double>()));
		//outputTreeNode(*child.get(), option, output, level + 1);

	}
	for (auto& future : futures) {
		future.get();
	}
}
void OsgNodeTo3DTiles(const osg::ref_ptr<osg::Node> osgNode, const osg::ref_ptr<osgDB::Options>& option, const std::string& type, const double max, const double ratio, const std::string& output, const double lng, const double lat, const double height) {
	if (osgNode.valid()) {
		osgDB::makeDirectory(output);
		osg::ref_ptr<TileNode> threeDTilesNode;
		if (type == "quad") {
			QuadTreeBuilder qtb(osgNode, max, 8, ratio);
			threeDTilesNode = qtb.rootTreeNode;
		}
		else {
			OctreeBuilder octb(osgNode, max, 8, ratio);
			threeDTilesNode = octb.rootTreeNode;
		}
		getAllTreeNodesGeometricError(*threeDTilesNode.get());
		osg::EllipsoidModel ellipsoidModel;
		osg::Matrixd matrix;
		ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(lat), osg::DegreesToRadians(lng), height, matrix);
		std::vector<double> rootTransform;
		const double* ptr = matrix.ptr();
		for (unsigned i = 0; i < 16; ++i)
			rootTransform.push_back(*ptr++);
		outputTreeNode(*threeDTilesNode.get(), option, output, 0, rootTransform);
	}
}
#endif // !OSG_GIS_PLUGINS_OUTPUT3DTILES
