#include "3dtiles/optimizer/MeshSimplifier.h"
#include "3dtiles/optimizer/MeshOptimizer.h"
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
class TestVisitor : public osg::NodeVisitor {
public:
    double area = 0.0;
    TestVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
    double computeTriangleArea(const osg::Vec3 a, const osg::Vec3 b, const osg::Vec3 c) {
        osg::Vec3 ab = b - a;
        osg::Vec3 ac = c - a;
        return (ab ^ ac).length() * 0.5;
    }
    double computeTriangleAreaXY(const osg::Vec3& A, const osg::Vec3& B, const osg::Vec3& C) {
        osg::Vec3 A_xy(A.x(), A.y(), 0.0);
        osg::Vec3 B_xy(B.x(), B.y(), 0.0);
        osg::Vec3 C_xy(C.x(), C.y(), 0.0);

        osg::Vec3 AB = B_xy - A_xy;
        osg::Vec3 AC = C_xy - A_xy;

        return (AB ^ AC).length() * 0.5;
    }

    void apply(osg::Drawable& drawable) override
    {
        osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
        const unsigned int psetCount = geom->getNumPrimitiveSets();
        const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
        if (psetCount <= 0) return;
        for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
            osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(primIndex);
            if (typeid(*pset.get()) == typeid(osg::DrawElementsUShort)) {
                osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
                for (size_t i = 0; i < drawElementsUShort->size(); i += 3) {
                    const unsigned short aIndex = drawElementsUShort->at(i + 0);
                    const unsigned short bIndex = drawElementsUShort->at(i + 1);
                    const unsigned short cIndex = drawElementsUShort->at(i + 2);

                    const osg::Vec3 a = positions->at(aIndex);
                    const osg::Vec3 b = positions->at(bIndex);
                    const osg::Vec3 c = positions->at(cIndex);
                    area += computeTriangleAreaXY(a, b, c);
                }
            }
            else if (typeid(*pset.get()) == typeid(osg::DrawElementsUInt)) {
                osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
                for (size_t i = 0; i < drawElementsUInt->size(); i += 3) {
                    const unsigned int aIndex = drawElementsUInt->at(i + 0);
                    const unsigned int bIndex = drawElementsUInt->at(i + 1);
                    const unsigned int cIndex = drawElementsUInt->at(i + 2);

                    const osg::Vec3 a = positions->at(aIndex);
                    const osg::Vec3 b = positions->at(bIndex);
                    const osg::Vec3 c = positions->at(cIndex);
                    area += computeTriangleAreaXY(a, b, c);
                }
            }
        }
    }

};
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

    std::string input = R"(E:\Code\2023\Other\data\卡拉电站.fbx)", output = R"(D:\nginx-1.22.1\html\gltf\cow.gltf)";
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
        //flatten transform、index mesh(顺序很重要，
        //还可以使用osgUtil::Optimizer::VERTEX_POSTTRANSFORM | osgUtil::Optimizer::VERTEX_PRETRANSFORM，
        // 但是由于还要进行简化，所以选择使用meshoptimizer进行VERTEX_POSTTRANSFORM和VERTEX_PRETRANSFORM优化)
        osgUtil::Optimizer optimizer;
        optimizer.optimize(node, osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS|osgUtil::Optimizer::INDEX_MESH);
        TestVisitor tv1;
        node->accept(tv1);
        const double area1 = tv1.area;
        //mesh optimizer
        MeshSimplifierBase* meshOptimizer = new MeshSimplifier;
        MeshOptimizer mov(meshOptimizer, ratio);
        node->accept(mov);
        TestVisitor tv2;
        node->accept(tv2);
        const double area2 = tv2.area;
        const double change = abs(area2 - area1);
        //write node to file
        osgDB::writeNodeFile(*node.get(), output);
    }
    return 1;
}
