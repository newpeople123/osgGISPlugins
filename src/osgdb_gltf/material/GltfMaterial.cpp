#include "osgdb_gltf/material/GltfMaterial.h"
using namespace osgGISPlugins;

bool GltfMaterial::compareTexture2D(osg::ref_ptr<osg::Texture2D> texture1, osg::ref_ptr<osg::Texture2D> texture2)
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

	osg::ref_ptr<osg::Image> img1 = texture1->getImage();
	osg::ref_ptr<osg::Image> img2 = texture2->getImage();
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

bool GltfMaterial::compareExtensions(const osg::MixinVector<GltfExtension*>& ext1,
	const osg::MixinVector<GltfExtension*>& ext2) {
	if (ext1.size() != ext2.size()) return false;

	for (size_t i = 0; i < ext1.size(); ++i) {
		if (!ext1[i]->compare(*ext2[i])) return false;
	}
	return true;
}

osg::ref_ptr<osg::Image> GltfMaterial::convertToGrayscale(osg::Image* input) {
	if (!input) return nullptr;

	int width = input->s();
	int height = input->t();
	osg::ref_ptr<osg::Image> gray = new osg::Image;
	gray->allocateImage(width, height, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE);

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			unsigned char* pixel = input->data(x, y);
			unsigned char grayValue = static_cast<unsigned char>(
				0.299f * pixel[0] + 0.587f * pixel[1] + 0.114f * pixel[2]);
			*gray->data(x, y) = grayValue;
		}
	}

	return gray;
}

osg::ref_ptr<osg::Image> GltfMaterial::generateNormalMap(osg::Image* heightMap) {
	if (!heightMap || heightMap->getPixelFormat() != GL_LUMINANCE) return nullptr;
	int width = heightMap->s(), height = heightMap->t();
	osg::ref_ptr<osg::Image> normalMap = new osg::Image;
	// 使用浮点存储提高精度
	normalMap->allocateImage(width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE);
	GLenum type = heightMap->getDataType();

	auto sample = [&](int x, int y)->float {
		x = osg::clampBetween(x, 0, width - 1);
		y = osg::clampBetween(y, 0, height - 1);
		return heightMap->getColor(x, y).r();
		};

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			// Sobel 卷积计算 dX, dY
			float h00 = sample(x - 1, y - 1), h10 = sample(x, y - 1), h20 = sample(x + 1, y - 1);
			float h01 = sample(x - 1, y), h21 = sample(x + 1, y);
			float h02 = sample(x - 1, y + 1), h12 = sample(x, y + 1), h22 = sample(x + 1, y + 1);
			float dX = (h20 + 2 * h21 + h22) - (h00 + 2 * h01 + h02);
			float dY = (h02 + 2 * h12 + h22) - (h00 + 2 * h10 + h20);
			osg::Vec3 n(-dX, -dY, 1.0f);
			n.normalize();
			osg::Vec4f color(n.x() * 0.5f + 0.5f, n.y() * 0.5f + 0.5f, n.z() * 0.5f + 0.5f, 1.0f);
			normalMap->setColor(color, x, y);
		}
	}
	return normalMap;
}

