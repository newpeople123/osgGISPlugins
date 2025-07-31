#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osg/NodeVisitor>
#include <osg/ShapeDrawable>
#include <osgViewer/Viewer>
#ifdef _WIN32
#include <windows.h>
#endif
#include <utils/GltfOptimizer.h>
#include <osg/ComputeBoundsVisitor>
#include <osg/LineWidth>
#include <3dtiles/Tileset.h>
#include <3dtiles/hlod/QuadTreeBuilder.h>
#include <osg/MatrixTransform>
using namespace std;
using namespace osgGISPlugins;
//const std::string OUTPUT_BASE_PATH = R"(C:\Users\94764\Desktop\nginx-1.26.2\html\)";
//const std::string INPUT_BASE_PATH = R"(C:\baidunetdiskdownload\)";

const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.22.1\html\)";
const std::string INPUT_BASE_PATH = R"(E:\Code\2023\Other\data\)";


int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
    osgDB::Registry* instance = osgDB::Registry::instance();
    instance->addFileExtensionAlias("glb", "gltf");//插件注册别名
    instance->addFileExtensionAlias("b3dm", "gltf");//插件注册别名
    instance->addFileExtensionAlias("i3dm", "gltf");//插件注册别名
    instance->addFileExtensionAlias("ktx2", "ktx");//插件注册别名

    return 1;
}
