#include "utils/TextureOptimizer.h"
#include <osgDB/WriteFile>
#include <osg/ImageUtils>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <iomanip>
#include "osgdb_gltf/material/GltfPbrMRMaterial.h"
#include "osgdb_gltf/material/GltfPbrSGMaterial.h"
TexturePackingVisitor::TexturePackingVisitor(int maxWidth, int maxHeight, std::string ext, std::string cachePath, bool bPackTexture) : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
_maxWidth(maxWidth), _maxHeight(maxHeight), _ext(ext), _cachePath(cachePath), _bPackTexture(bPackTexture) {}

const std::string TexturePackingVisitor::Filename = "osgGisPlugins-filename";
const std::string TexturePackingVisitor::ExtensionName = "osgGisPlugins-KHR_texture_transform";
const std::string TexturePackingVisitor::ExtensionOffsetX = "osgGisPlugins-offsetX";
const std::string TexturePackingVisitor::ExtensionOffsetY = "osgGisPlugins-offsetY";
const std::string TexturePackingVisitor::ExtensionScaleX = "osgGisPlugins-scaleX";
const std::string TexturePackingVisitor::ExtensionScaleY = "osgGisPlugins-scaleY";
const std::string TexturePackingVisitor::ExtensionTexCoord = "osgGisPlugins-texCoord";

const std::string TexturePackingVisitor::ExtensionNormalTextureName = "osgGisPlugins-normal-KHR_texture_transform";
const std::string TexturePackingVisitor::ExtensionNormalTextureOffsetX = "osgGisPlugins-normal-offsetX";
const std::string TexturePackingVisitor::ExtensionNormalTextureOffsetY = "osgGisPlugins-normal-offsetY";
const std::string TexturePackingVisitor::ExtensionNormalTextureScaleX = "osgGisPlugins-normal-scaleX";
const std::string TexturePackingVisitor::ExtensionNormalTextureScaleY = "osgGisPlugins-normal-scaleY";
const std::string TexturePackingVisitor::ExtensionNormalTextureTexCoord = "osgGisPlugins-normal-texCoord";

const std::string TexturePackingVisitor::ExtensionOcclusionTextureName = "osgGisPlugins-occlusion-KHR_texture_transform";
const std::string TexturePackingVisitor::ExtensionOcclusionTextureOffsetX = "osgGisPlugins-occlusion-offsetX";
const std::string TexturePackingVisitor::ExtensionOcclusionTextureOffsetY = "osgGisPlugins-occlusion-offsetY";
const std::string TexturePackingVisitor::ExtensionOcclusionTextureScaleX = "osgGisPlugins-occlusion-scaleX";
const std::string TexturePackingVisitor::ExtensionOcclusionTextureScaleY = "osgGisPlugins-occlusion-scaleY";
const std::string TexturePackingVisitor::ExtensionOcclusionTextureTexCoord = "osgGisPlugins-occlusion-texCoord";

const std::string TexturePackingVisitor::ExtensionEmissiveTextureName = "osgGisPlugins-emissive-KHR_texture_transform";
const std::string TexturePackingVisitor::ExtensionEmissiveTextureOffsetX = "osgGisPlugins-emissive-offsetX";
const std::string TexturePackingVisitor::ExtensionEmissiveTextureOffsetY = "osgGisPlugins-emissive-offsetY";
const std::string TexturePackingVisitor::ExtensionEmissiveTextureScaleX = "osgGisPlugins-emissive-scaleX";
const std::string TexturePackingVisitor::ExtensionEmissiveTextureScaleY = "osgGisPlugins-emissive-scaleY";
const std::string TexturePackingVisitor::ExtensionEmissiveTextureTexCoord = "osgGisPlugins-emissive-texCoord";

const std::string TexturePackingVisitor::ExtensionMRTextureName = "osgGisPlugins-MR-KHR_texture_transform";
const std::string TexturePackingVisitor::ExtensionMRTextureOffsetX = "osgGisPlugins-MR-offsetX";
const std::string TexturePackingVisitor::ExtensionMRTextureOffsetY = "osgGisPlugins-MR-offsetY";
const std::string TexturePackingVisitor::ExtensionMRTextureScaleX = "osgGisPlugins-MR-scaleX";
const std::string TexturePackingVisitor::ExtensionMRTextureScaleY = "osgGisPlugins-MR-scaleY";
const std::string TexturePackingVisitor::ExtensionMRTexCoord = "osgGisPlugins-MR-texCoord";

const std::string TexturePackingVisitor::ExtensionDiffuseTextureName = "osgGisPlugins-diffuse-KHR_texture_transform";
const std::string TexturePackingVisitor::ExtensionDiffuseTextureOffsetX = "osgGisPlugins-diffuse-offsetX";
const std::string TexturePackingVisitor::ExtensionDiffuseTextureOffsetY = "osgGisPlugins-diffuse-offsetY";
const std::string TexturePackingVisitor::ExtensionDiffuseTextureScaleX = "osgGisPlugins-diffuse-scaleX";
const std::string TexturePackingVisitor::ExtensionDiffuseTextureScaleY = "osgGisPlugins-diffuse-scaleY";
const std::string TexturePackingVisitor::ExtensionDiffuseTexCoord = "osgGisPlugins-diffuse-texCoord";

const std::string TexturePackingVisitor::ExtensionSGTextureName = "osgGisPlugins-SG-KHR_texture_transform";
const std::string TexturePackingVisitor::ExtensionSGTextureOffsetX = "osgGisPlugins-SG-offsetX";
const std::string TexturePackingVisitor::ExtensionSGTextureOffsetY = "osgGisPlugins-SG-offsetY";
const std::string TexturePackingVisitor::ExtensionSGTextureScaleX = "osgGisPlugins-SG-scaleX";
const std::string TexturePackingVisitor::ExtensionSGTextureScaleY = "osgGisPlugins-SG-scaleY";
const std::string TexturePackingVisitor::ExtensionSGTexCoord = "osgGisPlugins-SG-texCoord";

void TexturePackingVisitor::apply(osg::Drawable& drawable)
{
    const osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
    osg::ref_ptr<osg::StateSet> stateSet = geom->getStateSet();
    if (stateSet.valid())
    {
        const osg::ref_ptr<osg::Material> osgMaterial = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
        if (osgMaterial.valid()) {
            const std::type_info& materialId = typeid(*osgMaterial.get());
            if (materialId == typeid(GltfMaterial) || materialId == typeid(GltfPbrMRMaterial) || materialId == typeid(GltfPbrSGMaterial)) {
                osg::ref_ptr<GltfMaterial> gltfMaterial = dynamic_cast<GltfMaterial*>(osgMaterial.get());
                optimizeOsgMaterial(gltfMaterial, geom);
            }
            else {
                optimizeOsgTexture(stateSet, geom);
            }
        }
        else {
            optimizeOsgTexture(stateSet, geom);
        }
    }
}

void TexturePackingVisitor::packTextures()
{
    packOsgTextures();
    packOsgMaterials();
}

void TexturePackingVisitor::optimizeOsgTexture(const osg::ref_ptr<osg::StateSet>& stateSet, const osg::ref_ptr<osg::Geometry>& geom)
{
    osg::ref_ptr<osg::Texture2D> texture = dynamic_cast<osg::Texture2D*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
    osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
    if (texCoords.valid() && texture.valid()) {
        osg::ref_ptr<osg::Image> image = texture->getImage(0);
        if (image.valid()) {
            resizeImageToPowerOfTwo(image);
            if (_bPackTexture) {
                bool bBuildTexturePacker = true;
                for (auto texCoord : *texCoords.get()) {
                    if (std::fabs(texCoord.x()) - 1.0 > 0.001 || std::fabs(texCoord.y()) - 1.0 > 0.001) {
                        bBuildTexturePacker = false;
                        break;
                    }
                }
                if (bBuildTexturePacker)
                {
                    if (image->s() < _maxWidth || image->t() < _maxHeight)
                    {
                        bool bAdd = true;
                        for (auto img : _images) {
                            if (img->getFileName() == image->getFileName()) {
                                _geometryImgMap[geom] = img;
                                bAdd = false;
                            }
                        }
                        if (bAdd) {
                            _images.push_back(image);
                            _geometryImgMap[geom] = image;
                        }
                    }
                    else
                    {
                        std::string name;
                        image->getUserValue(Filename, name);
                        if (name.empty()) {
                            exportImage(image);
                        }
                    }
                }
                else {
                    std::string name;
                    image->getUserValue(Filename, name);
                    if (name.empty()) {
                        exportImage(image);
                    }
                }
            }
            else {
                std::string name;
                image->getUserValue(Filename, name);
                if (name.empty()) {
                    exportImage(image);
                }
            }
        }
    }
}

void TexturePackingVisitor::optimizeOsgTextureSize(osg::ref_ptr<osg::Texture2D> texture)
{
    if (texture.valid()) {
        if (texture->getNumImages()) {
            osg::ref_ptr<osg::Image> image = texture->getImage(0);
            if (image.valid()) {
                resizeImageToPowerOfTwo(image);
            }
        }
    }
}

void TexturePackingVisitor::exportOsgTexture(osg::ref_ptr<osg::Texture2D> texture)
{
    if (texture.valid()) {
        if (texture->getNumImages()) {
            osg::ref_ptr<osg::Image> image = texture->getImage(0);
            if (image.valid()) {
                std::string name;
                image->getUserValue(Filename, name);
                if (name.empty()) {
                    exportImage(image);
                }
            }
        }
    }
}

void TexturePackingVisitor::optimizeOsgMaterial(const osg::ref_ptr<GltfMaterial>& gltfMaterial, const osg::ref_ptr<osg::Geometry>& geom)
{
    if (gltfMaterial.valid()) {
        osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
        if (texCoords.valid()) {
            optimizeOsgTextureSize(gltfMaterial->normalTexture);
            optimizeOsgTextureSize(gltfMaterial->occlusionTexture);
            optimizeOsgTextureSize(gltfMaterial->emissiveTexture);
            osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
            if (gltfMRMaterial.valid()) {
                optimizeOsgTextureSize(gltfMRMaterial->metallicRoughnessTexture);
                optimizeOsgTextureSize(gltfMRMaterial->baseColorTexture);
            }
            for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i) {
                GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
                if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness)) {
                    KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
                    optimizeOsgTextureSize(pbrSpecularGlossiness_extension->osgDiffuseTexture);
                    optimizeOsgTextureSize(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture);
                }
            }

            if (_bPackTexture) {
                bool bBuildTexturePacker = true;
                for (auto texCoord : *texCoords.get()) {
                    if (std::fabs(texCoord.x()) - 1.0 > 0.001 || std::fabs(texCoord.y()) - 1.0 > 0.001) {
                        bBuildTexturePacker = false;
                        break;
                    }
                }
                if (bBuildTexturePacker) {
                    bool bAdd = true;
                    for (auto* item : _gltfMaterials) {
                        if (item == gltfMaterial.get()) {
                            _geometryMatMap[geom] = item;
                            bAdd = false;
                        }
                    }
                    if (bAdd) {
                        _gltfMaterials.push_back(gltfMaterial.get());
                        _geometryMatMap[geom] = gltfMaterial.get();
                    }
                }
                else {
                    exportOsgTexture(gltfMaterial->normalTexture);
                    exportOsgTexture(gltfMaterial->occlusionTexture);
                    exportOsgTexture(gltfMaterial->emissiveTexture);
                    osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
                    if (gltfMRMaterial.valid()) {
                        exportOsgTexture(gltfMRMaterial->metallicRoughnessTexture);
                        exportOsgTexture(gltfMRMaterial->baseColorTexture);
                    }
                    for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i) {
                        GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
                        if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness)) {
                            KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
                            exportOsgTexture(pbrSpecularGlossiness_extension->osgDiffuseTexture);
                            exportOsgTexture(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture);
                        }
                    }
                }
            }
            else {
                exportOsgTexture(gltfMaterial->normalTexture);
                exportOsgTexture(gltfMaterial->occlusionTexture);
                exportOsgTexture(gltfMaterial->emissiveTexture);
                osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
                if (gltfMRMaterial.valid()) {
                    exportOsgTexture(gltfMRMaterial->metallicRoughnessTexture);
                    exportOsgTexture(gltfMRMaterial->baseColorTexture);
                }
                for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i) {
                    GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
                    if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness)) {
                        KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
                        exportOsgTexture(pbrSpecularGlossiness_extension->osgDiffuseTexture);
                        exportOsgTexture(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture);
                    }
                }
            }
        }
    }
}

void TexturePackingVisitor::packOsgTextures()
{
    if (_images.empty()) return;
    while (_images.size())
    {
        TexturePacker packer(_maxWidth, _maxHeight);
        std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
        osg::ref_ptr<osg::Image> packedImage = packImges(packer, _images, deleteImgs);

        if (packedImage.valid() && deleteImgs.size())
        {
            const int width = packedImage->s(), height = packedImage->t();
            bool bResizeImage = resizeImageToPowerOfTwo(packedImage);
            exportImage(packedImage);
            for (auto& entry : _geometryImgMap)
            {
                osg::Geometry* geometry = entry.first;
                osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
                osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_ALL);

                osg::ref_ptr<osg::Texture2D> oldTexture = dynamic_cast<osg::Texture2D*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
                osg::ref_ptr<osg::Texture2D> packedTexture = dynamic_cast<osg::Texture2D*>(stateSetCopy->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

                packedTexture->setImage(packedImage);
                osg::Image* image = entry.second;
                int w, h;
                double x, y;
                const size_t id = packer.getId(image);
                if (packer.getPackingData(id, x, y, w, h))
                {
                    packedTexture->setUserValue(ExtensionName, true);
                    double offsetX = x / width;
                    packedTexture->setUserValue(ExtensionOffsetX, clamp(offsetX, 0.0, 1.0));
                    double offsetY = y / height;
                    packedTexture->setUserValue(ExtensionOffsetY, clamp(offsetY, 0.0, 1.0));
                    double scaleX = static_cast<double>(w) / width;
                    packedTexture->setUserValue(ExtensionScaleX, clamp(scaleX, 0.0, 1.0));
                    double scaleY = static_cast<double>(h) / height;
                    packedTexture->setUserValue(ExtensionScaleY, clamp(scaleY, 0.0, 1.0));
                    packedTexture->setUserValue(ExtensionTexCoord, 0);
                    stateSetCopy->setTextureAttribute(0, packedTexture.get());
                    geometry->setStateSet(stateSetCopy.get());
                }
            }
            removePackedImages(_images, deleteImgs);
        }

    }
}

osg::ref_ptr<osg::Image> TexturePackingVisitor::packImges(TexturePacker& packer, std::vector<osg::Image*>& imgs, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs) {
    int area = _maxWidth * _maxHeight;
    std::sort(imgs.begin(), imgs.end(), TexturePackingVisitor::compareImageHeight);
    std::vector<size_t> indexPacks;
    osg::ref_ptr<osg::Image> packedImage;

    for (size_t i = 0; i < imgs.size(); ++i) {
        osg::ref_ptr<osg::Image> img = imgs.at(i);
        if (area >= img->s() * img->t()) {
            if (deleteImgs.size()) {
                if (deleteImgs.at(0)->getInternalTextureFormat() == img->getInternalTextureFormat() &&
                    deleteImgs.at(0)->getPixelFormat() == img->getPixelFormat()) {
                    indexPacks.push_back(packer.addElement(img));
                    area -= img->s() * img->t();
                    deleteImgs.push_back(img);
                }
            }
            else {
                indexPacks.push_back(packer.addElement(img));
                area -= img->s() * img->t();
                deleteImgs.push_back(img);
            }
            if (i == (imgs.size() - 1)) {
                packImages(packedImage, indexPacks, deleteImgs, packer);
            }
        }
        else {
            packImages(packedImage, indexPacks, deleteImgs, packer);
            break;
        }
    }

    return packedImage;
}

void TexturePackingVisitor::updateGltfMaterialUserValue(
    osg::Geometry* geometry,
    osg::ref_ptr<osg::StateSet>& stateSetCopy,
    osg::ref_ptr<GltfMaterial> packedGltfMaterial,
    TexturePacker& packer,
    const int width,
    const int height,
    const osg::ref_ptr<osg::Texture2D>& oldTexture,
    const GltfTextureType type) {

    std::string textureNameExtension, textureOffsetXExtension, textureOffsetYExtension, textureScaleXExtension, textureScaleYExtension, textureTecCoord;
    switch (type)
    {
    case GltfTextureType::NORMAL:
        textureNameExtension = ExtensionNormalTextureName;
        textureOffsetXExtension = ExtensionNormalTextureOffsetX;
        textureOffsetYExtension = ExtensionNormalTextureOffsetY;
        textureScaleXExtension = ExtensionNormalTextureScaleX;
        textureScaleYExtension = ExtensionNormalTextureScaleY;
        textureTecCoord = ExtensionNormalTextureTexCoord;
        break;
    case GltfTextureType::OCCLUSION:
        textureNameExtension = ExtensionOcclusionTextureName;
        textureOffsetXExtension = ExtensionOcclusionTextureOffsetX;
        textureOffsetYExtension = ExtensionOcclusionTextureOffsetY;
        textureScaleXExtension = ExtensionOcclusionTextureScaleX;
        textureScaleYExtension = ExtensionOcclusionTextureScaleY;
        textureTecCoord = ExtensionOcclusionTextureTexCoord;
        break;
    case GltfTextureType::EMISSIVE:
        textureNameExtension = ExtensionEmissiveTextureName;
        textureOffsetXExtension = ExtensionEmissiveTextureOffsetX;
        textureOffsetYExtension = ExtensionEmissiveTextureOffsetY;
        textureScaleXExtension = ExtensionEmissiveTextureScaleX;
        textureScaleYExtension = ExtensionEmissiveTextureScaleY;
        textureTecCoord = ExtensionEmissiveTextureTexCoord;
        break;
    case GltfTextureType::METALLICROUGHNESS:
        textureNameExtension = ExtensionMRTextureName;
        textureOffsetXExtension = ExtensionMRTextureOffsetX;
        textureOffsetYExtension = ExtensionMRTextureOffsetY;
        textureScaleXExtension = ExtensionMRTextureScaleX;
        textureScaleYExtension = ExtensionMRTextureScaleY;
        textureTecCoord = ExtensionMRTexCoord;
        break;
    case GltfTextureType::BASECOLOR:
        textureNameExtension = ExtensionName;
        textureOffsetXExtension = ExtensionOffsetX;
        textureOffsetYExtension = ExtensionOffsetY;
        textureScaleXExtension = ExtensionScaleX;
        textureScaleYExtension = ExtensionScaleY;
        textureTecCoord = ExtensionTexCoord;
        break;
    case GltfTextureType::DIFFUSE:
        textureNameExtension = ExtensionDiffuseTextureName;
        textureOffsetXExtension = ExtensionDiffuseTextureOffsetX;
        textureOffsetYExtension = ExtensionDiffuseTextureOffsetY;
        textureScaleXExtension = ExtensionDiffuseTextureScaleX;
        textureScaleYExtension = ExtensionDiffuseTextureScaleY;
        textureTecCoord = ExtensionDiffuseTexCoord;
        break;
    case GltfTextureType::SPECULARGLOSSINESS:
        textureNameExtension = ExtensionSGTextureName;
        textureOffsetXExtension = ExtensionSGTextureOffsetX;
        textureOffsetYExtension = ExtensionSGTextureOffsetY;
        textureScaleXExtension = ExtensionSGTextureScaleX;
        textureScaleYExtension = ExtensionSGTextureScaleY;
        textureTecCoord = ExtensionSGTexCoord;
        break;
    default:
        break;
    }

    int w, h;
    double x, y;
    const size_t id = packer.getId(oldTexture->getImage(0));
    if (packer.getPackingData(id, x, y, w, h))
    {
        packedGltfMaterial->setUserValue(textureNameExtension, true);
        double offsetX = x / width;
        packedGltfMaterial->setUserValue(textureOffsetXExtension, clamp(offsetX, 0.0, 1.0));
        double offsetY = y / height;
        packedGltfMaterial->setUserValue(textureOffsetYExtension, clamp(offsetY, 0.0, 1.0));
        double scaleX = static_cast<double>(w) / width;
        packedGltfMaterial->setUserValue(textureScaleXExtension, clamp(scaleX, 0.0, 1.0));
        double scaleY = static_cast<double>(h) / height;
        packedGltfMaterial->setUserValue(textureScaleYExtension, clamp(scaleY, 0.0, 1.0));
        packedGltfMaterial->setUserValue(textureTecCoord, 0);
        stateSetCopy->setAttribute(packedGltfMaterial.get(), osg::StateAttribute::MATERIAL);
        geometry->setStateSet(stateSetCopy.get());
    }
}

void TexturePackingVisitor::processGltfGeneralImages(std::vector<osg::Image*>& imgs, const GltfTextureType type)
{

    while (imgs.size())
    {
        TexturePacker packer(_maxWidth, _maxHeight);
        std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
        osg::ref_ptr<osg::Image> packedImage = packImges(packer, imgs, deleteImgs);

        if (packedImage.valid() && deleteImgs.size())
        {
            const int width = packedImage->s(), height = packedImage->t();
            bool bResizeImage = resizeImageToPowerOfTwo(packedImage);
            exportImage(packedImage);
            for (auto& entry : _geometryMatMap)
            {
                osg::Geometry* geometry = entry.first;
                osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
                osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_ALL);

                osg::ref_ptr<GltfMaterial> olgMaterial = dynamic_cast<GltfMaterial*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
                osg::ref_ptr<GltfMaterial> packedGltfMaterial = dynamic_cast<GltfMaterial*>(stateSetCopy->getAttribute(osg::StateAttribute::MATERIAL));

                osg::ref_ptr<osg::Texture2D> packedTexture, oldTexture;
                if (type == GltfTextureType::NORMAL) {
                    packedTexture = packedGltfMaterial->normalTexture;
                }
                else if (type == GltfTextureType::OCCLUSION) {
                    packedTexture = packedGltfMaterial->occlusionTexture;
                }
                else if (type == GltfTextureType::EMISSIVE) {
                    packedTexture = packedGltfMaterial->emissiveTexture;
                }

                if (packedTexture.valid() && packedTexture->getNumImages()) {
                    GltfMaterial* gltfMaterial = entry.second;
                    if (olgMaterial.get() == gltfMaterial) {
                        packedTexture->setImage(packedImage);
                        if (type == GltfTextureType::NORMAL) {
                            oldTexture = gltfMaterial->normalTexture;
                        }
                        else if (type == GltfTextureType::OCCLUSION) {
                            oldTexture = gltfMaterial->occlusionTexture;
                        }
                        else if (type == GltfTextureType::EMISSIVE) {
                            oldTexture = gltfMaterial->emissiveTexture;
                        }
                        if (oldTexture.valid() && oldTexture->getNumImages()) {
                            updateGltfMaterialUserValue(geometry, stateSetCopy, packedGltfMaterial, packer, width, height, oldTexture, type);
                        }
                    }
                }
            }
            removePackedImages(imgs, deleteImgs);
        }
    }
}

void TexturePackingVisitor::addImageFromTexture(const osg::ref_ptr<osg::Texture2D>& texture, std::vector<osg::Image*>& imgs) {
    if (texture.valid()) {
        if (texture->getNumImages()) {
            bool bAdd = true;
            osg::ref_ptr<osg::Image> normalImg = texture->getImage(0);
            for (auto* img : imgs) {
                if (img == normalImg)
                    bAdd = false;
            }
            if (bAdd)
                imgs.push_back(normalImg);
        }
    }
}

void TexturePackingVisitor::removePackedImages(std::vector<osg::Image*>& imgs, const std::vector<osg::ref_ptr<osg::Image>>& deleteImgs) {
    for (osg::ref_ptr<osg::Image> img : deleteImgs) {
        auto it = std::remove(imgs.begin(), imgs.end(), img.get());
        imgs.erase(it, imgs.end());
    }
}

void TexturePackingVisitor::processGltfPbrMRImages(std::vector<osg::Image*>& imageList, const GltfTextureType type)
{
    while (imageList.size())
    {
        TexturePacker packer(_maxWidth, _maxHeight);
        std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
        osg::ref_ptr<osg::Image> packedImage = packImges(packer, imageList, deleteImgs);

        if (packedImage.valid() && deleteImgs.size())
        {
            const int width = packedImage->s(), height = packedImage->t();
            bool bResizeImage = resizeImageToPowerOfTwo(packedImage);
            exportImage(packedImage);

            for (auto& entry : _geometryMatMap)
            {
                osg::Geometry* geometry = entry.first;
                osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
                osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_ALL);

                osg::ref_ptr<GltfPbrMRMaterial> olgMaterial = dynamic_cast<GltfPbrMRMaterial*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
                osg::ref_ptr<GltfPbrMRMaterial> packedGltfMaterial = dynamic_cast<GltfPbrMRMaterial*>(stateSetCopy->getAttribute(osg::StateAttribute::MATERIAL));

                osg::ref_ptr<osg::Texture2D> texture = (type == GltfTextureType::METALLICROUGHNESS) ? packedGltfMaterial->metallicRoughnessTexture : packedGltfMaterial->baseColorTexture;

                if (texture.valid() && texture->getNumImages())
                {
                    GltfMaterial* gltfMaterial = entry.second;
                    if (olgMaterial.get() == gltfMaterial)
                    {
                        GltfPbrMRMaterial* gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial);
                        texture->setImage(packedImage);
                        osg::ref_ptr<osg::Texture2D> originalTexture = (type == GltfTextureType::METALLICROUGHNESS) ? gltfMRMaterial->metallicRoughnessTexture : gltfMRMaterial->baseColorTexture;

                        if (originalTexture.valid() && originalTexture->getNumImages())
                        {
                            updateGltfMaterialUserValue(geometry, stateSetCopy, packedGltfMaterial, packer, width, height, originalTexture, type);
                        }
                    }
                }
            }
            removePackedImages(imageList, deleteImgs);
        }
    }
}

void TexturePackingVisitor::processGltfPbrSGImages(std::vector<osg::Image*>& images, const GltfTextureType type)
{
    while (images.size())
    {
        TexturePacker packer(_maxWidth, _maxHeight);
        std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
        osg::ref_ptr<osg::Image> packedImage = packImges(packer, images, deleteImgs);

        if (packedImage.valid() && deleteImgs.size())
        {
            const int width = packedImage->s(), height = packedImage->t();
            bool bResizeImage = resizeImageToPowerOfTwo(packedImage);
            exportImage(packedImage);
            for (auto& entry : _geometryMatMap)
            {
                osg::Geometry* geometry = entry.first;
                osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
                osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_ALL);

                osg::ref_ptr<GltfMaterial> olgMaterial = dynamic_cast<GltfMaterial*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
                osg::ref_ptr<GltfMaterial> packedGltfMaterial = dynamic_cast<GltfMaterial*>(stateSetCopy->getAttribute(osg::StateAttribute::MATERIAL));
                GltfMaterial* gltfMaterial = entry.second;
                if (olgMaterial.get() == gltfMaterial) {
                    for (size_t i = 0; i < packedGltfMaterial->materialExtensionsByCesiumSupport.size(); ++i) {
                        GltfExtension* packedExtension = packedGltfMaterial->materialExtensionsByCesiumSupport.at(i);
                        for (size_t j = 0; j < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++j) {
                            GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(j);
                            if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness) && typeid(*packedExtension) == typeid(KHR_materials_pbrSpecularGlossiness)) {

                                KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
                                KHR_materials_pbrSpecularGlossiness* packed_pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(packedExtension);

                                osg::ref_ptr<osg::Texture2D> osgTexture = (type == GltfTextureType::DIFFUSE) ? pbrSpecularGlossiness_extension->osgDiffuseTexture : pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture;
                                osg::ref_ptr<osg::Texture2D> packedOsgTexture = (type == GltfTextureType::DIFFUSE) ? packed_pbrSpecularGlossiness_extension->osgDiffuseTexture : packed_pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture;

                                if (osgTexture.valid() && packedOsgTexture.valid()) {
                                    if (osgTexture->getNumImages() && packedOsgTexture->getNumImages()) {
                                        packedOsgTexture->setImage(packedImage);

                                        updateGltfMaterialUserValue(geometry, stateSetCopy, packedGltfMaterial, packer, width, height, osgTexture, type);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            removePackedImages(images, deleteImgs);
        }
    }
}

void TexturePackingVisitor::packOsgMaterials()
{
    if (_gltfMaterials.empty()) return;
    std::vector<osg::Image*> normalImgs, occlusionImgs, emissiveImgs, mrImgs, baseColorImgs, diffuseImgs, sgImgs;
    for (auto* material : _gltfMaterials) {
        addImageFromTexture(material->normalTexture, normalImgs);
        addImageFromTexture(material->occlusionTexture, occlusionImgs);
        addImageFromTexture(material->emissiveTexture, emissiveImgs);

        osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(material);
        if (gltfMRMaterial.valid()) {
            addImageFromTexture(gltfMRMaterial->metallicRoughnessTexture, mrImgs);
            addImageFromTexture(gltfMRMaterial->baseColorTexture, baseColorImgs);
        }
        for (size_t i = 0; i < material->materialExtensionsByCesiumSupport.size(); ++i) {
            GltfExtension* extension = material->materialExtensionsByCesiumSupport.at(i);
            if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness)) {
                KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
                addImageFromTexture(pbrSpecularGlossiness_extension->osgDiffuseTexture, diffuseImgs);
                addImageFromTexture(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture, sgImgs);
            }
        }
    }

    processGltfGeneralImages(normalImgs, GltfTextureType::NORMAL);
    processGltfGeneralImages(occlusionImgs, GltfTextureType::OCCLUSION);
    processGltfGeneralImages(emissiveImgs, GltfTextureType::EMISSIVE);

    processGltfPbrMRImages(mrImgs, GltfTextureType::METALLICROUGHNESS);
    processGltfPbrMRImages(baseColorImgs, GltfTextureType::BASECOLOR);

    processGltfPbrSGImages(diffuseImgs, GltfTextureType::DIFFUSE);

    processGltfPbrSGImages(diffuseImgs, GltfTextureType::SPECULARGLOSSINESS);
}

void TexturePackingVisitor::packImages(osg::ref_ptr<osg::Image>& img, std::vector<size_t>& indexes, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs, TexturePacker& packer)
{
    size_t numImages;
    img = packer.pack(numImages, true);
    if (!img.valid() || numImages != indexes.size()) {
        img = NULL;
        packer.removeElement(indexes.back());
        indexes.pop_back();
        deleteImgs.pop_back();
        packImages(img, indexes, deleteImgs, packer);
    }
}

void TexturePackingVisitor::exportImage(const osg::ref_ptr<osg::Image>& img)
{
    std::string ext = _ext;
    std::string filename = computeImageHash(img);
    const GLenum pixelFormat = img->getPixelFormat();
    if (_ext == ".jpg" && pixelFormat != GL_ALPHA && pixelFormat != GL_RGB) {
        ext = ".png";
    }
    std::string fullPath = _cachePath + "/" + filename + ext;
    bool isFileExists = osgDB::fileExists(fullPath);
    if (!isFileExists)
    {
        osg::ref_ptr< osg::Image > flipped = new osg::Image(*img);
        if (!(osgDB::writeImageFile(*flipped.get(), fullPath))) {
            fullPath = _cachePath + "/" + filename + ".png";
            std::ifstream fileExistedPng(fullPath);
            if ((!fileExistedPng.good()) || (fileExistedPng.peek() == std::ifstream::traits_type::eof()))
            {
                if (!(osgDB::writeImageFile(*flipped.get(), fullPath))) {
                    osg::notify(osg::FATAL) << '\n';
                }
            }
            fileExistedPng.close();
        }
    }
    img->setUserValue(Filename, fullPath);
}

bool TexturePackingVisitor::resizeImageToPowerOfTwo(const osg::ref_ptr<osg::Image>& img)
{
    int originalWidth = img->s();
    int originalHeight = img->t();
    int newWidth = findNearestPowerOfTwo(originalWidth);
    int newHeight = findNearestPowerOfTwo(originalHeight);

    if (!(originalWidth == newWidth && originalHeight == newHeight))
    {
        newWidth = newWidth > _maxWidth ? _maxWidth : newWidth;
        newHeight = newHeight > _maxHeight ? _maxHeight : newHeight;
        img->scaleImage(newWidth, newHeight, img->r());
        return true;
    }
    return false;
}

int TexturePackingVisitor::findNearestPowerOfTwo(int value)
{
    int powerOfTwo = 1;
    while (powerOfTwo * 2 <= value) {
        powerOfTwo *= 2;
    }
    return powerOfTwo;
}

std::string TexturePackingVisitor::computeImageHash(const osg::ref_ptr<osg::Image>& image)
{
    size_t hash = 0;
    auto hash_combine = [&hash](size_t value) {
        hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        };

    hash_combine(std::hash<int>()(image->s()));
    hash_combine(std::hash<int>()(image->t()));
    hash_combine(std::hash<int>()(image->r()));
    hash_combine(std::hash<int>()(image->getPixelFormat()));
    hash_combine(std::hash<int>()(image->getDataType()));

    const unsigned char* data = image->data();
    size_t dataSize = image->getImageSizeInBytes();

    for (size_t i = 0; i < dataSize; ++i)
    {
        hash_combine(std::hash<unsigned char>()(data[i]));
    }
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

bool TexturePackingVisitor::compareImageHeight(osg::Image* img1, osg::Image* img2)
{
    if (img1->t() == img2->t()) {
        return img1->s() > img2->s();
    }
    return img1->t() > img2->t();
}
