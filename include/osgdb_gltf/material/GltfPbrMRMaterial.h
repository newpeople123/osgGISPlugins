#ifndef OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H
#define OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H 1
#include "GltfMaterial.h"
class GltfPbrMRMaterial :public GltfMaterial {
public:
    osg::ref_ptr<osg::Texture2D> metallicRoughnessTexture;//G is roughtness，B is metallic
    osg::ref_ptr<osg::Texture2D> baseColorTexture;
    std::array<double, 4> baseColorFactor{ 1.0,1.0 ,1.0 ,1.0 };
    double metallicFactor = 1.0;
    double roughnessFactor = 1.0;
    osg::ref_ptr<osg::Image> mergeImages(const osg::ref_ptr<osg::Image>& metalnessImage, const osg::ref_ptr<osg::Image>& roughnessImage) override;
    GltfPbrMRMaterial(const GltfPbrMRMaterial& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
        : GltfMaterial(other, copyop),
        baseColorFactor(other.baseColorFactor),
        metallicFactor(other.metallicFactor),
        roughnessFactor(other.roughnessFactor) {
        if (other.metallicRoughnessTexture.valid()) {
            metallicRoughnessTexture = (osg::Texture2D*)other.metallicRoughnessTexture->clone(copyop);
        }
        if (other.baseColorTexture.valid()) {
            baseColorTexture = (osg::Texture2D*)other.baseColorTexture->clone(copyop);
        }
    }
    GltfPbrMRMaterial() :GltfMaterial() {}
    META_Object(osg, GltfPbrMRMaterial);
};

#endif // !OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H
