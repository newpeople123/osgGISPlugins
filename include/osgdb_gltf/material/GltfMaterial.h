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
		~GltfMaterial() override = default;
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
			for (GltfExtension* item : other.materialExtensions) {
				GltfExtension* extension = item->clone();
				materialExtensions.push_back(extension);
			}
			for (GltfExtension* item : other.materialExtensionsByCesiumSupport) {
				GltfExtension* extension = item->clone();
				materialExtensionsByCesiumSupport.push_back(extension);
			}
		}
		GltfMaterial() {}
		//META_Object(osg, GltfMaterial);

		static bool compareTexture2D(const osg::ref_ptr<osg::Texture2D>& texture1, const osg::ref_ptr<osg::Texture2D>& texture2)
		{
			if (texture1 == texture2)
			{
				return true;
			}
			if (texture1.valid() != texture2.valid())
			{
				return false;
			}
			if (!texture1)
			{
				return true;
			}

			if (texture1->getWrap(osg::Texture::WRAP_S) != texture2->getWrap(osg::Texture::WRAP_S))
			{
				return false;
			}
			if (texture1->getWrap(osg::Texture::WRAP_T) != texture2->getWrap(osg::Texture::WRAP_T))
			{
				return false;
			}
			if (texture1->getWrap(osg::Texture::WRAP_R) != texture2->getWrap(osg::Texture::WRAP_R))
			{
				return false;
			}
			if (texture1->getFilter(osg::Texture::MIN_FILTER) != texture2->getFilter(osg::Texture::MIN_FILTER))
			{
				return false;
			}
			if (texture1->getFilter(osg::Texture::MAG_FILTER) != texture2->getFilter(osg::Texture::MAG_FILTER))
			{
				return false;
			}

			if (texture1->getNumImages() != texture2->getNumImages())
			{
				return false;
			}

			const osg::ref_ptr<osg::Image> img1 = texture1->getImage();
			const osg::ref_ptr<osg::Image> img2 = texture2->getImage();
			if (img1->getFileName() != img2->getFileName())
			{
				return false;
			}
			if (img1->s() != img2->s())
			{
				return false;
			}
			if (img1->r() != img2->r())
			{
				return false;
			}
			if (img1->t() != img2->t())
			{
				return false;
			}
			if (img1->getTotalDataSize() != img2->getTotalDataSize())
			{
				return false;
			}
			return true;
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
	protected:
		template<typename T, size_t N>
		static bool compareArray(const std::array<T, N>& arr1, const std::array<T, N>& arr2) {
			for (size_t i = 0; i < N; ++i) {
				if (!osg::equivalent(arr1[i], arr2[i])) return false;
			}
			return true;
		}

		static bool compareExtensions(const osg::MixinVector<GltfExtension*>& ext1,
			const osg::MixinVector<GltfExtension*>& ext2) {
			if (ext1.size() != ext2.size()) return false;

			for (size_t i = 0; i < ext1.size(); ++i) {
				if (!ext1[i]->compare(*ext2[i])) return false;
			}
			return true;
		}
	};
}
#endif // !OSG_GIS_PLUGINS_GLTF_MATERIAL_H
