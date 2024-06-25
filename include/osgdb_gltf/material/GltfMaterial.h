#ifndef OSG_GIS_PLUGINS_GLTF_MATERIAL_H
#define OSG_GIS_PLUGINS_GLTF_MATERIAL_H 1
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/Image>
#include <array>
#include "osgdb_gltf/Extensions.h"
class GltfMaterial :public osg::Material
{
public:
	osg::ref_ptr<osg::Texture2D> normalTexture;
	osg::ref_ptr<osg::Texture2D> occlusionTexture;
	osg::ref_ptr<osg::Texture2D> emissiveTexture;
	std::array<double, 3> emissiveFactor{ 0.0 };
	osg::MixinVector<GltfExtension*> materialExtensions;
	osg::MixinVector<GltfExtension*> materialExtensionsByCesiumSupport;
	virtual osg::ref_ptr<osg::Image> mergeImages(const osg::ref_ptr<osg::Image>& img1, const osg::ref_ptr<osg::Image>& img2) = 0;
	virtual ~GltfMaterial() = default;
	GltfMaterial(const GltfMaterial& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
		: osg::Material(other, copyop),
		emissiveFactor(other.emissiveFactor) {
		if (other.normalTexture.valid()) {
			normalTexture = (osg::Texture2D*)other.normalTexture->clone(copyop);
		}
		if (other.occlusionTexture.valid()) {
			occlusionTexture = (osg::Texture2D*)other.occlusionTexture->clone(copyop);
		}
		if (other.emissiveTexture.valid()) {
			emissiveTexture = (osg::Texture2D*)other.emissiveTexture->clone(copyop);
		}
		for (auto* item : other.materialExtensions) {
			GltfExtension* extension = item->clone();
			materialExtensions.push_back(extension);
		}
		for (auto* item : other.materialExtensionsByCesiumSupport) {
			GltfExtension* extension = item->clone();
			materialExtensionsByCesiumSupport.push_back(extension);
		}
	}
	GltfMaterial() {}
	//META_Object(osg, GltfMaterial);
};
#endif // !OSG_GIS_PLUGINS_GLTF_MATERIAL_H
