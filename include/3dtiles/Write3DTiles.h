#pragma once
#ifndef OSG_GIS_PLUGINS_WRITE3DTILES_H
#define OSG_GIS_PLUGINS_WRITE3DTILES_H 1
#include <future>
#include <queue>
#include <nlohmann/json.hpp>
#include "TileNode.h"
#include <osgDB/ConvertUTF>
#include <osgDB/FileUtils>
#include <osgDB/Options>
#include <osgUtil/Simplifier>
#include <osgViewer/Viewer>
#include "GeometricError.h"
#include <osgDB/WriteFile>
#include "utils/SimplifyIndices.h"

using namespace nlohmann;

std::vector<std::string> filenames;

inline double GetFileSize(const std::string& filename) {
	const std::string realFilename = osgDB::convertStringFromUTF8toCurrentCodePage(filename);
	const auto it = std::find(filenames.begin(), filenames.end(), realFilename);

	if (it != filenames.end()) {
		return 0;
	}
	filenames.push_back(realFilename);
	std::ifstream file(realFilename, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		// 处理文件无法打开的情况
		std::cerr << "Error opening file: " << realFilename << std::endl;
		return -1;
	}

	const std::streampos fileSize = file.tellg(); // 获取当前位置，即文件大小
	file.close();

	const double fileSizeB = fileSize;
	const double fileSizeKB = fileSizeB / 1024.0;
	const double fileSizeMB = fileSizeKB / 1024.0;
	return fileSizeMB;
}
class ComputeTextureSizeVisitor :public osg::NodeVisitor
{
public:
	ComputeTextureSizeVisitor() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {

	}
	void apply(osg::Node& node) override
	{
		osg::StateSet* ss = node.getStateSet();
		if (ss) {
			apply(*ss);
		}
		traverse(node);
	}
	void apply(osg::StateSet& stateset) {
		for (unsigned int i = 0; i < stateset.getTextureAttributeList().size(); ++i)
		{
			osg::StateAttribute* sa = stateset.getTextureAttribute(i, osg::StateAttribute::TEXTURE);
			osg::Texture* texture = dynamic_cast<osg::Texture*>(sa);
			if (texture)
			{
				apply(*texture);
			}
		}
	}
	void apply(osg::Texture& texture) {
		for (unsigned int i = 0; i < texture.getNumImages(); i++) {
			osg::ref_ptr<osg::Image> img = texture.getImage(i);
			size += GetFileSize(img->getFileName());
		}
	}
	int size = 0;

};

inline void ComputeBoundingVolume(const osg::ref_ptr<TileNode>& treeNode) {
	if (treeNode->nodes->getNumChildren()) {
		osg::ComputeBoundsVisitor computeBoundsVisitor;
		treeNode->nodes->accept(computeBoundsVisitor);
		osg::BoundingBox boundingBox = computeBoundsVisitor.getBoundingBox();
		osg::Matrixd mat = osg::Matrixd::rotate(osg::inDegrees(90.0f), 1.0f, 0.0f, 0.0f);
		const osg::Vec3f size = boundingBox._max * mat - boundingBox._min * mat;
		const osg::Vec3f cesiumBoxCenter = boundingBox.center() * mat;

		treeNode->box.push_back(cesiumBoxCenter.x());
		treeNode->box.push_back(cesiumBoxCenter.y());
		treeNode->box.push_back(cesiumBoxCenter.z());

		treeNode->box.push_back(size.x() / 2);
		treeNode->box.push_back(0);
		treeNode->box.push_back(0);

		treeNode->box.push_back(0);
		treeNode->box.push_back(size.y() / 2);
		treeNode->box.push_back(0);

		treeNode->box.push_back(0);
		treeNode->box.push_back(0);
		treeNode->box.push_back(size.z() / 2);
	}
	for (unsigned int i = 0; i < treeNode->children->getNumChildren(); ++i) {
		ComputeBoundingVolume(dynamic_cast<TileNode*>(treeNode->children->getChild(i)));
	}
}

inline void ConvertTreeNode2Levels(const osg::ref_ptr<TileNode>& rootTreeNode, std::vector<std::vector<osg::ref_ptr<TileNode>>>& levels) {
	std::queue<osg::ref_ptr<TileNode>> q;
	q.push(rootTreeNode);
	while (!q.empty()) {
		const int size = q.size();
		std::vector<osg::ref_ptr<TileNode>> level;
		for (int i = 0; i < size; i++) {
			osg::ref_ptr<TileNode> node = q.front();
			q.pop();
			level.push_back(node);

			for (unsigned int j = 0; j < node->children->getNumChildren(); ++j) {
				osg::ref_ptr<TileNode> child = dynamic_cast<TileNode*>(node->children->getChild(j));
				q.push(child);
			}
		}
		levels.push_back(level);
	}
}

inline void WriteB3DM(const osg::ref_ptr<TileNode>& tileNode, const std::string& output, const osg::ref_ptr<osgDB::Options>& option, const double simpleRatio)
{
	if (tileNode.valid()) {
		std::string b3dmPath;
		if (tileNode->level != 0)
		{
			const std::string childOutput = output + "/" + std::to_string(tileNode->level - 1);
			osgDB::makeDirectory(childOutput);
			b3dmPath = childOutput + "/" + "L" + std::to_string(tileNode->level - 1) + "_" + std::to_string(tileNode->x) + "_" + std::to_string(tileNode->y) + "_" + std::to_string(tileNode->z) + ".b3dm";
		}
		else
		{
			if (tileNode->children->getNumChildren() == 0)
				b3dmPath = output + "/root.b3dm";
		}
		if (!b3dmPath.empty() && tileNode->currentNodes != nullptr && tileNode->currentNodes->getNumChildren()) {
			SimplifyGeometryNodeVisitor sgnv(simpleRatio);
			tileNode->currentNodes->accept(sgnv);
			const int pixelSize = tileNode->singleTextureMaxSize;
			option->setOptionString(option->getOptionString() + " textureMaxSize=" + std::to_string(pixelSize));
			osgDB::writeNodeFile(*tileNode->currentNodes, b3dmPath, option);
		}
	}
}

inline void BuildHlodAndWriteB3DM(const osg::ref_ptr<TileNode>& rootTreeNode, const std::string& output, const osg::ref_ptr<osgDB::Options>& option, const double simpleRatio, const bool enableMultiThreading) {
	std::vector<std::vector<osg::ref_ptr<TileNode>>> levels;
	ConvertTreeNode2Levels(rootTreeNode, levels);
	auto func = [&levels, &simpleRatio](int i) {
		const std::vector<osg::ref_ptr<TileNode>> level = levels.at(i);
		for (const osg::ref_ptr<TileNode>& treeNode : level) {
			if (treeNode->currentNodes->getNumChildren()) {
				const osg::ref_ptr<osg::Node> lodModel = treeNode->currentNodes.get();
				//osgUtil::Simplifier simplifier;
				//simplifier.setSampleRatio(simpleRatio);
				//lodModel->accept(simplifier);
				SimplifyGeometryNodeVisitor sgnv(simpleRatio);
				lodModel->accept(sgnv);
			}
		}
		};
	bool isLoadB3dmDll = false;
	for (int i = levels.size() - 1; i > -1; --i) {
		std::vector<osg::ref_ptr<TileNode>> level = levels.at(i);
		for (osg::ref_ptr<TileNode>& treeNode : level) {

			if (treeNode->currentNodes->getNumChildren() == 0) {
				int childrenCount = 0;
				for (unsigned int j = 0; j < treeNode->children->getNumChildren(); ++j) {
					const osg::ref_ptr<TileNode> childNode = dynamic_cast<TileNode*>(treeNode->children->getChild(j));
					if (childNode->currentNodes != nullptr) {
						const osg::BoundingBox childBoundingBox = GetBoundingBox(childNode->currentNodes);

						for (unsigned int l = 0; l < childNode->currentNodes->getNumChildren(); ++l) {
							childrenCount++;
							osg::ref_ptr<osg::Node> childNodeCurrentNode = childNode->currentNodes->getChild(l);
							const osg::BoundingBox grandChildBoundingBox = GetBoundingBox(childNodeCurrentNode);
							if (childNodeCurrentNode.valid()) {
								if ((grandChildBoundingBox._max - grandChildBoundingBox._min).length() * 10 >= (childBoundingBox._max - childBoundingBox._min).length()) {
									treeNode->currentNodes->addChild(childNodeCurrentNode);
								}
							}
						}
						ComputeTextureSizeVisitor ctsv;
						treeNode->currentNodes->accept(ctsv);
						treeNode->size = ctsv.size;
					}
				}
				if (treeNode->currentNodes->getNumChildren() == childrenCount && childrenCount != 0) {
					treeNode->currentNodes = nullptr;
				}
			}
			GetTileNodeGeometricErrors(treeNode);
			if (treeNode->currentNodes != nullptr && treeNode->currentNodes->getNumChildren()) {
				if (treeNode->lowerGeometricError != 0.0) {
					const double distance = GetDistanceByGeometricError(treeNode->lowerGeometricError);
					treeNode->singleTextureMaxSize = GetPixelSize(distance, treeNode->nodes->getBound().radius());
					//treeNode->singleTextureMaxSize = 4096;
				}
				else
				{
					treeNode->singleTextureMaxSize = 4096;
				}
				//if(treeNode->singleTextureMaxSize == 4096)
				//{
				//	treeNode->refine = "ADD";
				//	const unsigned int currentNodeNum = treeNode->currentNodes->getNumChildren();
				//	const unsigned int childrenNum = treeNode->children->getNumChildren();
				//	for (unsigned int j = 0; j < childrenNum; ++j) {
				//		const osg::ref_ptr<TileNode> childNode = dynamic_cast<TileNode*>(treeNode->children->getChild(j));
				//		for (unsigned int k = 0; k < currentNodeNum; k++)
				//		{
				//			childNode->currentNodes->removeChild(treeNode->currentNodes->getChild(k));
				//		}
				//	}
				//}
			}
			if (!enableMultiThreading) {
				osg::ref_ptr<osgDB::Options> copyOption = new osgDB::Options(option->getOptionString());
				WriteB3DM(treeNode, output, copyOption, simpleRatio);
			}
		}
		if (enableMultiThreading) {
			std::vector<std::future<void>> futures;
			for (const osg::ref_ptr<TileNode>& treeNode : level) {
				osg::ref_ptr<osgDB::Options> copyOption = new osgDB::Options(option->getOptionString());
				if (!isLoadB3dmDll)
				{
					WriteB3DM(treeNode, output, copyOption, simpleRatio);
					isLoadB3dmDll = true;
				}
				else
				{
					futures.push_back(std::async(std::launch::async, WriteB3DM, treeNode, output, copyOption, simpleRatio));
				}
			}
			for (auto& future : futures) {
				future.get();
			}
		}
	}
	//for (int i = levels.size() - 1; i > -1; --i) {
	//	std::vector<osg::ref_ptr<TileNode>> level = levels.at(i);
	//	std::vector<std::future<void>> futures;
	//	for (osg::ref_ptr<TileNode>& treeNode : level) {
	//		GetTileNodeGeometricErrors(treeNode);
	//		osg::ref_ptr<osgDB::Options> copyOption = new osgDB::Options(option->getOptionString());
	//		if (!isLoadB3dmDll)
	//		{
	//			WriteB3DM(treeNode, output, copyOption);
	//			isLoadB3dmDll = true;
	//		}
	//		else
	//		{
	//			futures.push_back(std::async(std::launch::async, WriteB3DM, treeNode, output, copyOption));
	//		}
	//	}
	//	for (auto& future : futures) {
	//		future.get();
	//	}
	//}
	ComputeBoundingVolume(rootTreeNode);

}

inline void WriteTileset(const osg::ref_ptr<TileNode>& node, const std::string& output, const std::vector<double>&
	transform = std::vector<double>()) {

	std::string tilesetPath;
	if (node->level != 0) {
		std::string childOutput;
		childOutput = output + "/" + std::to_string(node->level - 1);
		osgDB::makeDirectory(childOutput);
		tilesetPath = childOutput + "/L" + std::to_string(node->level - 1) + "_" + std::to_string(node->x) + "_" + std::to_string(node->y) + "_" + std::to_string(node->z) + ".json";
	}
	else {
		tilesetPath = output + "/tileset.json";
	}

	json tileset;
	tileset["asset"]["version"] = "1.0";
	tileset["asset"]["tilesetVersion"] = "1.0.0";
	tileset["root"]["refine"] = node->refine;
	for (const double& i : node->box) {
		tileset["root"]["boundingVolume"]["box"].push_back(i);
	}
	for (double i : transform) {
		tileset["root"]["transform"].push_back(i);
	}

	if (node->level == 0) {
		if (node->children->getNumChildren()) {
			for (unsigned int i = 0; i < node->children->getNumChildren(); ++i) {
				osg::ref_ptr<TileNode> child = dynamic_cast<TileNode*>(node->children->getChild(i));
				json root;
				for (const double& i : child->box) {
					root["boundingVolume"]["box"].push_back(i);
				}
				const std::string uri = std::to_string(child->level - 1) + "/" + "L" + std::to_string(child->level - 1) + "_" + std::to_string(child->x) + "_" + std::to_string(child->y) + "_" + std::to_string(child->z) + ".json";
				root["content"]["uri"] = uri;
				root["geometricError"] = child->lowerGeometricError;
				root["refine"] = child->refine;
				tileset["root"]["children"].push_back(root);
			}
			tileset["geometricError"] = node->upperGeometricError;
			tileset["root"]["geometricError"] = node->lowerGeometricError;
		}
		else {
			if (node->currentNodes != nullptr && node->currentNodes->getNumChildren()) {
				tileset["geometricError"] = node->upperGeometricError;
				tileset["root"]["geometricError"] = node->lowerGeometricError;
			}
		}
	}
	else if (node->children->getNumChildren() == 0) {
		tileset["root"]["content"]["uri"] = "L" + std::to_string(node->level - 1) + "_" + std::to_string(node->x) + "_" + std::to_string(node->y) + "_" + std::to_string(node->z) + ".b3dm";
		tileset["geometricError"] = node->upperGeometricError;
		tileset["refine"] = node->refine;
		tileset["root"]["geometricError"] = node->lowerGeometricError;
	}
	else {
		if (node->currentNodes != nullptr && node->currentNodes->getNumChildren()) {
			tileset["root"]["content"]["uri"] = "L" + std::to_string(node->level - 1) + "_" + std::to_string(node->x) + "_" + std::to_string(node->y) + "_" + std::to_string(node->z) + ".b3dm";
		}
		for (unsigned int i = 0; i < node->children->getNumChildren(); ++i) {
			osg::ref_ptr<TileNode> child = dynamic_cast<TileNode*>(node->children->getChild(i));
			json root;
			for (const double& i : child->box) {
				root["boundingVolume"]["box"].push_back(i);
			}
			root["content"]["uri"] = "../" + std::to_string(child->level - 1) + "/" + "L" + std::to_string(child->level - 1) + "_" + std::to_string(child->x) + "_" + std::to_string(child->y) + "_" + std::to_string(child->z) + ".json";

			root["geometricError"] = child->lowerGeometricError;
			root["refine"] = child->refine;
			tileset["root"]["children"].push_back(root);
		}
		tileset["root"]["geometricError"] = node->lowerGeometricError;
		tileset["geometricError"] = node->upperGeometricError;
	}

	std::ofstream tilesetFile(tilesetPath);
	if (tilesetFile.is_open()) {
		tilesetFile << tileset.dump();
		tilesetFile.close();
	}
	std::vector<std::future<void>> futures;
	for (unsigned int i = 0; i < node->children->getNumChildren(); ++i) {
		osg::ref_ptr<TileNode> child = dynamic_cast<TileNode*>(node->children->getChild(i));
		futures.push_back(std::async(std::launch::async, WriteTileset, child, output, std::vector<double>()));
		//WriteTileset(child, output);

	}
	for (auto& future : futures) {
		future.get();
	}
}

inline void Write3DTiles(const osg::ref_ptr<TileNode>& node, const osg::ref_ptr<osgDB::Options>& option, const std::string& output, const double simpleRatio, const bool enableMultiThreading, std::vector<double> transform = std::vector<double>()) {
	BuildHlodAndWriteB3DM(node, output, option, simpleRatio, enableMultiThreading);
	WriteTileset(node, output, transform);
}
#endif // !OSG_GIS_PLUGINS_WRITE3DTILES_H
