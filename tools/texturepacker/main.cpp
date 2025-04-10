#ifndef OSG_GIS_PLUGINS_PATH_SPLIT_STRING

#ifdef _WIN32
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "\\"
#else
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "/"
#endif
#endif // !OSG_GIS_PLUGINS_PATH_SPLIT_STRING
#include "utils/TexturePacker.h"

#include <osg/ArgumentParser>
#include <iostream>
#include <osgDB/ConvertUTF>
#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
#include <osgDB/FileUtils>
using namespace osgGISPlugins;
using namespace nlohmann;
int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32

    osg::ArgumentParser arguments(&argc, argv);
    osg::ApplicationUsage* usage = arguments.getApplicationUsage();
    usage->setDescription(arguments.getApplicationName() + ",that is used to convert and simplify 3D model.");
    usage->setCommandLineUsage("texturepacker.exe -i C:\\input -o C:\\output\\atlas.png -width 2048 -height 2048");
    usage->addCommandLineOption("-i <files/folder>", "Input image files or folder path containing images.");
    usage->addCommandLineOption("-o <file>", "Output image file full path.");
    usage->addCommandLineOption("-width <number>", "Texture atlas width, must be a power of 2.");
    usage->addCommandLineOption("-height <number>", "Texture atlas height, must be a power of 2.");
    usage->addCommandLineOption("-h or --help", "Display command line parameters.");

    if (arguments.read("-h") || arguments.read("--help"))
    {
        usage->write(std::cout);
        return 100;
    }

    std::vector<std::string> inputFiles;
    std::string output = "";
    int width = 0, height = 0;

    // 读取输入文件或文件夹
    std::string input;
    while (arguments.read("-i", input)) {
#ifndef NDEBUG
#else
        input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
#endif // !NDEBUG
        if (osgDB::fileType(input) == osgDB::DIRECTORY) {
            osgDB::DirectoryContents files = osgDB::getDirectoryContents(input);
            for (const std::string& file : files) {
                std::string fullPath = osgDB::concatPaths(input, file);
                std::string extension = osgDB::convertToLowerCase(osgDB::getFileExtension(fullPath));
                if (extension == "jpg" || extension == "jpeg" || extension == "png" || extension == "ktx" || extension == "ktx2" || extension == "webp") {
                    inputFiles.push_back(fullPath);
                }
            }
        }
        else if (osgDB::fileType(input) == osgDB::REGULAR_FILE) {
            inputFiles.push_back(input);
        }
    }


    // 读取输出文件
    arguments.read("-o", output);
#ifndef NDEBUG
#else
    output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG
    // 读取纹理图集的宽度和高度
    arguments.read("-width", width);
    arguments.read("-height", height);

    // 检查输入文件是否为空
    if (inputFiles.empty()) {
        std::cerr << "Input files cannot be empty!" << '\n';
        usage->write(std::cout);
        return 1;
    }

    // 检查输出路径是否为空
    if (output.empty()) {
        std::cerr << "Output file cannot be empty!" << '\n';
        usage->write(std::cout);
        return 2;
    }

    // 检查宽度和高度是否为2的幂次
    auto isPowerOfTwo = [](const int n) {
        return (n > 0) && ((n & (n - 1)) == 0);
        };

    if (width <= 0 || !isPowerOfTwo(width)) {
        std::cerr << "Width must be a positive power of 2!" << '\n';
        return 3;
    }

    if (height <= 0 || !isPowerOfTwo(height)) {
        std::cerr << "Height must be a positive power of 2!" << '\n';
        return 3;
    }

    std::vector<osg::ref_ptr<osg::Image>> images;
    std::map<std::string, size_t> geometryIdMap;
    size_t numImages = 0;

    for (const std::string& str : inputFiles) {
        osg::ref_ptr<osg::Image> img = osgDB::readImageFile(str);
        if (img->valid())
            images.push_back(img);
        else
            OSG_WARN << "Failed to read file: " << str << '\n';
    }

    osg::ref_ptr<TexturePacker> packer = new TexturePacker(width, height);
    for (size_t i = 0; i < images.size(); ++i)
    {
        geometryIdMap[images[i]->getFileName()] = packer->addElement(images[i].get());
    }

    osg::ref_ptr<osg::Image> atlas = packer->pack(numImages, true);
    if (atlas->valid()) 
    {
        if (osgDB::writeImageFile(*atlas.get(), output))
        {
            json info = json::object();
            info["filename"] = input;
            info["w"] = width;
            info["h"] = height;
            json children = json::array();
            for (const auto& item : geometryIdMap)
            {
                double x, y;
                int w, h;
                if (packer->getPackingData(item.second, x, y, w, h))
                {
                    json jsonItem = json::object();
                    jsonItem["filename"] = item.first;
                    jsonItem["x"] = x;
                    jsonItem["y"] = y;
                    jsonItem["w"] = w;
                    jsonItem["h"] = h;
                    children.push_back(jsonItem);
                }
            }
            info["images"] = children;
            const std::string atlasInfo = info.dump(4);
#ifndef NDEBUG
            const std::string infoOutputPath = osgDB::getFilePath(output) + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + osgDB::getStrippedName(output) + "-info.json";
#else
            const std::string infoOutputPath = osgDB::convertStringFromUTF8toCurrentCodePage(osgDB::getFilePath(output)) + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + osgDB::getStrippedName(output) + "-info.json";
#endif // !NDEBUG
            // 尝试创建目录（如果不存在）
            if (!osgDB::fileExists(osgDB::getFilePath(output))) {
                if (!osgDB::makeDirectory(osgDB::getFilePath(output))) {
                    std::cerr << "Failed to create directory: " << osgDB::getFilePath(output) << '\n';
                    return 4;
                }
            }
            std::ofstream infoOutFile(infoOutputPath);
            if (!infoOutFile.is_open()) {
                std::cerr << "Failed to open file for writing: " << osgDB::convertStringFromCurrentCodePageToUTF8(infoOutputPath) << '\n';
                std::cerr << "Erro code: " << errno << " (" << std::strerror(errno) << ")" << std::endl;
                return 5;
            }
            infoOutFile << atlasInfo;
            infoOutFile.close();
            std::cout << "Successfully packed texture atlas!" << '\n';
        }
        else
            std::cerr << "Failed to pack texture atlas!" << '\n';
    }
    else
        std::cerr << "Failed to pack texture atlas! The texture atlas size is too small!" << '\n';
    return 0;
}
