#include <osg/ArgumentParser>
#include <3dtiles/Write3DTiles.h>
#include <osgDB/ReadFile>
#include <iostream>

#include "osgDB/FileUtils"
#include <3dtiles/OctreeBuilder.h>
#include <3dtiles/QuadTreeBuilder.h>

int main(int argc, char** argv)
{
#ifdef _WIN32
    //system("chcp 65001");
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32

    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc, argv);
    arguments.getApplicationUsage()->setDescription(arguments.getApplicationName() + "this tool is used to convert 3D model to 3dtiles.");
    arguments.getApplicationUsage()->addCommandLineOption("-i <file>", "input 3D model file full path");
    arguments.getApplicationUsage()->addCommandLineOption("-o <path>", "output 3dtiles path");
    arguments.getApplicationUsage()->addCommandLineOption("-tf <png/jpg/webp/ktx2>", "texture format,option values are png、jpg、webp、ktx2，default value is png.");
    arguments.getApplicationUsage()->addCommandLineOption("-vf <draco/meshopt/none>", "vertex format,option values are draco、meshopt、none,default is none.");
    arguments.getApplicationUsage()->addCommandLineOption("-t <quad/oc>", " tree format,option values are quad、oc,default is oc.");
    arguments.getApplicationUsage()->addCommandLineOption("-max <number>", "the maximum number of triangles contained in the b3dm node.default value is 40000.");
    arguments.getApplicationUsage()->addCommandLineOption("-ratio <number>", "Simplified ratio of intermediate nodes.default is 0.5.");
    arguments.getApplicationUsage()->addCommandLineOption("-lat <number>", "datum point's latitude");
    arguments.getApplicationUsage()->addCommandLineOption("-lng <number>", "datum point's longitude");
    arguments.getApplicationUsage()->addCommandLineOption("-height <number>", "datum point's height");
    arguments.getApplicationUsage()->addCommandLineOption("-draco_compression_level <low/medium/hight>", "draco compression level");
    arguments.getApplicationUsage()->addCommandLineOption("-h or --help", "Display command line parameters");

    // if user request help write it out to cout.
    if (arguments.read("-h") || arguments.read("--help"))
    {
        arguments.getApplicationUsage()->write(std::cout);
        return 1;
    }

    std::string input = R"(E:\Code\2023\Other\data\龙翔桥站厅.FBX)", output = "D:\\nginx-1.22.1\\html\\3dtiles\\new-hlod1";
    while (arguments.read("-i", input));
    while (arguments.read("-o", output));

    if (input.empty()) {
        std::cout << "input file can not be null!" << '\n';
        arguments.getApplicationUsage()->write(std::cout);
        return 0;
    }

    if (output.empty()) {
        std::cout << "output file can not be null!" << '\n';
        arguments.getApplicationUsage()->write(std::cout);
        return 0;
    }
    //input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(input);
    if (node.valid()) {

        std::string textureFormat = "jpg", vertexFormat = "none", treeFormat = "quad", maxTriangle = "40000", simplifiedRatio = "0.5", latitude = "30", longitude = "116", height = "300", comporessLevel="high";
        while (arguments.read("-tf", textureFormat));
        while (arguments.read("-vf", vertexFormat));
        while (arguments.read("-t", treeFormat));
        while (arguments.read("-max", maxTriangle));
        while (arguments.read("-ratio", simplifiedRatio));
        while (arguments.read("-lat", latitude));
        while (arguments.read("-lng", longitude));
        while (arguments.read("-height", height));
        while (arguments.read("-comporessLevel", comporessLevel));
        osg::ref_ptr<osgDB::Options> options = new osgDB::Options;
        std::string optionStr = "textureType=" + textureFormat + " compressionType=" + vertexFormat + " comporessLevel=" + comporessLevel;
        options->setOptionString(optionStr);
        try {
            int max = std::stoi(maxTriangle);
            double ratio = std::stod(simplifiedRatio);
            double lat = std::stod(latitude);
            double lng = std::stod(longitude);
            double h = std::stod(height);

            osgDB::makeDirectory(output);
            osg::ref_ptr<TileNode> threeDTilesNode;
            if (treeFormat == "quad") {
                const QuadTreeBuilder qtb(node, max, 8);
                threeDTilesNode = qtb.rootTreeNode;
            }
            else {
                const OctreeBuilder octb(node, max, 8);
                threeDTilesNode = octb.rootTreeNode;
            }
            const osg::EllipsoidModel ellipsoidModel;
            osg::Matrixd matrix;
            ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(lat), osg::DegreesToRadians(lng), h, matrix);
            std::vector<double> rootTransform;
            const double* ptr = matrix.ptr();
            for (unsigned i = 0; i < 16; ++i)
                rootTransform.push_back(*ptr++);
            Write3DTiles(threeDTilesNode, options, output, rootTransform);

        }
        catch (const std::invalid_argument& e) {
            std::cerr << "invalid input: " << e.what() << '\n';
        }
        catch (const std::out_of_range& e) {
            std::cerr << "value out of range: " << e.what() << '\n';
        }

    }
    else {
        std::cout << "Error:can not read 3d model file!" << '\n';
    }
}
