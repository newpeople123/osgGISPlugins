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

    // 通过 GltfMaterial 继承
    int convert2TinyGltfMaterial(tinygltf::Model& model, const osg::ref_ptr<osg::StateSet>& stateSet) override;
};

#endif // !OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H
