#include <osg/ArgumentParser>
#include <osgDB/FileNameUtils>
#include <iostream>
#include <fstream>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <cstdio>
#include "osgdb_gltf/b3dm/ThreeDModelHeader.h"
#include <osgDB/ConvertUTF>
using namespace osgGISPlugins;
int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc, argv);
    arguments.getApplicationUsage()->setDescription(arguments.getApplicationName() + "this tool is used to convert b3dm to gltf.");
    arguments.getApplicationUsage()->addCommandLineOption("-i <input>","input b3dm file full path");
    arguments.getApplicationUsage()->addCommandLineOption("-o <output>","output gltf/glb file full path,The output file format depends on the format of the gltf file embedded in the b3dm file");
    arguments.getApplicationUsage()->addCommandLineOption("-h or --help", "Display command line parameters");

    // if user request help write it out to cout.
    if (arguments.read("-h") || arguments.read("--help"))
    {
        arguments.getApplicationUsage()->write(std::cout);
        return 1;
    }

    std::string input, output;
    while (arguments.read("-i", input));
    while (arguments.read("-o", output));

#ifndef NDEBUG
#else
    input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
    output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG

    if (input.empty()) {
        std::cerr << "input file can not be null!" << '\n';
        arguments.getApplicationUsage()->write(std::cout);
        return 0;
    }

    if (output.empty()) {
        std::cerr << "output file can not be null!" << '\n';
        arguments.getApplicationUsage()->write(std::cout);
        return 0;
    }

    std::ifstream b3dmFile(input, std::ios::binary);
    if (!b3dmFile) {
        std::cerr << "can not open this b3dm file:" << input << '\n';
        return 0;
    }

    B3DMFile b3dm;
    b3dmFile.read(reinterpret_cast<char*>(&b3dm.header), sizeof(B3DMHeader));
    if (std::strncmp(b3dm.header.magic, "b3dm", 4) != 0) {
        std::cerr << "invalid b3dm file,b3dm header's magic is not \"b3dm\":" << input << '\n';
        return 0;
    }
    if (b3dm.header.version != 1) {
        std::cerr << "invalid b3dm file,b3dm header's version is not \"1\":" << input << '\n';
        return 0;
    }
    //读取Feature Table JSON
    if (b3dm.header.featureTableJSONByteLength > 0)
    {
        b3dm.featureTableJSON.resize(b3dm.header.featureTableJSONByteLength);
        b3dmFile.read(&b3dm.featureTableJSON[0], b3dm.header.featureTableJSONByteLength);
    }
    //读取Feature Table Binary
    if (b3dm.header.featureTableBinaryByteLength > 0)
    {
        b3dm.featureTableBinary.resize(b3dm.header.featureTableBinaryByteLength);
        b3dmFile.read(&b3dm.featureTableBinary[0], b3dm.header.featureTableBinaryByteLength);
    }

    // 读取Batch Table JSON
    if (b3dm.header.batchTableJSONByteLength > 0) {
        b3dm.batchTableJSON.resize(b3dm.header.batchTableJSONByteLength);
        b3dmFile.read(&b3dm.batchTableJSON[0], b3dm.header.batchTableJSONByteLength);
    }

    // 读取Batch Table Binary
    if (b3dm.header.batchTableBinaryByteLength > 0) {
        b3dm.batchTableBinary.resize(b3dm.header.batchTableBinaryByteLength);
        b3dmFile.read(&b3dm.batchTableBinary[0], b3dm.header.batchTableBinaryByteLength);
    }
    // 读取GLB数据
    uint32_t position = 28 + b3dm.header.featureTableJSONByteLength + b3dm.header.featureTableBinaryByteLength + b3dm.header.batchTableJSONByteLength + b3dm.header.batchTableBinaryByteLength;
    b3dmFile.seekg(position, std::ios::beg);

    uint32_t glbLength = b3dm.header.byteLength - position;
    b3dm.glbData.resize(glbLength);
    b3dmFile.read(&b3dm.glbData[0], glbLength);

    std::ofstream gltfFile(output, std::ios::binary);
    if (!gltfFile) {
        std::cerr << "create gltf/glb file failed:" << output << '\n';
        return false;
    }

    gltfFile.write(b3dm.glbData.c_str(), glbLength);
    gltfFile.close();

    if (osgDB::convertToLowerCase(osgDB::getFileExtension(output)) != "glb") {
        tinygltf::TinyGLTF writer;
        tinygltf::Model model;
        std::string err, waring;
        bool result = writer.LoadBinaryFromFile(&model, &err, &waring, output);
        if (result) {
            result = writer.WriteGltfSceneToFile(&model, output, true, false, false, false);
            if (result)
                std::cout << "Successfully converted B3DM to GLTF:" << input << " -> " << output << '\n';
        }
        else {
            std::remove(output.c_str());
            std::cerr << "Failed converted B3DM to GLTF:" << input << " -> " << output << '\n';
        }
    }
    else {
        std::cout << "Successfully converted B3DM to GLB:" << input << " -> " << output << '\n';
    }
    return 1;
}
