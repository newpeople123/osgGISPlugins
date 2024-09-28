#include <osg/ArgumentParser>
#include <osgDB/Options>
#include <osgDB/ConvertUTF>
#include <osgDB/ReadFile>
#include <iostream>
#include <osgDB/FileUtils>
#include <osg/MatrixTransform>
#include <osgUtil/SmoothingVisitor>
#ifdef _WIN32
#include <windows.h>
#endif
#include "3dtiles/Tileset.h"
#include "3dtiles/hlod/QuadtreeBuilder.h"
#include "3dtiles/hlod/OctreeBuilder.h"
int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
    //插件注册别名
    osgDB::Registry* instance = osgDB::Registry::instance();
    instance->addFileExtensionAlias("glb", "gltf");
    instance->addFileExtensionAlias("b3dm", "gltf");
    instance->addFileExtensionAlias("i3dm", "gltf");
    instance->addFileExtensionAlias("ktx2", "ktx");

    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc, argv);
    osg::ApplicationUsage* usage = arguments.getApplicationUsage();
    usage->setDescription(arguments.getApplicationName() + ",that is used to convert 3D model to 3dtiles.");
    usage->setCommandLineUsage("model23dtiles.exe -i C:\\input\\test.fbx -o C:\\output\\test -lat 116.0 -lng 39.0 -height 300.0 -tf ktx2");
    usage->addCommandLineOption("-i <file>", "input 3D model file full path,must be a file.");
    usage->addCommandLineOption("-o <folder>", "output 3dtiles path,must be a directory.");
    usage->addCommandLineOption("-tf <png/jpg/webp/ktx2>", "texture format,option values are png、jpg、webp、ktx2，default value is jpg.");
    usage->addCommandLineOption("-vf <draco/meshopt/quantize/quantize_meshopt>", "vertex format,option values are draco、meshopt、quantize、quantize_meshopt,default is none.");
    usage->addCommandLineOption("-t <quad/oc>", " tree format,option values are quad、oc,default is oc.");
    usage->addCommandLineOption("-ratio <number>", "Simplified ratio of intermediate nodes.default is 0.5.");
    usage->addCommandLineOption("-lat <number>", "datum point's latitude.");
    usage->addCommandLineOption("-lng <number>", "datum point's longitude.");
    usage->addCommandLineOption("-height <number>", "datum point's height.");
    usage->addCommandLineOption("-translationX <number>", "The x-coordinate of the model datum point,default is 0, must be a power of 2.");
    usage->addCommandLineOption("-translationY <number>", "The y-coordinate of the model datum point,default is 0, must be a power of 2.");
    usage->addCommandLineOption("-translationZ <number>", "The z-coordinate of the model datum point,default is 0, must be a power of 2.");
    usage->addCommandLineOption("-upAxis <X/Y/Z>", "Indicate which axis of the model is facing upwards,default is Y");
    usage->addCommandLineOption("-maxTextureWidth <number>", "single texture's max width,default is 256.");
    usage->addCommandLineOption("-maxTextureHeight <number>", "single texture's max height,default is 256.");
    usage->addCommandLineOption("-maxTextureAtlasWidth <number>", "textrueAtlas's max width,default is 2048.");
    usage->addCommandLineOption("-maxTextureAtlasHeight <number>", "textrueAtlas's max height,default is 2048.");
    usage->addCommandLineOption("-comporessLevel <low/medium/high>", "draco/quantize/quantize_meshopt compression level,default is medium.");
    usage->addCommandLineOption("-recomputeNormal", "Recalculate normals.");
    usage->addCommandLineOption("-unlit", "Enable KHR_materials_unlit, the model is not affected by lighting.");


    usage->addCommandLineOption("-h or --help", "Display command line parameters.");

    // if user request help write it out to cout.
    if (arguments.read("-h") || arguments.read("--help"))
    {
        usage->write(std::cout);
        return 1;
    }

    std::string input = "", output = "";

    while (arguments.read("-i", input));
    while (arguments.read("-o", output));

    if (input.empty()) {
        OSG_FATAL << "input file can not be null!" << '\n';
        usage->write(std::cout);
        return 0;
    }

    if (output.empty()) {
        OSG_FATAL << "output file can not be null!" << '\n';
        usage->write(std::cout);
        return 0;
    }
#ifndef NDEBUG
#else
    input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
    output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG
    OSG_NOTICE << "Reading model file..." << std::endl;
    osg::ref_ptr<osgDB::Options> readOptions = new osgDB::Options;
    readOptions->setOptionString("TessellatePolygons");
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(input, readOptions.get());

    if (node.valid()) {
        OSG_NOTICE << "Resolving parameters..." << std::endl;
        std::string textureFormat = "jpg", vertexFormat = "none", treeFormat = "quad", simplifiedRatio = "0.5", latitude = "30", longitude = "116", height = "300", comporessLevel = "medium";
        std::string maxHeight = "256", maxWidth = "256", maxTextureAtlasWidth = "2048", maxTextureAtlasHeight = "2048";
        std::string translationX = "0", translationY = "0", translationZ = "0", upAxis = "Y";
        while (arguments.read("-tf", textureFormat));
        while (arguments.read("-vf", vertexFormat));
        while (arguments.read("-t", treeFormat));
        if (treeFormat != "oc")
            treeFormat = "quad";
        while (arguments.read("-ratio", simplifiedRatio));
        while (arguments.read("-lat", latitude));
        while (arguments.read("-lng", longitude));
        while (arguments.read("-height", height));
        while (arguments.read("-maxTextureWidth", maxWidth));
        while (arguments.read("-maxTextureHeight", maxHeight));
        while (arguments.read("-maxTextureAtlasWidth", maxTextureAtlasWidth));
        while (arguments.read("-maxTextureAtlasHeight", maxTextureAtlasHeight));
        while (arguments.read("-comporessLevel", comporessLevel));
        while (arguments.read("-translationX", translationX));
        while (arguments.read("-translationY", translationY));
        while (arguments.read("-translationZ", translationZ));
        while (arguments.read("-upAxis", upAxis));
        try {
            double ratio = std::stod(simplifiedRatio);
            double lat = std::stod(latitude);
            double lng = std::stod(longitude);
            double h = std::stod(height);

            double x = std::stod(translationX);
            double y = std::stod(translationY);
            double z = std::stod(translationZ);

            int width = std::stoi(maxWidth);
            int height = std::stoi(maxHeight);
            int textureAtlasWidth = std::stoi(maxTextureAtlasWidth);
            int textureAtlasHeight = std::stoi(maxTextureAtlasHeight);

            if (arguments.find("-recomputeNormal") > 0)
            {
                osgUtil::SmoothingVisitor smoothingVisitor;
                node->accept(smoothingVisitor);
            }

            const osg::Vec3d datumPoint = osg::Vec3d(-x, -y, -z);
            osg::ref_ptr<osg::MatrixTransform> xtransform = new osg::MatrixTransform;
            osg::Matrixd matrix;
            if (upAxis == "X")
            {
                const osg::Quat quat = osg::Quat(-osg::PI_2, osg::Vec3d(0.0, 0.0, 1.0));
                matrix.makeRotate(quat);
                matrix.setTrans(quat * datumPoint);
            }
            else if (upAxis == "Z")
            {
                const osg::Quat quat = osg::Quat(-osg::PI_2, osg::Vec3(1.0, 0.0, 0.0));
                matrix.makeRotate(quat);
                matrix.setTrans(quat * datumPoint);
            }
            else if (upAxis == "Y")
            {
                matrix.setTrans(datumPoint);
            }
            xtransform->setMatrix(matrix);
            xtransform->addChild(node);

            TreeBuilder* treeBuilder = new QuadtreeBuilder;
            if (treeFormat == "oc")
                treeBuilder = new OctreeBuilder;
            OSG_NOTICE << "Building " + treeFormat << " tree..." << std::endl;
            osg::ref_ptr<Tileset> tileset = new Tileset(xtransform, *treeBuilder);

            Tileset::Config config;
            config.latitude = lat;
            config.longitude = lng;
            config.height = h;
            config.simplifyRatio = ratio;
            config.path = output;
            config.gltfTextureOptions.maxWidth = width;
            config.gltfTextureOptions.maxHeight = height;
            config.gltfTextureOptions.maxTextureAtlasWidth = textureAtlasWidth;
            config.gltfTextureOptions.maxTextureAtlasHeight = textureAtlasHeight;
            config.gltfTextureOptions.ext = "." + textureFormat;

            std::string optionsStr = "";
            if (vertexFormat == "draco")
            {
                if (comporessLevel == "low")
                {
                    optionsStr = "ct=draco vp=16 vt=14 vc=10 vn=12 vg=18 ";
                }
                else if (comporessLevel == "high")
                {
                    optionsStr = "ct=draco vp=12 vt=12 vc=8 vn=8 vg=14 ";
                }
            }
            else if (vertexFormat == "meshopt")
            {
                optionsStr = "ct=meshopt ";
            }
            else if (vertexFormat == "quantize")
            {
                if (comporessLevel == "low")
                {
                    optionsStr = "quantize vp=16 vt=14 vn=12 vc=12 ";
                }
                else if (comporessLevel == "high")
                {
                    optionsStr = "quantize vp=10 vt=8 vn=4 vc=4 ";
                }
            }
            else if (vertexFormat == "quantize_meshopt")
            {
                optionsStr = "ct=meshopt ";
                if (comporessLevel == "low")
                {
                    optionsStr += "quantize vp=16 vt=14 vn=12 vc=12 ";
                }
                else if (comporessLevel == "high")
                {
                    optionsStr += "quantize vp=10 vt=8 vn=4 vc=4 ";
                }
            }

            if (arguments.find("-unlit") > 0)
            {
                optionsStr += " unlit";
            }
            config.options->setOptionString(optionsStr);

            OSG_NOTICE << "Exporting 3dtiles..." << std::endl;
            if (!tileset->toFile(config))
                OSG_FATAL << "Failed converted " + input + " to 3dtiles..." << std::endl;
            else
                OSG_NOTICE << "Successfully converted " + input + " to 3dtiles!" << std::endl;
            delete treeBuilder;
            
        }
        catch (const std::invalid_argument& e) {
            OSG_FATAL << "invalid input: " << e.what() << '\n';
        }
        catch (const std::out_of_range& e) {
            OSG_FATAL << "value out of range: " << e.what() << '\n';
        }

    }
    else {
        OSG_FATAL << "Error:can not read 3d model file!" << '\n';
    }

    return 1;
}
