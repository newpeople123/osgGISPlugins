#include "3dtiles/optimizer/texture/TexturePacker.h"

#include <osg/ArgumentParser>
#include <iostream>
#include <osgDB/ConvertUTF>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <osgViewer/Viewer>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
    osg::ArgumentParser arguments(&argc, argv);
    arguments.getApplicationUsage()->setDescription(arguments.getApplicationName() + "this tool is used to convert and simplify 3D model.");
    arguments.getApplicationUsage()->addCommandLineOption("-i <file>", "input 3D model file full path");
    arguments.getApplicationUsage()->addCommandLineOption("-o <file>", "output simplified 3D model file full path");
    arguments.getApplicationUsage()->addCommandLineOption("-ratio <number>", "Simplified ratio of intermediate nodes.default is 0.5.");
    arguments.getApplicationUsage()->addCommandLineOption("-h or --help", "Display command line parameters");
    if (arguments.read("-h") || arguments.read("--help"))
    {
        arguments.getApplicationUsage()->write(std::cout);
        return 1;
    }

    std::string input = "", output = "";
    while (arguments.read("-i", input));
    while (arguments.read("-o", output));

    //if (input.empty()) {
    //    std::cout << "input file can not be null!" << '\n';
    //    arguments.getApplicationUsage()->write(std::cout);
    //    return 0;
    //}

    //if (output.empty()) {
    //    std::cout << "output file can not be null!" << '\n';
    //    arguments.getApplicationUsage()->write(std::cout);
    //    return 0;
    //}
#ifndef NDEBUG
#else
    input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
    output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG
    const std::string basePath = R"(E:\Code\2023\Other\data\1芜湖水厂总装.fbm\)";
    std::vector<std::string> imgPaths = {
        basePath+R"(01 - Default_Base_Color.jpg)",
        basePath + R"(城南污水处理厂地形-4.jpg)",
        basePath + R"(440305A003GHJZA001T008.png)",
        basePath + R"(出水泵二期颜色666.jpg)",
        basePath + R"(MXGMB003 - 副本.png)",
        basePath + R"(办公室单开门.jpg)",
        basePath + R"(办公室双开门.png)",

    };
    std::vector<osg::ref_ptr<osg::Image>> images;
    std::map<size_t, size_t> geometryIdMap;
    std::string imageName; size_t numImages = 0;
    for (auto str : imgPaths) {
        osg::ref_ptr<osg::Image> img = osgDB::readImageFile(str);
        images.push_back(img);
    }

    osg::ref_ptr<TexturePacker> packer = new TexturePacker(4096, 4096);
    for (size_t i = 0; i < images.size(); ++i)
    {
        geometryIdMap[i] = packer->addElement(images[i].get());
    }
    osg::ref_ptr<osg::Image> atlas = packer->pack(numImages, true);
    osgDB::writeImageFile(*atlas.get(), "./test.png");
    return 1;
}
