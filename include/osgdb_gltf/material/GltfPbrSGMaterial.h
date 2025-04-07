#ifndef OSG_GIS_PLUGINS_GLTF_PBR_SG_MATERIAL_H
#define OSG_GIS_PLUGINS_GLTF_PBR_SG_MATERIAL_H 1
#include "GltfMaterial.h"
namespace osgGISPlugins {
    class GltfPbrSGMaterial :public GltfMaterial {
    public:
        osg::ref_ptr<osg::Image> mergeImages(const osg::ref_ptr<osg::Image>& specularImage, const osg::ref_ptr<osg::Image>& glossinessImage) override;
        ~GltfPbrSGMaterial(){}
        GltfPbrSGMaterial(const GltfPbrSGMaterial& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
            : GltfMaterial(other, copyop){}
        GltfPbrSGMaterial() :GltfMaterial() {}

        META_Object(osg, GltfPbrSGMaterial);
    };
}
#endif // !OSG_GIS_PLUGINS_GLTF_PBR_SG_MATERIAL_H
