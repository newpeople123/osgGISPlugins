#ifndef OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H
#define OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H 1
#include "GltfMaterial.h"
namespace osgGISPlugins {
    class GltfPbrMRMaterial :public GltfMaterial {
    public:
        osg::ref_ptr<osg::Texture2D> metallicRoughnessTexture;//G is roughtness，B is metallic
        osg::ref_ptr<osg::Texture2D> baseColorTexture;
        std::array<double, 4> baseColorFactor{ 1.0,1.0 ,1.0 ,1.0 };
        double metallicFactor = 0.0;
        double roughnessFactor = 0.5;
        osg::ref_ptr<osg::Image> mergeImages(const osg::ref_ptr<osg::Image>& metalnessImage, const osg::ref_ptr<osg::Image>& roughnessImage) override;
        osg::ref_ptr<osg::Image> mergeImages(const osg::ref_ptr<osg::Image>& occlusionImage, const osg::ref_ptr<osg::Image>& metalnessImage, const osg::ref_ptr<osg::Image>& roughnessImage) override;
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

        virtual bool compare(const GltfMaterial& other) const override
        {
            const GltfPbrMRMaterial* pbrMR = dynamic_cast<const GltfPbrMRMaterial*>(&other);
            if (!pbrMR) return false;

            // 基类比较
            if (!GltfMaterial::compare(*pbrMR)) return false;

            // PBR 特有属性比较
            if (!compareTexture2D(metallicRoughnessTexture, pbrMR->metallicRoughnessTexture))
                return false;
            if (!compareTexture2D(baseColorTexture, pbrMR->baseColorTexture))
                return false;

            // 因子比较
            if (!compareArray(baseColorFactor, pbrMR->baseColorFactor)) return false;
            if (!osg::equivalent(metallicFactor, pbrMR->metallicFactor)) return false;
            if (!osg::equivalent(roughnessFactor, pbrMR->roughnessFactor)) return false;

            return true;
        }

        static osg::ref_ptr<osg::Image> generateMetallicRoughnessMap(osg::Image* input);
    };
}

#endif // !OSG_GIS_PLUGINS_GLTF_PBR_MR_MATERIAL_H
