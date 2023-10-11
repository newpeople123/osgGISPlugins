#ifndef OSGDB_UTILS_GLTF_MATERIAL_H
#define OSGDB_UTILS_GLTF_MATERIAL_H 1
#include <osg/Material>
#include <osg/Texture2D>
#include <array>
class GltfMaterial:public osg::Material
{
public:
    osg::ref_ptr<osg::Texture2D> normalTexture;
    osg::ref_ptr<osg::Texture2D> occlusionTexture;
    osg::ref_ptr<osg::Texture2D> emissiveTexture;
    std::array<double, 3> emissiveFactor{ 0.0 };
#pragma region KHR_materials_clearcoat
    bool enable_KHR_materials_clearcoat = false;
    double clearcoatFactor = 0.0;
    double clearcoatRoughnessFactor = 0.0;
    osg::ref_ptr<osg::Texture2D> clearcoatTexture;
    osg::ref_ptr<osg::Texture2D> clearcoatRoughnessTexture;
    osg::ref_ptr<osg::Texture2D> clearcoatNormalTexture;
#pragma endregion
#pragma region KHR_materials_anisotropy 
    bool enable_KHR_materials_anisotropy = false;
    double anisotropyStrength = 0.0;
    double anisotropyRotation = 0.0;
    osg::ref_ptr<osg::Texture2D> anisotropyTexture;
#pragma endregion
#pragma region KHR_materials_emissive_strength(conflict KHR_materials_unlit)
    bool enable_KHR_materials_emissive_strength = false;
    double emissiveStrength = 1.0;
#pragma endregion
};
#endif //!OSGDB_UTILS_GLTF_MATERIAL_H
