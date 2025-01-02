#include <iostream>
#include <osgDB/ReadFile>
using namespace std;
int main() {
    const std::string filename = R"(E:\Code\2023\Other\data\20240529卢沟桥分洪枢纽2.fbx)";
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);
    return 1;
}
