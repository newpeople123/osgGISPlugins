#ifndef OSG_GIS_PLUGINS_GLTF_MATERIAL_H
#define OSG_GIS_PLUGINS_GLTF_MATERIAL_H 1
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/Image>
#include <array>
#include "gltf/Extensions.h"
class GltfMaterial :public osg::Material
{
public:
    osg::ref_ptr<osg::Texture2D> normalTexture;
    osg::ref_ptr<osg::Texture2D> occlusionTexture;
    osg::ref_ptr<osg::Texture2D> emissiveTexture;
    std::array<double, 3> emissiveFactor{ 0.0 };
    osg::MixinVector<GltfExtension*> materialExtensions;
    osg::MixinVector<GltfExtension*> materialExtensionsByCesiumSupport;
    virtual osg::ref_ptr<osg::Image> mergeImages(const osg::ref_ptr<osg::Image>& img1, const osg::ref_ptr<osg::Image>& img2) = 0;
    virtual ~GltfMaterial() = default;
};
#endif // !OSG_GIS_PLUGINS_GLTF_MATERIAL_H
