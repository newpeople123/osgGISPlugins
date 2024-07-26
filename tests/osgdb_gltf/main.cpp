
#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
#include "osgdb_gltf/Osg2Gltf.h"
#include "osgdb_gltf/compress/GltfDracoCompressor.h"//必须放在<windows.h>前面
#include "osgdb_gltf/compress/GltfMeshOptCompressor.h"
#include "osgdb_gltf/compress/GltfMeshQuantizeCompressor.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <osgDB/ConvertUTF>
#include <osgDB/FileUtils>
#include <utils/TextureOptimizer.h>
#include <osgUtil/Optimizer>
#include <3dtiles/optimizer/MeshSimplifier.h>
#include <3dtiles/optimizer/MeshOptimizer.h>
#include <osgViewer/Viewer>
#include <utils/FlattenTransformVisitor.h>
#include <osgUtil/Optimizer>

using namespace std;
//const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.27.0\html\test\gltf\)";
//const std::string INPUT_BASE_PATH = R"(E:\Data\data\)";

const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.22.1\html\gltf\)";
const std::string INPUT_BASE_PATH = R"(E:\Code\2023\Other\data\)";



/// <summary>
/// 导出gltf
/// </summary>
/// <param name="node"></param>
/// <param name="filename"></param>
/// <param name="path"></param>
void exportGltf(osg::ref_ptr<osg::Node> node, const std::string& filename, const std::string& path) {
	osgUtil::Optimizer optimizer;
	optimizer.optimize(node, osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS);
	Osg2Gltf osgb2Gltf;
	node->accept(osgb2Gltf);

	const std::string output = OUTPUT_BASE_PATH + path + "\\" + osgDB::getStrippedName(filename) + ".gltf";
	tinygltf::TinyGLTF writer;
	tinygltf::Model gltfModel = osgb2Gltf.getGltfModel();
	bool isSuccess = writer.WriteGltfSceneToFile(
		&gltfModel,
		output,
		false,           // embedImages
		true,           // embedBuffers
		true,           // prettyPrint
		false);
}
/// <summary>
/// 测试纹理图集
/// </summary>
/// <param name="filename"></param>
/// <param name="packTextures"></param>
void convertOsgModel2Gltf(const std::string& filename, bool packTextures = true) {
	std::string s1 = osgDB::findDataFile(filename, new osgDB::Options);
	std::string s2 = osgDB::findDataFile(osgDB::convertStringFromUTF8toCurrentCodePage(filename), new osgDB::Options);

	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);
	FlattenTransformVisitor ftv;
	node->accept(ftv);
	//index mesh
	osgUtil::Optimizer optimizer;
	optimizer.optimize(node, osgUtil::Optimizer::INDEX_MESH);
	exportGltf(node, filename, "关闭纹理尺寸优化");

	std::string path = "启用纹理图集";
	if (!packTextures)
		path = "关闭纹理图集";
	const std::string textureExt = "jpg";
	const std::string textureCachePath = OUTPUT_BASE_PATH + path + "\\" + osgDB::getStrippedName(filename) + "\\" + textureExt;
	osgDB::makeDirectory(textureCachePath);
	TexturePackingVisitor tpv(4096, 4096, "." + textureExt, textureCachePath, packTextures);
	node->accept(tpv);
	tpv.packTextures();
	exportGltf(node, filename, path);
}
/// <summary>
/// 测试网格优化，主要是简化、vertexCacheOptimize、overdrawOptimize、vertexFetchOptimize等
/// </summary>
/// <param name="filename"></param>
void convertOsgModel2Gltf2(const std::string& filename) {
	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);

	//index mesh
	osgUtil::Optimizer optimizer;
	optimizer.optimize(node, osgUtil::Optimizer::INDEX_MESH);
	//mesh optimizer
	MeshSimplifierBase* meshOptimizer = new MeshSimplifier;
	MeshOptimizer mov(meshOptimizer, 1.0);
	//node->accept(mov);
	const std::string textureExt = "jpg";
	const std::string textureCachePath = OUTPUT_BASE_PATH + osgDB::getStrippedName(filename) + "\\" + textureExt;
	osgDB::makeDirectory(textureCachePath);
	TexturePackingVisitor tpv(4096, 4096, "." + textureExt, textureCachePath, true);
	node->accept(tpv);
	tpv.packTextures();
	exportGltf(node, filename, "");
}
/// <summary>
/// 导出gltf，测试网格压缩、向量化
/// </summary>
/// <param name="node"></param>
/// <param name="filename"></param>
/// <param name="path"></param>
void exportGltf3(osg::ref_ptr<osg::Node> node, const std::string& filename, const std::string& path) {
	osgUtil::Optimizer optimizer;
	optimizer.optimize(node, osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS);
	Osg2Gltf osgb2Gltf;
	node->accept(osgb2Gltf);

	const std::string output = OUTPUT_BASE_PATH + path + "\\" + osgDB::getStrippedName(filename) + "_quantization.gltf";
	tinygltf::TinyGLTF writer;
	tinygltf::Model gltfModel = osgb2Gltf.getGltfModel();
	//GltfMeshQuantizeCompressor meshQuantize(gltfModel);
	//GltfDracoCompressor dracoCompressor(gltfModel);
	GltfMeshOptCompressor meshOptCompressor(gltfModel);
	bool isSuccess = writer.WriteGltfSceneToFile(
		&gltfModel,
		output,
		false,           // embedImages
		true,           // embedBuffers
		true,           // prettyPrint
		false);
}
/// <summary>
/// 测试网格压缩、向量化
/// </summary>
/// <param name="filename"></param>
void convertOsgModel2Gltf3(const std::string& filename) {
	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);
	//FlattenTransformVisitor ftv;
	//node->accept(ftv);
	//index mesh
	osgUtil::Optimizer optimizer;
	optimizer.optimize(node.get(), osgUtil::Optimizer::INDEX_MESH);
	//mesh optimizer
	MeshSimplifierBase* meshOptimizer = new MeshSimplifier;
	MeshOptimizer mov(meshOptimizer, 1.0);
	//node->accept(mov);
	const std::string textureExt = "jpg";
	const std::string textureCachePath = OUTPUT_BASE_PATH + osgDB::getStrippedName(filename) + "_quantization\\" + textureExt;
	osgDB::makeDirectory(textureCachePath);
	TexturePackingVisitor tpv(4096, 4096, "." + textureExt, textureCachePath, true);
	node->accept(tpv);
	tpv.packTextures();
	exportGltf3(node, filename, "");
}

int main() {
	osgDB::Registry* instance = osgDB::Registry::instance();
	instance->addFileExtensionAlias("glb", "gltf");//插件注册别名
	instance->addFileExtensionAlias("b3dm", "gltf");//插件注册别名
	instance->addFileExtensionAlias("ktx2", "ktx");//插件注册别名

	std::cout << instance->createLibraryNameForExtension("glb") << std::endl;
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#else
	setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
	//convertOsgModel2Gltf3(INPUT_BASE_PATH + R"(卡拉电站.fbx)");
	convertOsgModel2Gltf3(INPUT_BASE_PATH + R"(龙翔桥站.fbx)");
	convertOsgModel2Gltf3(INPUT_BASE_PATH + R"(龙翔桥站厅.fbx)");

	return 1;
}
