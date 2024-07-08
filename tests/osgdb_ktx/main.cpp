#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
using namespace std;
int findNearestPowerOfTwo(int value)
{
    int powerOfTwo = 1;
    while (powerOfTwo * 2 <= value) {
        powerOfTwo *= 2;
    }
    return powerOfTwo;
}

bool resizeImageToPowerOfTwo(const osg::ref_ptr<osg::Image>& img)
{
    int originalWidth = img->s();
    int originalHeight = img->t();
    int newWidth = findNearestPowerOfTwo(originalWidth);
    int newHeight = findNearestPowerOfTwo(originalHeight);

    if (!(originalWidth == newWidth && originalHeight == newHeight))
    {
        img->scaleImage(newWidth, newHeight, img->r());
        return true;
    }
    return false;
}

int main() {
    osgDB::Registry* instance = osgDB::Registry::instance();
    instance->addFileExtensionAlias("ktx2", "ktx");//插件注册别名
    osg::ref_ptr<osg::Image> jpgImg = osgDB::readImageFile(R"(E:\Code\2023\Other\data\jianzhu+tietu.fbm\bricks006dd.jpg)");
    resizeImageToPowerOfTwo(jpgImg);
    osg::ref_ptr<osg::Image> pngImg = osgDB::readImageFile(R"(E:\Code\2023\Other\data\jianzhu+tietu.fbm\MHDPI022.png)");
    resizeImageToPowerOfTwo(pngImg);
    osgDB::writeImageFile(*jpgImg.get(), "./bricks006dd.ktx");
    osgDB::writeImageFile(*pngImg.get(), "./MHDPI022.ktx2");

    return 1;
}
