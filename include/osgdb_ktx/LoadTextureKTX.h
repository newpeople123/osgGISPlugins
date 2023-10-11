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

    bool saveKtx(const std::string& file, bool asCubeMap,
        const std::vector<osg::Image*>& images, bool compressed = true);
    bool saveKtx2(std::ostream& out, bool asCubeMap,
        const std::vector<osg::Image*>& images, bool compressed = true);
}
