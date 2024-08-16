
#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
//#include "osgdb_gltf/compress/GltfDracoCompressor.h"//必须放在<windows.h>前面
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
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
#include <osgDB/WriteFile>
#include <future>
#include "utils/Simplifier.h"
using namespace std;
using namespace osgGISPlugins;
//const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.27.0\html\test\gltf\)";
//const std::string INPUT_BASE_PATH = R"(E:\Data\data\)";

const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.22.1\html\gltf\)";
const std::string INPUT_BASE_PATH = R"(E:\Code\2023\Other\data\)";
/// <summary>
/// 导出gltf/glb/b3dm
/// </summary>
/// <param name="filename">原始三维模型文件名</param>
/// <param name="ext">输出文件后缀名，不带.</param>
/// <param name="options">输出文件选项</param>
/// <param name="textureExt">纹理格式，如果为空值则不启用纹理图集</param>
/// <param name="ratio">简化比例</param>
/// <param name="bFlatternTransform">是否展开变换矩阵</param>
void exportGltfWithOptions(const std::string& filename,const std::string ext,const osg::ref_ptr<osgDB::Options> options,const std::string textureExt,const float ratio,const bool bFlatternTransform)
{
	std::string lowerExtStr = ext;
	std::transform(lowerExtStr.begin(), lowerExtStr.end(), lowerExtStr.begin(),
		[](unsigned char c) { return std::tolower(c); });
	if (lowerExtStr != "gltf" && lowerExtStr != "glb" && lowerExtStr != "b3dm")
		return;
	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(INPUT_BASE_PATH + filename +R"(.fbx)");
	//1
	//if (bFlatternTransform) {
	//	FlattenTransformVisitor ftv;
	//	node->accept(ftv);
	//}
	//2
	osgUtil::Optimizer optimizer;
	osg::setNotifyLevel(osg::INFO);
	//optimizer.optimize(node, osgUtil::Optimizer::INDEX_MESH|osgUtil::Optimizer::STATIC_OBJECT_DETECTION|osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS);
	optimizer.optimize(node, osgUtil::Optimizer::STATIC_OBJECT_DETECTION| osgUtil::Optimizer::OPTIMIZE_TEXTURE_SETTINGS | 
		osgUtil::Optimizer::SHARE_DUPLICATE_STATE | osgUtil::Optimizer::COPY_SHARED_NODES | osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS);
	//3
	MeshSimplifierBase* meshOptimizer = new MeshSimplifier;
	MeshOptimizer mov(meshOptimizer, ratio);
	node->accept(mov);
	if (!textureExt.empty())
	{
		const std::string textureCachePath = OUTPUT_BASE_PATH + filename + "\\" + textureExt;
		osgDB::makeDirectory(textureCachePath);
		//4
		TexturePackingVisitor tpv(4096, 4096, "." + textureExt, textureCachePath, true);
		node->accept(tpv);
		tpv.packTextures();
	}
	//5
	if (lowerExtStr == "b3dm")
	{
		BatchIdVisitor biv;
		node->accept(biv);
	}
	osgDB::writeNodeFile(*node.get(), OUTPUT_BASE_PATH + filename + R"(.)" + lowerExtStr, options.get());
}

void testSimplifier(const std::string& filename)
{
	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(INPUT_BASE_PATH + filename + R"(.fbx)");
	Simplifier simplifier(0.5);
	node->accept(simplifier);
	osgDB::writeNodeFile(*node.get(), OUTPUT_BASE_PATH + filename + R"(_simplify_05.fbx)");
}

int main() {
	osgDB::Registry* instance = osgDB::Registry::instance();
	instance->addFileExtensionAlias("glb", "gltf");//插件注册别名
	instance->addFileExtensionAlias("b3dm", "gltf");//插件注册别名
	instance->addFileExtensionAlias("ktx2", "ktx");//插件注册别名

	//OSG_NOTICE << instance->createLibraryNameForExtension("glb") << std::endl;
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#else
	setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
	osg::ref_ptr<osgDB::Options> options = new osgDB::Options;

	//options->setOptionString("eb pp ct=draco");
	//exportGltfWithOptions(R"(广州塔)", "gltf", options, "jpg", 0.5, false);
	//OSG_NOTICE << R"(广州塔处理完毕)" << std::endl;

	//options->setOptionString("eb pp ct=meshopt");
    //exportGltfWithOptions(R"(卡拉电站)", "gltf", options, "webp", 0.5, false);
	//OSG_NOTICE << R"(卡拉电站处理完毕)" << std::endl;

	//options->setOptionString("eb pp quantize ct=meshopt");
	//exportGltfWithOptions(R"(龙翔桥站)", "gltf", options, "ktx2", 0.5, true);
	//OSG_NOTICE << R"(龙翔桥站处理完毕)" << std::endl;

	//options->setOptionString("eb quantize ct=meshopt");
	//exportGltfWithOptions(R"(龙翔桥站厅)", "b3dm", options, "jpg", 0.5, true);
	//OSG_NOTICE << R"(龙翔桥站厅处理完毕)" << std::endl;

	//options->setOptionString("eb pp ct=meshopt");
	//exportGltfWithOptions(R"(芜湖水厂总装单位M)", "gltf", options, "jpg", 0.5, false);
	//OSG_NOTICE << R"(芜湖水厂总装单位M处理完毕)" << std::endl;

	testSimplifier(R"(芜湖水厂总装单位M)");
	return 1;
}
