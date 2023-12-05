#include <osg/ArgumentParser>
#include <osgDB/FileNameUtils>
#include <iostream>
#include <fstream>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <cstdio>
int main(int argc, char** argv)
{
    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc, argv);
    arguments.getApplicationUsage()->setDescription(arguments.getApplicationName() + "this tool is used to convert b3dm to gltf.");
    arguments.getApplicationUsage()->addCommandLineOption("-i <input>","input b3dm file full path");
    arguments.getApplicationUsage()->addCommandLineOption("-o <output>","output glb file full path,The output file format depends on the format of the gltf file embedded in the b3dm file");
    arguments.getApplicationUsage()->addCommandLineOption("-h or --help", "Display command line parameters");

    // if user request help write it out to cout.
    if (arguments.read("-h") || arguments.read("--help"))
    {
        arguments.getApplicationUsage()->write(std::cout);
        return 1;
    }

    std::string input = "", output = "";
    while (arguments.read("-i", input));
    while (arguments.read("-o", output));

    if (input == "") {
        std::cout << "input file can not be null!" << std::endl;
        arguments.getApplicationUsage()->write(std::cout);
        return 0;
    }

    if (output == "") {
        std::cout << "output file can not be null!" << std::endl;
        arguments.getApplicationUsage()->write(std::cout);
        return 0;
    }

    std::ifstream b3dmFile(input, std::ios::binary);
    if (!b3dmFile) {
        std::cout << "can not open this b3dm file:" << input << std::endl;
        return 0;
    }

    //read file header

    //magic:b3dm
    char magic[4];
    b3dmFile.read(magic, 4);
    if (std::strncmp(magic, "b3dm", 4) != 0) {
        std::cout << "invalid b3dm file,b3dm header's magic is not \"b3dm\":" << input << std::endl;
        return 0;
    }
    //version:1
    uint32_t version;
    b3dmFile.read(reinterpret_cast<char*>(&version), 4);
    if (version != 1) {
        std::cout << "invalid b3dm file,b3dm header's version is not \"1\":" << input << std::endl;
        return 0;
    }
    //byteLength=28+featureTableJSONByteLength+featureTableBinaryByteLength+batchTableJSONByteLength+batchTableBinaryByteLength+glbLength
    uint32_t byteLength;
    b3dmFile.read(reinterpret_cast<char*>(&byteLength), 4);
    //featureTableJSONByteLength
    uint32_t featureTableJSONByteLength;
    b3dmFile.read(reinterpret_cast<char*>(&featureTableJSONByteLength), 4);
    //featureTableBinaryByteLength
    uint32_t featureTableBinaryByteLength;
    b3dmFile.read(reinterpret_cast<char*>(&featureTableBinaryByteLength), 4);
    //batchTableJSONByteLength
    uint32_t batchTableJSONByteLength;
    b3dmFile.read(reinterpret_cast<char*>(&batchTableJSONByteLength), 4);
    //batchTableBinaryByteLength
    uint32_t batchTableBinaryByteLength;
    b3dmFile.read(reinterpret_cast<char*>(&batchTableBinaryByteLength), 4);

    uint32_t position = 28 + featureTableJSONByteLength + featureTableBinaryByteLength + batchTableJSONByteLength + batchTableBinaryByteLength;
    b3dmFile.seekg(position, std::ios::beg);

    uint32_t glbLength = byteLength - position;
    std::unique_ptr<char[]> glbBuffer(new char[glbLength]);
    b3dmFile.read(glbBuffer.get(), glbLength);
    tinygltf::Model model;
    std::string err, waring;
    tinygltf::TinyGLTF writer;

    std::ofstream gltfFile(output, std::ios::binary);
    if (!gltfFile) {
        std::cout << "create gltf/glb file failed:" << output << std::endl;
        return false;
    }

    gltfFile.write(glbBuffer.get(), glbLength);

    gltfFile.close();
    if (osgDB::convertToLowerCase(osgDB::getFileExtension(output)) != "glb") {
        bool result = writer.LoadBinaryFromFile(&model, &err, &waring, output);
        if (result) {
            result = writer.WriteGltfSceneToFile(&model, output, true, true, false, false);
            if (result)
                std::cout << "Successfully converted B3DM to GLTF:" << input << " -> " << output << std::endl;
        }
        else {
            std::remove(output.c_str());
            std::cout << "Failed converted B3DM to GLTF:" << input << " -> " << output << std::endl;
        }
    }
    else {
        std::cout << "Successfully converted B3DM to GLB:" << input << " -> " << output << std::endl;
    }
    return 1;
}
