#include "3dtiles/optimizer/mesh/FlattenTransformVisitor.h"
#include "3dtiles/optimizer/mesh/MeshOptimizer.h"
#include "3dtiles/optimizer/mesh/PmpMeshOptimizer.h"
#include "3dtiles/optimizer/mesh/MeshOptimizerVisitor.h"
#include <osg/ArgumentParser>
#include <iostream>
#include <osgDB/ConvertUTF>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
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

    std::string input, output;
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
#ifndef NDEBUG
#else
    input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
    output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(input);
    if (node.valid()) {
        std::string simplifiedRatio = "0.5";
        while (arguments.read("-ratio", simplifiedRatio));
        const double ratio = std::stod(simplifiedRatio);
        if (ratio < 1.0) {
            //index mesh
            osgUtil::Optimizer optimizer;
            optimizer.optimize(node, osgUtil::Optimizer::INDEX_MESH);
            //flatten transform
            FlattenTransformVisitor ftv;
            node->accept(ftv);
            //mesh optimizer
            MeshOptimizerBase* meshOptimizer = new MeshOptimizer;
            MeshOptimizerVisitor mov(meshOptimizer, ratio);
            node->accept(mov);
            //write node to file
            osgDB::writeNodeFile(*node.get(), output);
            
        }
    }
    return 1;
}
