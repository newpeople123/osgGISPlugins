#ifndef OSG_GIS_PLUGINS_TEXTURE_ATLAS_H
#define OSG_GIS_PLUGINS_TEXTURE_ATLAS_H 1
#include <osg/Node>
#include <osg/Vec2>
#include <vector>
#include <string>
#include <utils/UUID.h>
#include <osg/Image>
#include <utils/GltfUtils.h>
//#ifndef TINYGLTF_IMPLEMENTATION
//#else
//#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include <tinygltf/tiny_gltf.h>
//#endif // !1
double atlasMaxWidth = 4096.0, atlasMaxHeight = 4096.0;
struct UVRange
{
	double startU, startV, endU, endV;
};
struct BoundingRectangle {
	double x, y, w, h;
	UVRange uvRange;
	BoundingRectangle(double x,double y,double w,double h){
		this->x = x;
		this->y = y;
		this->w = w;
		this->h = h;

		uvRange.startU = x;
		uvRange.endU = x + w;
		uvRange.startV = y;
		uvRange.endV = y + h;
	}
	BoundingRectangle() :BoundingRectangle(0.0, 0.0, 1.0, 1.0) {

	}
};
struct TextureAtlasNode
{
	TextureAtlasNode* childNode1;
	TextureAtlasNode* childNode2;
	int imageIndex;
	osg::Vec2 bottomLeft = osg::Vec2(0.0, 0.0);
	osg::Vec2 topRight = osg::Vec2(0.0, 0.0);
	explicit TextureAtlasNode(osg::Vec2 bottomLeft = osg::Vec2(0.0, 0.0), osg::Vec2 topRight = osg::Vec2(0.0, 0.0),TextureAtlasNode* childNode1=NULL, TextureAtlasNode* childNode2=NULL, int imageIndex=-1) {
		this->childNode1 = childNode1;
		this->childNode2 = childNode2;
		this->imageIndex = imageIndex;
		this->bottomLeft = bottomLeft;
		this->topRight = topRight;
	}
	TextureAtlasNode() {
		childNode1 = nullptr;
		childNode2 = nullptr;
		imageIndex = -1;
	}
	~TextureAtlasNode()
	{
		delete childNode1;

		delete childNode2;
	}
};
class TextureAtlas;
int getIndex(TextureAtlas* atlas, osg::ref_ptr<osg::Image>& image);
struct TextureAtlasOptions
{
	double borderWidthInPixels = 0.0;
	osg::Vec2 atlasMaxSize = osg::Vec2(4096.0, 4096.0);
	osg::Vec2 imgMaxSize = osg::Vec2(4096.0, 4096.0);
	GLenum pixelFormat = GL_RGB;
	unsigned int packing = 1;
	TextureAtlasOptions(osg::Vec2 atlasMaxSize, osg::Vec2 imgMaxSize, GLenum pixelFormat, unsigned int packing, double borderWidthInPixels = 0.0) {
		this->atlasMaxSize = atlasMaxSize;
		this->imgMaxSize = imgMaxSize;
		this->pixelFormat = pixelFormat;
		this->packing = packing;
		this->borderWidthInPixels = borderWidthInPixels;
	}
	TextureAtlasOptions() = default;
};
class TextureAtlas {
public:
	explicit TextureAtlas(TextureAtlasOptions options) {
		if (options.borderWidthInPixels < 0) {
			OSG_FATAL << "TextureAtlas Error:borderWidthInPixels must be greater than or equal to zero. ";
		}
		atlasMaxWidth = options.atlasMaxSize.x();
		atlasMaxHeight = options.atlasMaxSize.y();
		this->_imgMaxSize = options.imgMaxSize;
		this->_pixelFormat = options.pixelFormat;
		this->_borderWidthInPixels = options.borderWidthInPixels;
		this->_textureCoordinates.clear();
		this->_root = nullptr;
		this->_packing = options.packing;
	}
	~TextureAtlas() {
		this->_texture = nullptr;
		if (_root) {
			delete _root;
			this->_root = nullptr;
		}
	}
	int addImage(osg::ref_ptr<osg::Image>& image) {
		if (image.valid()) {
			resizeImage(image, this->_imgMaxSize);
			if (image->getPixelFormat() != this->_pixelFormat || image->getPacking() != this->_packing) {
				return -1;
			}
			int index = -1;
			index = getIndex(this, image);

			if (index != -1) {
				imgNames.push_back(osgDB::convertStringFromUTF8toCurrentCodePage(image->getFileName()));
				for (int i = 0; i < this->_textureCoordinates.size(); ++i) {
					const UVRange uvRange = this->_textureCoordinates[i].uvRange;
					std::string filename = imgNames.at(i);
					auto item = imgUVRangeMap.find(filename);
					if (item != imgUVRangeMap.end()) {
						item->second.endU = uvRange.endU;
						item->second.endV = uvRange.endV;
						item->second.startU = uvRange.startU;
						item->second.startV = uvRange.startV;

					}
					else {
						imgUVRangeMap.insert(std::make_pair(filename, uvRange));
					}
				}
				imgs.push_back(image);
			}
			return index;
		}
		return -1;
	}

	static void resizeImage(const osg::ref_ptr<osg::Image>& image,osg::Vec2 size) {
		const int currentWidth = image->s();
		const int currentHeight = image->t();
		const float scale = std::min(size.x() / currentWidth, size.y() / currentHeight);
		if (scale < 1) {
			image->scaleImage(static_cast<int>(scale * currentWidth), static_cast<int>(scale * currentHeight), image->r());
		}
	}

	int borderWidthInPixels() const
	{
		return this->_borderWidthInPixels;
	}
	auto& textureCoordinates() {
		return this->_textureCoordinates;
	}
	auto& texture() {
		return this->_texture;
	}
	auto numberOfImages() const
	{
		return this->_textureCoordinates.size();
	}
	auto& root() {
		return this->_root;
	}
	GLenum pixelFormat() const
	{
		return this->_pixelFormat;
	}
	TextureAtlasNode* _root;
	GLenum _pixelFormat;
	int _borderWidthInPixels;
	std::vector<BoundingRectangle> _textureCoordinates;
	osg::Vec2 _initialSize = osg::Vec2(64.0, 64.0);
	osg::Vec2 _imgMaxSize;
	osg::ref_ptr<osg::Image> _texture;
	unsigned int _packing;
	std::map<std::string, UVRange> imgUVRangeMap;
	std::vector<std::string> imgNames;
	std::vector<osg::ref_ptr<osg::Image>> imgs;
};

inline bool resizeAtlas(TextureAtlas* textureAtlas, const osg::ref_ptr<osg::Image>& image) {
	const int numImages = textureAtlas->numberOfImages();
	const int borderWidthInPixels = textureAtlas->borderWidthInPixels();

	auto findNearestGreaterPowerOfTwo = [](int n) {
		int powerOfTwo = 1;
		while (powerOfTwo < n) {
			powerOfTwo <<= 1;
		}
		return powerOfTwo;
		};

	if (numImages > 0) {
		const double oldAtlasWidth = textureAtlas->texture()->s();
		const double oldAtlasHeight = textureAtlas->texture()->t();
		//TO DO:seartch max useful storage
		const double atlasWidth = findNearestGreaterPowerOfTwo(image->s() + oldAtlasWidth + borderWidthInPixels);
		const double atlasHeight = findNearestGreaterPowerOfTwo(image->t() + oldAtlasHeight + borderWidthInPixels);

		if (atlasWidth > atlasMaxWidth || atlasHeight > atlasMaxHeight) {
			return false;
		}
		const double widthRatio = oldAtlasWidth / atlasWidth;
		const double heightRatio = oldAtlasHeight / atlasHeight;

		const TextureAtlasNode* nodeBottomRight = new TextureAtlasNode(osg::Vec2(oldAtlasWidth + borderWidthInPixels, borderWidthInPixels), osg::Vec2(atlasWidth, oldAtlasHeight));
		const TextureAtlasNode* nodeBottomHalf = new TextureAtlasNode(osg::Vec2(), osg::Vec2(atlasWidth, oldAtlasHeight), textureAtlas->_root, const_cast<TextureAtlasNode*>(nodeBottomRight));
		const TextureAtlasNode* nodeTopHalf = new TextureAtlasNode(osg::Vec2(borderWidthInPixels, oldAtlasHeight + borderWidthInPixels), osg::Vec2(atlasWidth, atlasHeight));
		const TextureAtlasNode* nodeMain = new TextureAtlasNode(osg::Vec2(), osg::Vec2(atlasWidth, atlasHeight), const_cast<TextureAtlasNode*>(nodeBottomHalf), const_cast<TextureAtlasNode*>(nodeTopHalf));

		for (int i = 0; i < textureAtlas->textureCoordinates().size(); ++i) {
			BoundingRectangle& texCoord = textureAtlas->textureCoordinates().at(i);
			texCoord.x *= widthRatio;
			texCoord.y *= heightRatio;
			texCoord.w *= widthRatio;
			texCoord.h *= heightRatio;
			texCoord.uvRange.startU = texCoord.x;
			texCoord.uvRange.endU = texCoord.x + texCoord.w;
			texCoord.uvRange.startV = texCoord.y;
			texCoord.uvRange.endV = texCoord.y + texCoord.h;

		}

		const osg::ref_ptr<osg::Image> newTexture = new osg::Image;
		newTexture->allocateImage(atlasWidth, atlasHeight, 1, textureAtlas->pixelFormat(), GL_UNSIGNED_BYTE);
		/*for (int imgY = 0; imgY < textureAtlas->_texture->t(); ++imgY) {
			for (int imgX = 0; imgX < textureAtlas->_texture->s(); ++imgX) {
				newTexture->setColor(textureAtlas->_texture->getColor(imgX, imgY), imgX, imgY);
			}
		}*/
		newTexture->copySubImage(0, 0, 0, textureAtlas->_texture.get());

		newTexture->setFileName(textureAtlas->_texture->getFileName());
		if (textureAtlas->_texture.valid()) {
			textureAtlas->_texture = nullptr;
		}
		textureAtlas->_texture = newTexture;
		textureAtlas->_root = const_cast<TextureAtlasNode*>(nodeMain);
	}
	else {
		int initialWidth = findNearestGreaterPowerOfTwo(image->s() + 2 * borderWidthInPixels);
		int initialHeight = findNearestGreaterPowerOfTwo(image->t() + 2 * borderWidthInPixels);
		if (initialWidth > 4096 || initialHeight > 4096) {
			return false;
		}
		if (initialWidth < textureAtlas->_initialSize.x()) {
			initialWidth = textureAtlas->_initialSize.x();
		}
		if (initialHeight < textureAtlas->_initialSize.y()) {
			initialHeight = textureAtlas->_initialSize.y();
		}
		if (textureAtlas->_texture.valid()) {
			textureAtlas->_texture.release();
		}
		const osg::ref_ptr<osg::Image> newTexture = new osg::Image;
		newTexture->setFileName(generateUUID());
		newTexture->allocateImage(initialWidth, initialHeight, 1, textureAtlas->pixelFormat(), GL_UNSIGNED_BYTE);
		textureAtlas->_texture = newTexture;
		textureAtlas->_root = new TextureAtlasNode(osg::Vec2(borderWidthInPixels, borderWidthInPixels), osg::Vec2(initialWidth, initialHeight));
	}
	return true;
}

TextureAtlasNode* findNode(TextureAtlas* textureAtlas, TextureAtlasNode* node, osg::ref_ptr<osg::Image>& image) {
	if (node == nullptr) {
		return nullptr;
	}
	if (node->childNode1 == nullptr && node->childNode2 == nullptr) {
		if (node->imageIndex != -1) {
			return nullptr;
		}

		const double nodeWidth = node->topRight.x() - node->bottomLeft.x();
		const double nodeHeight = node->topRight.y() - node->bottomLeft.y();
		const double widthDifference = nodeWidth - image->s();
		const double heightDifference = nodeHeight - image->t();
		if (widthDifference < 0 || heightDifference < 0) {
			return nullptr;
		}

		if (widthDifference == 0 && heightDifference == 0) {
			return node;
		}

		if (widthDifference > heightDifference) {
			node->childNode1 = new TextureAtlasNode(node->bottomLeft, osg::Vec2(node->bottomLeft.x() + image->s(), node->topRight.y()));
			const double childNode2BottomLeftX = node->bottomLeft.x() + image->s() + textureAtlas->borderWidthInPixels();
			if (childNode2BottomLeftX < node->topRight.x()) {
				node->childNode2 = new TextureAtlasNode(osg::Vec2(childNode2BottomLeftX, node->bottomLeft.y()), node->topRight);
			}
		}
		else {
			node->childNode1 = new TextureAtlasNode(node->bottomLeft, osg::Vec2(node->topRight.x(), node->bottomLeft.y() + image->t()));
			const double childNode2BottomLeftY = node->bottomLeft.y() + image->t() + textureAtlas->borderWidthInPixels();
			if (childNode2BottomLeftY < node->topRight.y()) {
				node->childNode2 = new TextureAtlasNode(osg::Vec2(node->bottomLeft.x(), childNode2BottomLeftY), node->topRight);
			}
		}
		return findNode(textureAtlas, node->childNode1, image);
	}
	TextureAtlasNode* result = findNode(textureAtlas, node->childNode1, image);
	if (result) {
		return result;
	}
	return findNode(textureAtlas, node->childNode2, image);
}

inline bool addImage(TextureAtlas* textureAtlas, osg::ref_ptr<osg::Image>& image, int index) {
	//if (index == 0) {
	//	resizeAtlas(textureAtlas, image);
	//}
	TextureAtlasNode* node = findNode(textureAtlas, textureAtlas->root(), image);
	if (node != nullptr) {
		node->imageIndex = index;

		const double atlasWidth = textureAtlas->_texture->s();
		const double atlasHeight = textureAtlas->_texture->t();
		const double nodeWidth = node->topRight.x() - node->bottomLeft.x();
		const double nodeHeight = node->topRight.y() - node->bottomLeft.y();
		const double x = node->bottomLeft.x() / atlasWidth;
		const double y = node->bottomLeft.y() / atlasHeight;
		const double w = nodeWidth / atlasWidth;
		const double h = nodeHeight / atlasHeight;
		textureAtlas->textureCoordinates().emplace_back(x, y, w, h);
		textureAtlas->texture()->copySubImage(node->bottomLeft.x(), node->bottomLeft.y(), 0, image.get());
		//for (int imgY = 0; imgY < image->t(); ++imgY) {
		//	for (int imgX = 0; imgX < image->s(); ++imgX) {
		//		textureAtlas->_texture->setColor(image->getColor(imgX, imgY), node->bottomLeft.x() + imgX, node->bottomLeft.y() + imgY);
		//	}
		//}
		//textureAtlas->setGuid(generateUUID());
	}
	//else {
	//	return false;
	//}
	//return true;
	else {
		if (!resizeAtlas(textureAtlas, image)) {
			return false;
		}
		return addImage(textureAtlas, image, index);
	}
	return true;
}

inline int getIndex(TextureAtlas* atlas,osg::ref_ptr<osg::Image>& image) {
	if (atlas == nullptr) {
		return -1;
	}
	const int index = atlas->numberOfImages();
	const bool result = addImage(atlas, image, index);
	if (result)
		return index;
	else
		return -1;
}
#endif // !OSG_GIS_PLUGINS_TEXTURE_ATLAS_H
