#include "utils/Simplifier.h"
#include "utils/GltfOptimizer.h"
#include <osg/ArgumentParser>
#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace osgGISPlugins;

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
    usage->setCommandLineUsage("simplifier.exe -i C:\\input\\test.fbx -o C:\\output\\test_05.fbx -ratio 0.1");
    usage->addCommandLineOption("-i <file>", "input 3D model file full path");
    usage->addCommandLineOption("-o <file>", "output simplified 3D model file full path");
    usage->addCommandLineOption("-ratio <number>", "Simplified ratio of intermediate nodes.default is 0.5.");
    usage->addCommandLineOption("-aggressive", "More aggressive simplification without preserving topology.");
    usage->addCommandLineOption("-h or --help", "Display command line parameters");
    if (arguments.read("-h") || arguments.read("--help"))
    {
        usage->write(std::cout);
        return 100;
    }

    std::string input = "", output = "";
    while (arguments.read("-i", input));
    while (arguments.read("-o", output));

    if (input.empty())
    {
        OSG_NOTICE << "input file can not be null!" << '\n';
        usage->write(std::cout);
        return 1;
    }

    if (output.empty())
    {
        OSG_NOTICE << "output file can not be null!" << '\n';
        usage->write(std::cout);
        return 2;
    }
#ifndef NDEBUG
#else
    input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
    output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG
    osg::ref_ptr<osgDB::Options> readOptions = new osgDB::Options;
    readOptions->setOptionString("TessellatePolygons");
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(input, readOptions.get());
    if (node.valid())
    {
        std::string simplifiedRatio = "0.5";
        while (arguments.read("-ratio", simplifiedRatio));
        const double ratio = osg::clampTo(std::stod(simplifiedRatio), 0.0, 1.0);

        if (ratio < 1.0 && ratio>0)
        {
            GltfOptimizer optimizer;
            optimizer.optimize(node, GltfOptimizer::INDEX_MESH);
            if (arguments.find("-aggressive") > 0)
            {
                Simplifier simplifier(ratio, true);
                node->accept(simplifier);
            }
            else
            {
                Simplifier simplifier(ratio);
                node->accept(simplifier);
            }
        }
        else
        {
            OSG_NOTICE << "Faild simplify model,simplified ratio must be between 0.0 and 1.0!" << std::endl;
        }
        //write node to file
        if (osgDB::writeNodeFile(*node.get(), output))
        {
            OSG_NOTICE << "Successfully simplify model!" << std::endl;
        }
    }
    return 0;
}
