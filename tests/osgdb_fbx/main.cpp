#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/SmoothingVisitor>
using namespace std;
int main() {
    osg::ref_ptr<osg::Node> node=osgDB::readNodeFile(R"(F:\Program Files\Epic Games\UE_5.5\Templates\TemplateResources\Standard\1M_Cube.FBX)");
	osgUtil::SmoothingVisitor sv;
	sv.setCreaseAngle(0.0);
	node->accept(sv);
	osgDB::writeNodeFile(*node.get(), R"(D:\nginx-1.22.1\html\t2.fbx)");
    return 0;
}
