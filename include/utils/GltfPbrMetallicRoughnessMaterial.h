#pragma once
#ifndef OSGDB_UTILS_GLTF_PBR_MR_MATERIAL
#define OSGDB_UTILS_GLTF_PBR_MR_MATERIAL 

#include "GltfMaterial.h"

class GltfPbrMetallicRoughnessMaterial :public GltfMaterial {
public:
    osg::ref_ptr<osg::Texture2D> metallicRoughnessTexture;//G is roughtness，B is metallic
    osg::ref_ptr<osg::Texture2D> baseColorTexture;


    std::array<double,4> baseColorFactor{1.0};
    double metallicFactor = 1.0;
    double roughnessFactor = 1.0;

#pragma region KHR_materials_ior(conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness)
    bool enable_KHR_materials_ior = false;
    double ior = 1.5;
#pragma endregion
#pragma region KHR_materials_sheen(conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness)
    bool enable_KHR_materials_sheen = false;
    std::array<double, 3> sheenColorFactor{ 0.0 };
    osg::ref_ptr<osg::Texture2D> sheenColorTexture;
    double sheenRoughnessFactor = 0.0;
    osg::ref_ptr<osg::Texture2D> sheenRoughnessTexture;
#pragma endregion

#pragma region KHR_materials_volume(conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness)
    bool enable_KHR_materials_volume = false;
    double thicknessFactor = 0.0;
    osg::ref_ptr<osg::Texture2D> thicknessTexture;
    double attenuationDistance = DBL_MAX;
    std::array<double, 3> attenuationColor{ 1.0 };
#pragma endregion


#pragma region KHR_materials_specular (conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness)
    bool enable_KHR_materials_specular = false;
    double specularFactor = 1.0;
    osg::ref_ptr<osg::Texture2D> specularTexture;
    std::array<double, 3> specularColorFactor{ 1.0 };
    osg::ref_ptr<osg::Texture2D> specularColorTexture;
#pragma endregion

#pragma region KHR_materials_transmission
    bool enable_KHR_materials_transmission = false;
    osg::ref_ptr<osg::Texture2D> transmissionTexture;
    double transmissionFactor = 0.0;
#pragma endregion
    osg::ref_ptr<osg::Image> mergeImagesToMetallicRoughnessImage(const osg::ref_ptr<osg::Image>& metalnessImage, const osg::ref_ptr<osg::Image>& roughnessImage) {
        int width = metalnessImage->s();
        int height = metalnessImage->t();

        osg::ref_ptr<osg::Image> metallicRoughnessImage = new osg::Image;
        metallicRoughnessImage->allocateImage(width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                osg::Vec4f roughnessPixel = roughnessImage->getColor(x, y);
                osg::Vec4f metalnessPixel = metalnessImage->getColor(x, y);

                osg::Vec4f combinedPixel(0.0f, roughnessPixel.r(), metalnessPixel.r(), 1.0f);
                metallicRoughnessImage->setColor(combinedPixel, x, y);
            }
        }
        return metallicRoughnessImage;
    }
};

#endif // OSGDB_UTILS_GLTF_PBR_MR_MATERIAL
