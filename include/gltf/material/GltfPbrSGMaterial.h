#ifndef OSG_GIS_PLUGINS_GLTF_PBR_SG_MATERIAL_H
#define OSG_GIS_PLUGINS_GLTF_PBR_SG_MATERIAL_H 1
#include "GltfMaterial.h"
class GltfPbrSGMaterial :public GltfMaterial {
public:
	osg::ref_ptr<osg::Image> mergeImages(const osg::ref_ptr<osg::Image>& specularImage, const osg::ref_ptr<osg::Image>& glossinessImage) override;

	// 通过 GltfMaterial 继承
	int convert2TinyGltfMaterial(tinygltf::Model& model, const osg::ref_ptr<osg::StateSet>& stateSet) override;
};

#endif // !OSG_GIS_PLUGINS_GLTF_PBR_SG_MATERIAL_H
