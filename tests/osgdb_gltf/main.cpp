#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
#include "osgdb_gltf/Osgb2Gltf.h"
#include "utils/FlattenTransformVisitor.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <osgDB/ConvertUTF>
#include <osgDB/FileUtils>
#include <utils/TextureOptimizer.h>
using namespace std;
void exportGltf(osg::ref_ptr<osg::Node> node, const std::string& filename) {
    FlattenTransformVisitor ftv;
    node->accept(ftv);
    node->dirtyBound();
    node->computeBound();
    Osgb2Gltf osgb2Gltf;
    node->accept(osgb2Gltf);

    const std::string output = R"(D:\nginx-1.27.0\html\test\gltf\)" + osgDB::getStrippedName(filename) + ".gltf";
    tinygltf::TinyGLTF writer;
    std::sort(osgb2Gltf.model.extensionsRequired.begin(), osgb2Gltf.model.extensionsRequired.end());
    osgb2Gltf.model.extensionsRequired.erase(std::unique(osgb2Gltf.model.extensionsRequired.begin(), osgb2Gltf.model.extensionsRequired.end()), osgb2Gltf.model.extensionsRequired.end());
    std::sort(osgb2Gltf.model.extensionsUsed.begin(), osgb2Gltf.model.extensionsUsed.end());
    osgb2Gltf.model.extensionsUsed.erase(std::unique(osgb2Gltf.model.extensionsUsed.begin(), osgb2Gltf.model.extensionsUsed.end()), osgb2Gltf.model.extensionsUsed.end());
    bool isSuccess = writer.WriteGltfSceneToFile(
        &osgb2Gltf.model,
        output,
        false,           // embedImages
        true,           // embedBuffers
        true,           // prettyPrint
        false);
}


void convertOsgModel2Gltf(const std::string& filename) {
    std::string s1 = osgDB::findDataFile(filename, new osgDB::Options);
    std::string s2 = osgDB::findDataFile(osgDB::convertStringFromUTF8toCurrentCodePage(filename), new osgDB::Options);

    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);
    TexturePackingVisitor tpv(4096, 4096, ".jpg", "./test3");
    node->accept(tpv);
    tpv.packTextures();
    exportGltf(node, filename);
}


int main() {



    osgDB::Registry* instance = osgDB::Registry::instance();
    instance->addFileExtensionAlias("glb", "gltf");//插件注册别名
    //osgDB::readNodeFile(R"(D:\nginx-1.27.0\html\test\gltf\1.glb)");

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

    convertOsgModel2Gltf(R"(E:\Data\data\龙翔桥站厅.fbx)");
    //convertOsgModel2Gltf(R"(E:\Data\data\卡拉电站.fbx)");
    //convertOsgModel2Gltf(R"(E:\Data\data\芜湖水厂总装单位M.fbx)");
    //convertOsgModel2Gltf(R"(E:\Data\data\龙翔桥站.fbx)");
    //convertOsgModel2Gltf(R"(E:\Data\data\jianzhu+tietu.fbx)");

    return 1;
}
