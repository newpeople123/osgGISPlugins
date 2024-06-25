#ifndef TEXTURE_PACKING_VISITOR_H
#define TEXTURE_PACKING_VISITOR_H

#include <osg/NodeVisitor>
#include <osg/Texture2D>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/StateSet>
#include <osg/TexMat>
#include "utils/TexturePacker.h"

class TexturePackingVisitor : public osg::NodeVisitor
{
public:
    static const std::string Filename;
    static const std::string ExtensionName;
    static const std::string ExtensionOffsetX;
    static const std::string ExtensionOffsetY;
    static const std::string ExtensionScaleX;
    static const std::string ExtensionScaleY;
    static const std::string ExtensionTexCoord;

    TexturePackingVisitor(int maxWidth, int maxHeight, std::string ext = ".png", std::string cachePath = "./");

    void apply(osg::Drawable& drawable) override;

    void packTextures();

    void packImages(osg::ref_ptr<osg::Image>& img, std::vector<size_t>& indexes, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs, TexturePacker& packer);
private:
    void _exportImage(const osg::ref_ptr<osg::Image>& img);
    bool _resizeImageToPowerOfTwo(const osg::ref_ptr<osg::Image>& img);
    int _findNearestPowerOfTwo(int value);
    std::string _computeImageHash(const osg::ref_ptr<osg::Image>& img);
    int _maxWidth, _maxHeight;
    std::vector<osg::Image*> _images;
    std::map<osg::Geometry*, osg::Image*> _geometryMap;
    std::string _ext, _cachePath;

    
};

#endif // TEXTURE_PACKING_VISITOR_H
