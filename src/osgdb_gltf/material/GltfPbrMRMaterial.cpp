#include "osgdb_gltf/material/GltfPbrMRMaterial.h"
using namespace osgGISPlugins;
osg::ref_ptr<osg::Image> GltfPbrMRMaterial::mergeImages(const osg::ref_ptr<osg::Image>& metalnessImage, const osg::ref_ptr<osg::Image>& roughnessImage)
{
    const int width = metalnessImage->s();
    const int height = metalnessImage->t();

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

