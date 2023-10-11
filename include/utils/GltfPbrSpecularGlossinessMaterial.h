#pragma once
#ifndef OSGDB_UTILS_GLTF_SG_MATERIAL
#define OSGDB_UTILS_GLTF_SG_MATERIAL 1

#include "GltfMaterial.h"
class GltfPbrSpecularGlossinessMaterial :public GltfMaterial {
public:
    osg::ref_ptr<osg::Texture2D> diffuseTexture;
    osg::ref_ptr<osg::Texture2D> specularGlossinessTexture;

    std::array<double, 4> diffuseFactor{ 1.0 };
    std::array<double, 3> specularFactor{ 1.0 };

    double glossinessFactor = 1.0;
    osg::ref_ptr<osg::Image> mergeImagesToSpecularGlossinessImage(const osg::ref_ptr<osg::Image>& specularImage, const osg::ref_ptr<osg::Image>& glossinessImage) {
        int width = specularImage->s();
        int height = specularImage->t();

        osg::ref_ptr<osg::Image> specularGlossinessImage = new osg::Image;
        specularGlossinessImage->allocateImage(width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                osg::Vec4f specularPixel = specularImage->getColor(x, y);
                osg::Vec4f glossinessPixel = specularImage->getColor(x, y);

                osg::Vec4f combinedPixel(specularPixel.r(), specularPixel.g(), specularPixel.b(), glossinessPixel.a());
                specularGlossinessImage->setColor(combinedPixel, x, y);
            }
        }
        return specularGlossinessImage;
    }
};
#endif // !OSGDB_UTILS_GLTF_SG_MATERIAL
