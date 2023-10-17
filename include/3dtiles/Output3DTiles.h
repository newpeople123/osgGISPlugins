#ifndef OSG_GIS_PLUGINS_OUTPUT3DTILES
#define OSG_GIS_PLUGINS_OUTPUT3DTILES 1
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "QuadTreeBuilder.h"
#include "OctreeBuilder.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include <tinygltf/tiny_gltf.h>
#include <osg/CoordinateSystemNode>
//#include <future>
double getGeometricError(const TreeNode& node) {
	double geometricError = 0.0;
	for (unsigned int i = 0; i < node.currentNodes->getNumChildren(); ++i) {
		osg::ComputeBoundsVisitor computeBoundsVisitor;
		node.currentNodes->getChild(i)->accept(computeBoundsVisitor);
		osg::BoundingBox boundingBox = computeBoundsVisitor.getBoundingBox();
		const double diagonalLength = (boundingBox._max - boundingBox._min).length();
		geometricError = std::max(geometricError, diagonalLength);
	}
	return geometricError;
}
void getAllTreeNodesGeometricError(TreeNode& node) {
	for (unsigned int i = 0; i < node.children->getNumChildren(); ++i) {
		osg::ref_ptr<TreeNode> child = dynamic_cast<TreeNode*>(node.children->getChild(i));
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

void outputTreeNode(const TreeNode& node, const osg::ref_ptr<osgDB::Options>& option, const std::string& output, int level, std::vector<double> transform = std::vector<double>()) {

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
	tileset["root"]["refine"] = node.children->getNumChildren() == 0 ? "REPLACE" : "ADD";
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
				osg::ref_ptr<TreeNode> child = dynamic_cast<TreeNode*>(node.children->getChild(i));
				json root;
				for (const double& i : child->box) {
					root["boundingVolume"]["box"].push_back(i);
				}
				const std::string uri = std::to_string(child->level - 1) + "/" + "L" + std::to_string(child->level - 1) + "_" + std::to_string(child->x) + "_" + std::to_string(child->y) + "_" + std::to_string(child->z) + ".json";
				root["content"]["uri"] = uri;
				root["geometricError"] = child->geometricError == 0 ? getGeometricError(*child) : child->geometricError;
				root["refine"] = child->children->getNumChildren() == 0 ? "REPLACE" : "ADD";
				tileset["root"]["children"].push_back(root);
				maxGeometricError = maxGeometricError > root["geometricError"] ? maxGeometricError : root["geometricError"];
			}
			tileset["geometricError"] = maxGeometricError;
			tileset["root"]["geometricError"] = tileset["geometricError"];
		}
		else {
			if (node.currentNodes->getNumChildren()) {
				tileset["geometricError"] = getGeometricError(node);
				tileset["root"]["geometricError"] = tileset["geometricError"];
				b3dmPath = output + "/root.b3dm";
			}
		}
	}
	else if (node.children->getNumChildren() == 0) {
		tileset["root"]["content"]["uri"] = "L" + std::to_string(node.level - 1) + "_" + std::to_string(node.x) + "_" + std::to_string(node.y) + "_" + std::to_string(node.z) + ".b3dm";
		tileset["geometricError"] = 0.0;
		tileset["root"]["geometricError"] = tileset["geometricError"];
	}
	else {
		if (node.currentNodes->getNumChildren()) {
			tileset["root"]["content"]["uri"] = "L" + std::to_string(node.level - 1) + "_" + std::to_string(node.x) + "_" + std::to_string(node.y) + "_" + std::to_string(node.z) + ".b3dm";
		}
		double maxGeometricError = 0;
		for (unsigned int i = 0; i < node.children->getNumChildren(); ++i) {
			osg::ref_ptr<TreeNode> child = dynamic_cast<TreeNode*>(node.children->getChild(i));
			json root;
			for (const double& i : child->box) {
				root["boundingVolume"]["box"].push_back(i);
			}
			root["content"]["uri"] = "../" + std::to_string(child->level - 1) + "/" + "L" + std::to_string(child->level - 1) + "_" + std::to_string(child->x) + "_" + std::to_string(child->y) + "_" + std::to_string(child->z) + ".json";

			root["geometricError"] = child->geometricError == 0 ? getGeometricError(*child) : child->geometricError;
			root["refine"] = child->children->getNumChildren() == 0 ? "REPLACE" : "ADD";
			tileset["root"]["children"].push_back(root);
			maxGeometricError = maxGeometricError > root["geometricError"] ? maxGeometricError : root["geometricError"];
		}
		tileset["geometricError"] = maxGeometricError;
		tileset["root"]["geometricError"] = tileset["geometricError"];
	}

	std::ofstream tilesetFile(tilesetPath);
	if (tilesetFile.is_open()) {
		tilesetFile << tileset.dump();
		tilesetFile.close();

		if (b3dmPath != "" && node.currentNodes->getNumChildren()) {
			osg::ref_ptr<osg::Node> outputNode = node.currentNodes->asNode();
			osgDB::writeNodeFile(*outputNode.get(), b3dmPath, option);
		}
	}
	std::vector<std::future<void>> futures;
	for (unsigned int i = 0; i < node.children->getNumChildren(); ++i) {
		osg::ref_ptr<TreeNode> child = dynamic_cast<TreeNode*>(node.children->getChild(i));
		futures.push_back(std::async(std::launch::async, outputTreeNode, *child.get(), option, output, level + 1, std::vector<double>()));
		//outputTreeNode(*child.get(), option, output, level + 1);

	}
	for (auto& future : futures) {
		future.get();
	}
}
void OsgNodeTo3DTiles(const osg::ref_ptr<osg::Node> osgNode, const osg::ref_ptr<osgDB::Options>& option,const std::string& type,const double max,const double ratio, const std::string& output, const double lng, const double lat, const double height) {
	if (osgNode.valid()) {
		osgDB::makeDirectory(output);
		osg::ref_ptr<TreeNode> threeDTilesNode;
		if (type == "quad") {
			QuadTreeBuilder qtb(osgNode,max,8,ratio);
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
