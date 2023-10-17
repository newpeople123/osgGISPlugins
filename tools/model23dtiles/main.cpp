#include <osg/ArgumentParser>
#include <3dtiles/QuadTreeBuilder.h>
#include <3dtiles/Output3DTiles.h>
#include <osgDB/ReadFile>
#include <iostream>
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
    arguments.getApplicationUsage()->addCommandLineOption("-vf <draco/none>", "vertex format,option values are draco、none,default is none.");
    arguments.getApplicationUsage()->addCommandLineOption("-t <quad/oc>", " tree format,option values are quad、oc,default is oc.");
    arguments.getApplicationUsage()->addCommandLineOption("-max <number>", "the maximum number of triangles contained in the b3dm node.default value is 40000.");
    arguments.getApplicationUsage()->addCommandLineOption("-ratio <number>", "Simplified ratio of intermediate nodes.default is 0.5.");
    arguments.getApplicationUsage()->addCommandLineOption("-lat <number>", "datum point's latitude");
    arguments.getApplicationUsage()->addCommandLineOption("-lng <number>", "datum point's longitude");
    arguments.getApplicationUsage()->addCommandLineOption("-height <number>", "datum point's height");
    arguments.getApplicationUsage()->addCommandLineOption("-draco_compression_level <low/medium/hight>", "draco compression level");
    arguments.getApplicationUsage()->addCommandLineOption("-draco_position_compression_level <number>", "PositionQuantizationBits value");
    arguments.getApplicationUsage()->addCommandLineOption("-draco_texcoord_compression_level <number>", "TexCoordQuantizationBits value");
    arguments.getApplicationUsage()->addCommandLineOption("-draco_normal_compression_level <number>", "NormalQuantizationBits value");
    arguments.getApplicationUsage()->addCommandLineOption("-draco_color_compression_level <number>", "ColorQuantizationBits value");
    arguments.getApplicationUsage()->addCommandLineOption("-draco_generic_compression_level <number>", "GenericQuantizationBits value");
    arguments.getApplicationUsage()->addCommandLineOption("-h or --help", "Display command line parameters");

    // if user request help write it out to cout.
    if (arguments.read("-h") || arguments.read("--help"))
    {
        arguments.getApplicationUsage()->write(std::cout);
        return 1;
    }

    std::string input = "E:\\Code\\2023\\Other\\data\\芜湖水厂总装1.fbx", output = "D:\\nginx-1.22.1\\html\\3dtiles\\singleThread";
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

    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(input);
    if (node.valid()) {

        std::string textureFormat = "png", vertexFormat = "none", treeFormat = "quad", maxTriangle = "40000", simplifiedRatio = "0.1", latitude = "30", longitude = "116", height = "300", comporessLevel="high";
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

            OsgNodeTo3DTiles(node, options, treeFormat, max, ratio, output, lng, lat, h);
        }
        catch (const std::invalid_argument& e) {
            std::cerr << "invalid input: " << e.what() << std::endl;
        }
        catch (const std::out_of_range& e) {
            std::cerr << "value out of range: " << e.what() << std::endl;
        }

    }
    else {
        std::cout << "Error:can not read 3d model file!" << std::endl;
    }
}
