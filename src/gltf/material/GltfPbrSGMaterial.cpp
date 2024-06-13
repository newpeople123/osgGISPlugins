#include "gltf/material/GltfPbrSGMaterial.h"
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
osg::ref_ptr<osg::Image> GltfPbrSGMaterial::mergeImages(const osg::ref_ptr<osg::Image>& specularImage, const osg::ref_ptr<osg::Image>& glossinessImage)
{
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

