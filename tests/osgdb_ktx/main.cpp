#include <iostream>
#include <osgDB/ReadFile>
using namespace std;
int main() {
    osg::ref_ptr<osg::Image> jpgImg = osgDB::readImageFile(R"(E:\Code\2023\Other\data\jianzhu+tietu.fbm\bricks006dd.jpg)");
    osg::ref_ptr<osg::Image> pngImg = osgDB::readImageFile(R"(E:\Code\2023\Other\data\jianzhu+tietu.fbm\MHDPI022.png)");

    return 1;
}
