#ifndef TEXTURE_PACKING_VISITOR_H
#define TEXTURE_PACKING_VISITOR_H

#include <osg/NodeVisitor>
#include <osg/Texture2D>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/StateSet>
#include <osg/TexMat>
#include "utils/TexturePacker.h"
#include "osgdb_gltf/material/GltfMaterial.h"

enum class GltfTextureType {
    NORMAL,
    OCCLUSION,
    EMISSIVE,
    METALLICROUGHNESS,
    BASECOLOR,
    DIFFUSE,
    SPECULARGLOSSINESS
};

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

    static const std::string ExtensionNormalTextureName;
    static const std::string ExtensionNormalTextureOffsetX;
    static const std::string ExtensionNormalTextureOffsetY;
    static const std::string ExtensionNormalTextureScaleX;
    static const std::string ExtensionNormalTextureScaleY;
    static const std::string ExtensionNormalTextureTexCoord;

    static const std::string ExtensionOcclusionTextureName;
    static const std::string ExtensionOcclusionTextureOffsetX;
    static const std::string ExtensionOcclusionTextureOffsetY;
    static const std::string ExtensionOcclusionTextureScaleX;
    static const std::string ExtensionOcclusionTextureScaleY;
    static const std::string ExtensionOcclusionTextureTexCoord;

    static const std::string ExtensionEmissiveTextureName;
    static const std::string ExtensionEmissiveTextureOffsetX;
    static const std::string ExtensionEmissiveTextureOffsetY;
    static const std::string ExtensionEmissiveTextureScaleX;
    static const std::string ExtensionEmissiveTextureScaleY;
    static const std::string ExtensionEmissiveTextureTexCoord;

    static const std::string ExtensionMRTextureName;
    static const std::string ExtensionMRTextureOffsetX;
    static const std::string ExtensionMRTextureOffsetY;
    static const std::string ExtensionMRTextureScaleX;
    static const std::string ExtensionMRTextureScaleY;
    static const std::string ExtensionMRTexCoord;

    static const std::string ExtensionDiffuseTextureName;
    static const std::string ExtensionDiffuseTextureOffsetX;
    static const std::string ExtensionDiffuseTextureOffsetY;
    static const std::string ExtensionDiffuseTextureScaleX;
    static const std::string ExtensionDiffuseTextureScaleY;
    static const std::string ExtensionDiffuseTexCoord;

    static const std::string ExtensionSGTextureName;
    static const std::string ExtensionSGTextureOffsetX;
    static const std::string ExtensionSGTextureOffsetY;
    static const std::string ExtensionSGTextureScaleX;
    static const std::string ExtensionSGTextureScaleY;
    static const std::string ExtensionSGTexCoord;


    TexturePackingVisitor(int maxWidth, int maxHeight, std::string ext = ".png", std::string cachePath = "./", bool bPackTexture = true);

    void apply(osg::Drawable& drawable) override;

    void packTextures();

private:
    void optimizeOsgTexture(const osg::ref_ptr<osg::StateSet>& stateSet, const osg::ref_ptr<osg::Geometry>& geom);

    void optimizeOsgTextureSize(osg::ref_ptr<osg::Texture2D> texture);

    void exportOsgTexture(osg::ref_ptr<osg::Texture2D> texture);

    void optimizeOsgMaterial(const osg::ref_ptr<GltfMaterial>& gltfMaterial, const osg::ref_ptr<osg::Geometry>& geom);

    void packOsgTextures();

    void packOsgMaterials();

    void exportImage(const osg::ref_ptr<osg::Image>& img);

    void packImages(osg::ref_ptr<osg::Image>& img, std::vector<size_t>& indexes, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs, TexturePacker& packer);

    osg::ref_ptr<osg::Image> packImges(TexturePacker& packer, std::vector<osg::Image*>& imgs, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs);

    void removePackedImages(std::vector<osg::Image*>& imgs, const std::vector<osg::ref_ptr<osg::Image>>& deleteImgs);

    void processGltfPbrMRImages(std::vector<osg::Image*>& imageList, const GltfTextureType type);

    void updateGltfMaterialUserValue(osg::Geometry* geometry,
        osg::ref_ptr<osg::StateSet>& stateSetCopy,
        osg::ref_ptr<GltfMaterial> packedGltfMaterial,
        TexturePacker& packer,
        const int width,
        const int height,
        const osg::ref_ptr<osg::Texture2D>& oldTexture,
        const GltfTextureType type);

    void processGltfPbrSGImages(std::vector<osg::Image*>& images, const GltfTextureType type);

    bool resizeImageToPowerOfTwo(const osg::ref_ptr<osg::Image>& img);

    int findNearestPowerOfTwo(int value);

    std::string computeImageHash(const osg::ref_ptr<osg::Image>& img);

    static bool compareImageHeight(osg::Image* img1, osg::Image* img2);

    void addImageFromTexture(const osg::ref_ptr<osg::Texture2D>& texture, std::vector<osg::Image*>& imgs);

    template <typename T>
    T clamp(T value, T min, T max);

    void processGltfGeneralImages(std::vector<osg::Image*>& imgs,const GltfTextureType type);

    int _maxWidth, _maxHeight;
    std::string _ext, _cachePath;

    //osgMaterial
    std::vector<GltfMaterial*> _gltfMaterials;
    std::map<osg::Geometry*, GltfMaterial*> _geometryMatMap;

    //osgTexture
    std::vector<osg::Image*> _images;
    std::map<osg::Geometry*, osg::Image*> _geometryImgMap;

    bool _bPackTexture;
};

#endif // TEXTURE_PACKING_VISITOR_H
