#include <osg/Texture2D>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <iterator>
#include <fstream>
#include <iostream>
#include <ktx.h>

namespace osg
{
    std::vector<osg::ref_ptr<osg::Image>> loadKtx(const std::string& file);
    std::vector<osg::ref_ptr<osg::Image>> loadKtx2(std::istream& in);

    bool saveKtx(const std::string& file,const osg::Image* image, bool compressed = true);
    bool saveKtx2(std::ostream& out,const osg::Image* image, bool compressed = true);
}
