#ifndef OSG_GIS_PLUGINS_GLTF_MATERIAL_H
#define OSG_GIS_PLUGINS_GLTF_MATERIAL_H 1
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/Image>
#include <array>
#include "osgdb_gltf/Extensions.h"
namespace osgGISPlugins {
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
		virtual osg::ref_ptr<osg::Image> mergeImages(const osg::ref_ptr<osg::Image>& img1, const osg::ref_ptr<osg::Image>& img2, const osg::ref_ptr<osg::Image>& img3) = 0;
		virtual ~GltfMaterial() {
			safeReleaseTexture(normalTexture);
			safeReleaseTexture(occlusionTexture);
			safeReleaseTexture(emissiveTexture);
		}
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
			//for (GltfExtension* item : other.materialExtensions) {
			//	GltfExtension* extension = item->clone();
			//	materialExtensions.push_back(extension);
			//}
			for (GltfExtension* item : other.materialExtensionsByCesiumSupport) {
				GltfExtension* extension = item->clone();
				materialExtensionsByCesiumSupport.push_back(extension);
			}
		}
		GltfMaterial() {}
		//META_Object(osg, GltfMaterial);

		static bool compareTexture2D(osg::ref_ptr<osg::Texture2D> texture1, osg::ref_ptr<osg::Texture2D> texture2);

		static void safeReleaseTexture(osg::ref_ptr<osg::Texture2D>& tex)
		{
			if (tex.valid())
			{
				if (tex->referenceCount() == 1)
					tex->setImage(0, nullptr);
				tex.release();
			}
		}

		virtual bool compare(const GltfMaterial& other) const
		{
			if (this == &other) return true;

			// 先比较基类 Material 的属性
			if (!osg::Material::compare(other)) return false;

			// 比较纹理
			if (!compareTexture2D(normalTexture, other.normalTexture)) return false;
			if (!compareTexture2D(occlusionTexture, other.occlusionTexture)) return false;
			if (!compareTexture2D(emissiveTexture, other.emissiveTexture)) return false;

			if (!compareArray(emissiveFactor, other.emissiveFactor)) return false;

			// 比较扩展
			if (!compareExtensions(materialExtensions, other.materialExtensions)) return false;
			if (!compareExtensions(materialExtensionsByCesiumSupport,
				other.materialExtensionsByCesiumSupport)) return false;

			return true;
		}

		static osg::ref_ptr<osg::Image> convertToGrayscale(osg::Image* input);

		static osg::ref_ptr<osg::Image> generateNormalMap(osg::Image* heightMap);
	protected:
		template<typename T, size_t N>
		static bool compareArray(const std::array<T, N>& arr1, const std::array<T, N>& arr2) {
			for (size_t i = 0; i < N; ++i) {
				if (!osg::equivalent(arr1[i], arr2[i])) return false;
			}
			return true;
		}

		static bool compareExtensions(const osg::MixinVector<GltfExtension*>& ext1,
			const osg::MixinVector<GltfExtension*>& ext2);
	};
}
#endif // !OSG_GIS_PLUGINS_GLTF_MATERIAL_H
