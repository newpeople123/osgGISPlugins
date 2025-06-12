#include "osgdb_gltf/material/GltfPbrMRMaterial.h"
using namespace osgGISPlugins;
osg::ref_ptr<osg::Image> GltfPbrMRMaterial::mergeImages(const osg::ref_ptr<osg::Image>& metalnessImage, const osg::ref_ptr<osg::Image>& roughnessImage)
{
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

osg::ref_ptr<osg::Image> GltfPbrMRMaterial::mergeImages(const osg::ref_ptr<osg::Image>& occlusionImage, const osg::ref_ptr<osg::Image>& metalnessImage, const osg::ref_ptr<osg::Image>& roughnessImage)
{
    int width = metalnessImage->s();
    int height = metalnessImage->t();

    osg::ref_ptr<osg::Image> occlusionMetallicRoughnessImage = new osg::Image;
    occlusionMetallicRoughnessImage->allocateImage(width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            osg::Vec4f occlusionPixel = occlusionImage->getColor(x, y);
            osg::Vec4f roughnessPixel = roughnessImage->getColor(x, y);
            osg::Vec4f metalnessPixel = metalnessImage->getColor(x, y);

            osg::Vec4f combinedPixel(occlusionPixel.r(), roughnessPixel.r(), metalnessPixel.r(), 1.0f);
            occlusionMetallicRoughnessImage->setColor(combinedPixel, x, y);
        }
    }
    return occlusionMetallicRoughnessImage;
}

osg::ref_ptr<osg::Image> GltfPbrMRMaterial::generateMetallicRoughnessMap(osg::Image* input) {
    if (!input) return nullptr;

    int width = input->s();
    int height = input->t();

    osg::ref_ptr<osg::Image> metallicRoughnessImage = new osg::Image;
    metallicRoughnessImage->allocateImage(width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned char* srcPixel = input->data(x, y);
            float r = srcPixel[0];
            float g = srcPixel[1];
            float b = srcPixel[2];

            // 计算感知亮度（常用公式：0.299R+0.587G+0.114B）
            float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
            // 计算粗糙度（亮 -> 光滑，暗 -> 粗糙），并截断到 [0,1]
            float roughness = osg::clampBetween(1.0f - luminance, 0.0f, 1.0f);

            // 连续金属度估算：若蓝色分量最高，则根据 b 与 max(r,g) 差值计算
            float metallic = 0.0f;
            if (b > r && b > g) {
                float M = std::max(r, g);
                if (M < 1.0f) { // 避免分母为0
                    metallic = (b - M) / (1.0f - M);
                }
            }
            metallic = osg::clampBetween(metallic, 0.0f, 1.0f);

            osg::Vec4f pixel(0.0f, roughness, metallic, 1.0f);
            metallicRoughnessImage->setColor(pixel, x, y);
        }
    }

    return metallicRoughnessImage;
}
