#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
#include "osgdb_gltf/Osg2Gltf.h"
#include "utils/FlattenTransformVisitor.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <osgDB/ConvertUTF>
#include <osgDB/FileUtils>
#include <utils/TextureOptimizer.h>
#include <osgUtil/Optimizer>
#include <3dtiles/optimizer/MeshSimplifier.h>
#include <3dtiles/optimizer/MeshOptimizer.h>
using namespace std;
void exportGltf(osg::ref_ptr<osg::Node> node, const std::string& filename,const std::string& path) {
    FlattenTransformVisitor ftv;
    node->accept(ftv);
    node->dirtyBound();
    node->computeBound();
    Osg2Gltf osgb2Gltf;
    node->accept(osgb2Gltf);

    const std::string output = R"(D:\nginx-1.22.1\html\gltf\)" + path + "\\" + osgDB::getStrippedName(filename) + ".gltf";
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


void convertOsgModel2Gltf(const std::string& filename,bool packTextures=true) {
    std::string s1 = osgDB::findDataFile(filename, new osgDB::Options);
    std::string s2 = osgDB::findDataFile(osgDB::convertStringFromUTF8toCurrentCodePage(filename), new osgDB::Options);

    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);
    //index mesh
    //osgUtil::Optimizer optimizer;
    //optimizer.optimize(node, osgUtil::Optimizer::INDEX_MESH);
    exportGltf(node, filename, "关闭纹理尺寸优化");

    std::string path = "启用纹理图集";
    if (!packTextures)
        path = "关闭纹理图集";
    const std::string textureExt = "jpg";
    const std::string textureCachePath = R"(D:\nginx-1.22.1\html\gltf\)" + path + "\\" + osgDB::getStrippedName(filename) + "\\" + textureExt;
    osgDB::makeDirectory(textureCachePath);
    TexturePackingVisitor tpv(4096, 4096, "." + textureExt, textureCachePath, packTextures);
    node->accept(tpv);
    tpv.packTextures();
    exportGltf(node, filename, path);
}

void convertOsgModel2Gltf2(const std::string& filename) {
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);

    //index mesh
    osgUtil::Optimizer optimizer;
    optimizer.optimize(node, osgUtil::Optimizer::INDEX_MESH);
    //flatten transform
    FlattenTransformVisitor ftv;
    node->accept(ftv);
    node->dirtyBound();
    node->computeBound();
    //mesh optimizer
    MeshSimplifierBase* meshOptimizer = new MeshSimplifier;
    MeshOptimizer mov(meshOptimizer, 1.0);
    node->accept(mov);
    const std::string textureExt = "ktx2";
    const std::string textureCachePath = R"(D:\nginx-1.22.1\html\gltf\)" + osgDB::getStrippedName(filename) + "\\" + textureExt;
    osgDB::makeDirectory(textureCachePath);
    TexturePackingVisitor tpv(4096, 4096, "." + textureExt, textureCachePath, false);
    node->accept(tpv);
    tpv.packTextures();
    exportGltf(node, filename, "");
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
    //const std::string filename = R"(E:\Data\data\龙翔桥站厅.fbx)";
    //osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);
    //TexturePackingVisitor tpv(4096, 4096, ".jpg", "./test3");
    //node->accept(tpv);
    //tpv.packTextures();
    //exportGltf(node, R"(E:\Data\data\龙翔桥站厅.FBX)");

    convertOsgModel2Gltf(R"(E:\SDK\OpenSceneGraph-Data\cow.osg)", false);

    //convertOsgModel2Gltf(R"(E:\Code\2023\Other\data\龙翔桥站厅.fbx)");
    //convertOsgModel2Gltf(R"(E:\Code\2023\Other\data\卡拉电站.fbx)");
    //convertOsgModel2Gltf(R"(E:\Code\2023\Other\data\芜湖水厂总装单位M.fbx)");
    //convertOsgModel2Gltf(R"(E:\Code\2023\Other\data\龙翔桥站.fbx)");

    //convertOsgModel2Gltf(R"(E:\Code\2023\Other\data\龙翔桥站厅.fbx)", false);
    //convertOsgModel2Gltf(R"(E:\Code\2023\Other\data\卡拉电站.fbx)", false);
    //convertOsgModel2Gltf(R"(E:\Code\2023\Other\data\芜湖水厂总装单位M.fbx)", false);
    //convertOsgModel2Gltf(R"(E:\Code\2023\Other\data\龙翔桥站.fbx)", false);
    //convertOsgModel2Gltf(R"(E:\Data\data\jianzhu+tietu.fbx)");

    //convertOsgModel2Gltf2(R"(E:\Code\2023\Other\data\卡拉电站.fbx)");

    return 1;
}
