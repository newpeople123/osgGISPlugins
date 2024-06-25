#include "utils/TextureOptimizer.h"
#include <osgDB/WriteFile>
#include <osg/ImageUtils>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <iomanip>
TexturePackingVisitor::TexturePackingVisitor(int maxWidth, int maxHeight, std::string ext, std::string cachePath) : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
_maxWidth(maxWidth), _maxHeight(maxHeight), _ext(ext), _cachePath(cachePath) {}

const std::string TexturePackingVisitor::Filename = "osgGisPlugins-filename";
const std::string TexturePackingVisitor::ExtensionName = "osgGisPlugins-KHR_texture_transform";
const std::string TexturePackingVisitor::ExtensionOffsetX = "osgGisPlugins-offsetX";
const std::string TexturePackingVisitor::ExtensionOffsetY = "osgGisPlugins-offsetY";
const std::string TexturePackingVisitor::ExtensionScaleX = "osgGisPlugins-scaleX";
const std::string TexturePackingVisitor::ExtensionScaleY = "osgGisPlugins-scaleY";
const std::string TexturePackingVisitor::ExtensionTexCoord = "osgGisPlugins-texCoord";


void TexturePackingVisitor::apply(osg::Drawable& drawable)
{
	const osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
	osg::ref_ptr<osg::StateSet> stateSet = geom->getStateSet();
	if (stateSet.valid())
	{
		osg::ref_ptr<osg::Texture2D> texture = dynamic_cast<osg::Texture2D*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
		osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
		if (texCoords.valid() && texture.valid()) {
			osg::ref_ptr<osg::Image> image = texture->getImage(0);
			if (image.valid()) {
				_resizeImageToPowerOfTwo(image);
				bool bBuildTexturePacker = true;
				for (auto texCoord : *texCoords.get()) {
					if (std::fabs(texCoord.x()) - 1.0 > 0.001 || std::fabs(texCoord.y()) - 1.0 > 0.001) {
						bBuildTexturePacker = false;
						break;
					}
				}
				if (bBuildTexturePacker)
				{
					if (image->s() < _maxWidth || image->t() < _maxHeight)
					{
						bool bAdd = true;
						for (auto img : _images) {
							if (img->getFileName() == image->getFileName()) {
								_geometryMap[geom] = img;
								bAdd = false;
							}
						}
						if (bAdd) {
							_images.push_back(image);
							_geometryMap[geom] = image;
						}
					}
					else
					{
						std::string name;
						image->getUserValue(Filename, name);
						if (name.empty()) {	
							_exportImage(image);
						}
					}
				}
				else {
					std::string name;
					image->getUserValue(Filename, name);
					if (name.empty()) {
						_exportImage(image);
					}
				}
			}
		}
	}
}

void TexturePackingVisitor::packTextures()
{
	if (_images.empty()) return;
	while (_images.size())
	{
		TexturePacker packer(_maxWidth, _maxHeight);

		int area = _maxWidth * _maxHeight;

		std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
		auto compareImageHeight = [](osg::Image* img1, osg::Image* img2) {
			if (img1->t() == img2->t()) {
				return img1->s() > img2->s();
			}
			return img1->t() > img2->t();
			};
		std::sort(_images.begin(), _images.end(), compareImageHeight);
		std::vector<size_t> indexPacks;
		osg::ref_ptr<osg::Image> packedImage;
		for (size_t i = 0; i < _images.size(); ++i) {
			osg::ref_ptr<osg::Image> img = _images.at(i);
			if (area >= img->s() * img->t()) {
				indexPacks.push_back(packer.addElement(img));
				area -= img->s() * img->t();
				deleteImgs.push_back(img);
				if (i == (_images.size() - 1)) {
					packImages(packedImage, indexPacks, deleteImgs, packer);
				}
			}
			else {
				packImages(packedImage, indexPacks, deleteImgs, packer);

				break;
			}
		}

		if (packedImage.valid() && deleteImgs.size())
		{
			const int width = packedImage->s(), height = packedImage->t();
			bool bResizeImage = _resizeImageToPowerOfTwo(packedImage);
			_exportImage(packedImage);
			for (auto& entry : _geometryMap)
			{
				osg::Geometry* geometry = entry.first;
				osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
				osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_ALL);

				osg::ref_ptr<osg::Texture2D> oldTexture = dynamic_cast<osg::Texture2D*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
				osg::ref_ptr<osg::Texture2D> packedTexture = osg::clone(oldTexture.get(), osg::CopyOp::DEEP_COPY_ALL);

				packedTexture->setImage(packedImage);
				osg::Image* image = entry.second;
				int w, h;
				double x, y;
				const size_t id = packer.getId(image);
				if (packer.getPackingData(id, x, y, w, h))
				{
					packedTexture->setUserValue(ExtensionName, true);
					float offsetX = x / width;
					offsetX = offsetX > 1.0 ? 1.0 : offsetX;
					offsetX = offsetX < 0.0 ? 0.0 : offsetX;
					packedTexture->setUserValue(ExtensionOffsetX, offsetX);
					float offsetY = y / height;
					offsetY = offsetY > 1.0 ? 1.0 : offsetY;
					offsetY = offsetY < 0.0 ? 0.0 : offsetY;
					packedTexture->setUserValue(ExtensionOffsetY, offsetY);
					float scaleX = static_cast<float>(w) / width;
					scaleX = scaleX > 1.0 ? 1.0 : scaleX;
					scaleX = scaleX < 0.0 ? 0.0 : scaleX;
					packedTexture->setUserValue(ExtensionScaleX, scaleX);
					float scaleY = static_cast<float>(h) / height;
					scaleY = scaleY > 1.0 ? 1.0 : scaleY;
					scaleY = scaleY < 0.0 ? 0.0 : scaleY;
					packedTexture->setUserValue(ExtensionScaleY, scaleY);
					packedTexture->setUserValue(ExtensionTexCoord, 0);
					stateSetCopy->setTextureAttribute(0, packedTexture.get());
					geometry->setStateSet(stateSetCopy.get());
				}
			}
			for (osg::ref_ptr<osg::Image> img : deleteImgs) {
				auto it = std::remove(_images.begin(), _images.end(), img.get());
				_images.erase(it, _images.end());
			}
		}

	}
}

void TexturePackingVisitor::packImages(osg::ref_ptr<osg::Image>& img, std::vector<size_t>& indexes, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs, TexturePacker& packer)
{
	size_t numImages;
	img = packer.pack(numImages, true);
	if (!img.valid() || numImages != indexes.size()) {
		img = NULL;
		packer.removeElement(indexes.back());
		indexes.pop_back();
		deleteImgs.pop_back();
		packImages(img, indexes, deleteImgs, packer);
	}
}

void TexturePackingVisitor::_exportImage(const osg::ref_ptr<osg::Image>& img)
{
	std::string ext = _ext;
	std::string filename = _computeImageHash(img);
	const GLenum pixelFormat = img->getPixelFormat();
	if (_ext == ".jpg" && pixelFormat != GL_ALPHA && pixelFormat != GL_RGB) {
		ext = ".png";
	}
	std::string fullPath = _cachePath + "/" + filename + ext;
	bool isFileExists = osgDB::fileExists(fullPath);
	if (!isFileExists)
	{
		osg::ref_ptr< osg::Image > flipped = new osg::Image(*img);
		if (flipped->getOrigin() == osg::Image::BOTTOM_LEFT)
		{
			flipped->flipVertical();
			flipped->setOrigin(osg::Image::TOP_LEFT);
		}
		if (!(osgDB::writeImageFile(*flipped.get(), fullPath))) {
			fullPath = _cachePath + "/" + filename + ".png";
			std::ifstream fileExistedPng(fullPath);
			if ((!fileExistedPng.good()) || (fileExistedPng.peek() == std::ifstream::traits_type::eof()))
			{
				if (!(osgDB::writeImageFile(*flipped.get(), fullPath))) {
					osg::notify(osg::FATAL) << '\n';
				}
			}
			fileExistedPng.close();
		}
	}
	img->setUserValue(Filename, fullPath);
}

bool TexturePackingVisitor::_resizeImageToPowerOfTwo(const osg::ref_ptr<osg::Image>& img)
{
	int originalWidth = img->s();
	int originalHeight = img->t();
	int newWidth = _findNearestPowerOfTwo(originalWidth);
	int newHeight = _findNearestPowerOfTwo(originalHeight);

	if (!(originalWidth == newWidth && originalHeight == newHeight))
	{
		newWidth = newWidth > _maxWidth ? _maxWidth : newWidth;
		newHeight = newHeight > _maxHeight ? _maxHeight : newHeight;
		img->scaleImage(newWidth, newHeight, img->r());
		return true;
	}
	return false;
}

int TexturePackingVisitor::_findNearestPowerOfTwo(int value)
{
	int powerOfTwo = 1;
	while (powerOfTwo * 2 <= value) {
		powerOfTwo *= 2;
	}
	return powerOfTwo;
}

std::string TexturePackingVisitor::_computeImageHash(const osg::ref_ptr<osg::Image>& image)
{
	size_t hash = 0;
	auto hash_combine = [&hash](size_t value) {
		hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		};

	hash_combine(std::hash<int>()(image->s()));
	hash_combine(std::hash<int>()(image->t()));
	hash_combine(std::hash<int>()(image->r()));
	hash_combine(std::hash<int>()(image->getPixelFormat()));
	hash_combine(std::hash<int>()(image->getDataType()));

	const unsigned char* data = image->data();
	size_t dataSize = image->getImageSizeInBytes();

	for (size_t i = 0; i < dataSize; ++i)
	{
		hash_combine(std::hash<unsigned char>()(data[i]));
	}
	std::ostringstream oss;
	oss << std::hex << std::setw(16) << std::setfill('0') << hash;
	return oss.str();
}
