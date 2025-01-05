#include <osg/ArgumentParser>
#include <osgDB/Options>
#include <osgDB/ConvertUTF>
#include <osgDB/ReadFile>
#include <iostream>
#include <osgDB/FileUtils>
#include <osg/MatrixTransform>
#include <osgUtil/SmoothingVisitor>
#include <osg/PositionAttitudeTransform>
#include <proj.h>
#include <osg/DeleteHandler>
#include <osg/MatrixTransform>
#ifdef _WIN32
#include <windows.h>
#endif
#include "3dtiles/Tileset.h"
#include "3dtiles/hlod/QuadtreeBuilder.h"
#include "3dtiles/hlod/OctreeBuilder.h"
#include <osg/CoordinateSystemNode>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/FileNameUtils>

// 设置控台编码
void initConsole()
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#else
	setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
}

// 注册文件扩展名别名
void registerFileAliases() {
	osgDB::Registry* instance = osgDB::Registry::instance();
	instance->addFileExtensionAlias("glb", "gltf");
	instance->addFileExtensionAlias("b3dm", "gltf");
	instance->addFileExtensionAlias("i3dm", "gltf");
	instance->addFileExtensionAlias("ktx2", "ktx");
	instance->setReadFileCallback(new Utils::ProgressReportingFileReadCallback);
}

// 读取模型文件
osg::ref_ptr<osg::Node> readModelFile(const std::string& input) {
	osg::ref_ptr<osgDB::Options> readOptions = new osgDB::Options;
#ifndef NDEBUG
	readOptions->setOptionString("ShowProgress");
#else
	readOptions->setOptionString("TessellatePolygons ShowProgress");
#endif // !NDEBUG

	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(input, readOptions.get());

	if (!node) {
		OSG_FATAL << "Error: Cannot read 3D model file!" << '\n';
	}

	return node;
}

// 应用优化器
void applyOptimizer(osg::ref_ptr<osg::Node>& node) {
	//osgUtil::Optimizer optimizer;
	//optimizer.optimize(node.get(), osgUtil::Optimizer::INDEX_MESH);
	GltfOptimizer optimizer;
	const std::string path = R"(C:\Users\94764\Desktop\nginx-1.26.2\html\3dtiles\textures)";
	osgDB::makeDirectory(path);
	GltfOptimizer::GltfTextureOptimizationOptions textureOptions;
	textureOptions.cachePath = path;
	optimizer.setGltfTextureOptimizationOptions(textureOptions);
	optimizer.optimize(node.get(), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS | GltfOptimizer::MERGE_TRANSFORMS);
	osgDB::writeNodeFile(*node, R"(C:\Users\94764\Desktop\nginx-1.26.2\html\3dtiles\textures\test.glb)");
}


int main(int argc, char** argv)
{
	initConsole();

	registerFileAliases();

	
	std::string input = R"(C:\baidunetdiskdownload\芜湖水厂总装.fbx)";
	std::string output = R"(C:\Users\94764\Desktop\nginx-1.26.2\html\3dtiles\20240529卢沟桥分洪枢纽1)";
#ifndef NDEBUG
#else
	input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
	output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG

	osg::ref_ptr<osg::Node> node = readModelFile(input);
	applyOptimizer(node);
	return 1;
}
