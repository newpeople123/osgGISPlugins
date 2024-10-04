#ifndef OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H
#define OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H 1
#include "GltfMaterial.h"
namespace osgGISPlugins {
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
        ~GltfPbrMRMaterial(){}

        META_Object(osg, GltfPbrMRMaterial);

        bool operator==(const GltfPbrMRMaterial& other)
        {
            if (!GltfMaterial::operator==(other))
            {
                return false;
            }
            if (!compareTexture2D(metallicRoughnessTexture, other.metallicRoughnessTexture))
            {
                return false;
            }
            if (!compareTexture2D(baseColorTexture, other.baseColorTexture))
            {
                return false;
            }
            for (size_t i = 0; i < 4; ++i)
            {
                if (baseColorFactor[i] != other.baseColorFactor[i])
                {
                    return false;
                }
            }
            if (metallicFactor != other.metallicFactor)
            {
                return false;
            }
            if (roughnessFactor != other.roughnessFactor)
            {
                return false;
            }
            return true;
        }
    };
}

#endif // !OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H
