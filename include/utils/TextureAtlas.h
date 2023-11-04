#ifndef OSG_GIS_PLUGINS_TEXTURE_ATLAS_H
#define OSG_GIS_PLUGINS_TEXTURE_ATLAS_H 1
#include <osg/Node>
#include <osg/Vec2>
#include <vector>
#include <string>
#include <utils/UUID.h>
#include <osg/Image>
//#ifndef TINYGLTF_IMPLEMENTATION
//#else
//#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include <tinygltf/tiny_gltf.h>
//#endif // !1

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
	osg::Vec2 bottomLeft;
	osg::Vec2 topRight;
	TextureAtlasNode(osg::Vec2 bottomLeft = osg::Vec2(0.0, 0.0), osg::Vec2 topRight = osg::Vec2(0.0, 0.0),TextureAtlasNode* childNode1=NULL, TextureAtlasNode* childNode2=NULL, int imageIndex=-1) {
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
		bottomLeft = osg::Vec2(0.0, 0.0);
		topRight = osg::Vec2(0.0, 0.0);
	}
};
class TextureAtlas;
int getIndex(TextureAtlas* atlas, osg::ref_ptr<osg::Image> image);
struct TextureAtlasOptions
{
	double borderWidthInPixels = 0.0;
	osg::Vec2 initialSize = osg::Vec2(2048.0, 2048.0);
	GLenum pixelFormat = GL_RGB;
	unsigned int packing = 1;
	TextureAtlasOptions(osg::Vec2 initialSize, GLenum pixelFormat, unsigned int packing, double borderWidthInPixels = 0.0) {
		this->initialSize = initialSize;
		this->pixelFormat = pixelFormat;
		this->packing = packing;
		this->borderWidthInPixels = borderWidthInPixels;
	}
	TextureAtlasOptions() {}
};
class TextureAtlas {
public:
	TextureAtlas(TextureAtlasOptions options) {
		if (options.borderWidthInPixels < 0) {
			OSG_FATAL << "TextureAtlas Error:borderWidthInPixels must be greater than or equal to zero. ";
		}
		if (options.initialSize.x() < 1.0 || options.initialSize.y() < 1.0) {
			OSG_FATAL << "TextureAtlas Error:initialSize must be greater than zero. ";
		}
		this->_pixelFormat = options.pixelFormat;
		this->_borderWidthInPixels = options.borderWidthInPixels;
		this->_textureCoordinates.clear();
		this->_guid = generateUUID();
		this->_initialSize = options.initialSize;
		this->_idHash = json::object();
		this->_indexHash = json::object();
		this->_root = NULL;
		this->_packing = options.packing;
	}
	~TextureAtlas() {
		if (this->_texture != NULL) {
			this->_texture.release();
			this->_texture = NULL;
		}
	}
	int getImageIndex(std::string id) {
		return this->_indexHash[id];
	}
	int addImage(std::string id, osg::ref_ptr<osg::Image> image) {
		if (image.valid()) {
			if (image->getPixelFormat() != this->_pixelFormat || image->getPacking() != this->_packing) {
				return -1;
			}
			int index = -1;
			auto val = this->_indexHash.find(id);
			if (val != this->_indexHash.end()) {
				index = this->_indexHash[id];
				if (index > -1) {
					return index;
				}
			}
			index = getIndex(this, image);
			this->_idHash[id] = index;
			this->_indexHash[id] = index;
			if (index != -1) {
				const std::string filename = image->getFileName();
				const UVRange uvRange = this->_textureCoordinates[index].uvRange;
				imgUVRangeMap.insert(std::make_pair(filename, uvRange));
				imgNames.push_back(filename);
			}
			return index;
		}
		return -1;
	}
	int addSubRegion(std::string id, BoundingRectangle subRegion) {
		const int index = this->_idHash[id];
		if (index == -1) {
			return -1;
		}
		const double atlasWidth = this->_texture->s();
		const double atlasHeight = this->_texture->t();

		const BoundingRectangle baseRegion = this->_textureCoordinates[index];
		const double x = baseRegion.x + subRegion.x / atlasWidth;
		const double y = baseRegion.y + subRegion.y / atlasHeight;
		const double w = subRegion.w / atlasWidth;
		const double h = subRegion.h / atlasHeight;
		this->_textureCoordinates.push_back(BoundingRectangle(x, y, w, h));
		const int size = this->_textureCoordinates.size();
		const int newIndex = size - 1;
		this->_indexHash[id] = newIndex;
		this->_guid = generateUUID();
		return newIndex;

	}
	
	int borderWidthInPixels() {
		return this->_borderWidthInPixels;
	}
	auto& textureCoordinates() {
		return this->_textureCoordinates;
	}
	auto& texture() {
		return this->_texture;
	}
	auto numberOfImages() {
		return this->_textureCoordinates.size();
	}
	auto& guid() {
		return this->_guid;
	}
	void setGuid(std::string guid) {
		this->_guid = guid;
	}
	auto& root() {
		return this->_root;
	}
	GLenum pixelFormat() {
		return this->_pixelFormat;
	}
	TextureAtlasNode* _root;
	GLenum _pixelFormat;
	int _borderWidthInPixels;
	std::vector<BoundingRectangle> _textureCoordinates;
	std::string _guid;
	json _idHash;
	json _indexHash;
	osg::Vec2 _initialSize;
	osg::ref_ptr<osg::Image> _texture;
	unsigned int _packing;
	std::map<std::string, UVRange> imgUVRangeMap;
	std::vector<std::string> imgNames;
};
BoundingRectangle calculateIntersection(const BoundingRectangle& r1, const BoundingRectangle& r2) {
	BoundingRectangle result;
	result.uvRange.startU = std::max(r1.uvRange.startU, r2.uvRange.startU);
	result.uvRange.startV = std::max(r1.uvRange.startV, r2.uvRange.startV);
	result.uvRange.endU = std::min(r1.uvRange.endU, r2.uvRange.endU);
	result.uvRange.endV = std::min(r1.uvRange.endV, r2.uvRange.endV);
	return result;
}
BoundingRectangle calculateMaxRemainingRectangle(std::vector<BoundingRectangle> rectangles) {
	double maxX1 = 0, maxY1 = 0, minX2 = 1.0, minY2 = 1.0;
	for (const BoundingRectangle& rect : rectangles) {
		maxX1 = std::max(maxX1, rect.uvRange.startU);
		maxY1 = std::max(maxY1, rect.uvRange.startV);
		minX2 = std::min(minX2, rect.uvRange.endU);
		minY2 = std::min(minY2, rect.uvRange.endV);

	}
	if (maxX1 < minX2 && maxY1 < minY2) {
		BoundingRectangle remaining(maxX1, maxY1, minX2, minY2);
		return remaining;
	}
	else {
		BoundingRectangle remaining(0, 0, 0, 0);
		return remaining;
	}
}

bool resizeAtlas(TextureAtlas* textureAtlas, osg::ref_ptr<osg::Image> image) {
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
		const int oldAtlasWidth = textureAtlas->texture()->s();
		const int oldAtlasHeight = textureAtlas->texture()->t();
		//TO DO:seartch max useful storage
		const int atlasWidth = findNearestGreaterPowerOfTwo(oldAtlasWidth + image->s() + borderWidthInPixels);
		const int atlasHeight = findNearestGreaterPowerOfTwo(oldAtlasHeight + image->t() + borderWidthInPixels);

		if (atlasWidth > 4096 || atlasHeight > 4096) {
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
		}

		osg::ref_ptr<osg::Image> newTexture = new osg::Image;
		newTexture->allocateImage(atlasWidth, atlasHeight, 1, textureAtlas->pixelFormat(), GL_UNSIGNED_BYTE);
		for (int imgY = 0; imgY < textureAtlas->_texture->t(); ++imgY) {
			for (int imgX = 0; imgX < textureAtlas->_texture->s(); ++imgX) {
				newTexture->setColor(textureAtlas->_texture->getColor(imgX, imgY), imgX, imgY);
			}
		}
		newTexture->setFileName(textureAtlas->_texture->getFileName());
		textureAtlas->_texture = textureAtlas->_texture ? textureAtlas->_texture.release() : textureAtlas->_texture;
		textureAtlas->_texture = newTexture;
		textureAtlas->_root = const_cast<TextureAtlasNode*>(nodeMain);
	}
	else {
		int initialWidth = findNearestGreaterPowerOfTwo(image->s() + 2 * borderWidthInPixels);
		int initialHeight = findNearestGreaterPowerOfTwo(image->t() + 2 * borderWidthInPixels);
		if (initialWidth < textureAtlas->_initialSize.x()) {
			initialWidth = textureAtlas->_initialSize.x();
		}
		if (initialHeight < textureAtlas->_initialSize.y()) {
			initialHeight = textureAtlas->_initialSize.y();
		}
		textureAtlas->_texture = textureAtlas->_texture ? textureAtlas->_texture.release() : textureAtlas->_texture;
		osg::ref_ptr<osg::Image> newTexture = new osg::Image;
		newTexture->setFileName(generateUUID());
		newTexture->allocateImage(initialWidth, initialHeight, 1, textureAtlas->pixelFormat(), GL_UNSIGNED_BYTE);
		textureAtlas->_texture = newTexture;
		textureAtlas->_root = new TextureAtlasNode(osg::Vec2(borderWidthInPixels, borderWidthInPixels), osg::Vec2(initialWidth, initialHeight));
	}
	return true;
	/*textureAtlas->_texture = textureAtlas->_texture ? textureAtlas->_texture.release() : textureAtlas->_texture;
	osg::ref_ptr<osg::Image> newTexture = new osg::Image;
	newTexture->allocateImage(textureAtlas->_initialSize.x(), textureAtlas->_initialSize.y(), 1, textureAtlas->pixelFormat(), GL_UNSIGNED_BYTE);
	textureAtlas->_texture = newTexture;
	textureAtlas->_root = new TextureAtlasNode(osg::Vec2(textureAtlas->borderWidthInPixels(), textureAtlas->borderWidthInPixels()), osg::Vec2(textureAtlas->_initialSize.x(), textureAtlas->_initialSize.y()));*/
}
void oldResizeAtlas(TextureAtlas* textureAtlas, osg::ref_ptr<osg::Image> image) {
	const int numImages = textureAtlas->numberOfImages();
	const double scalingFactor = 2.0;
	const double borderWidthInPixels = textureAtlas->borderWidthInPixels();

	if (numImages > 0) {
		const double oldAtlasWidth = textureAtlas->texture()->s();
		const double oldAtlasHeight = textureAtlas->texture()->t();
		const double atlasWidth = scalingFactor * (oldAtlasWidth + image->s() + borderWidthInPixels);
		const double atlasHeight = scalingFactor * (oldAtlasHeight + image->t() + borderWidthInPixels);
		const double widthRatio = oldAtlasWidth / atlasWidth;
		const double heightRatio = oldAtlasHeight / atlasHeight;

		const TextureAtlasNode* nodeBottomRight = new TextureAtlasNode(osg::Vec2(oldAtlasWidth + borderWidthInPixels, borderWidthInPixels), osg::Vec2(atlasWidth, oldAtlasHeight));
		const TextureAtlasNode* nodeBottomHalf = new TextureAtlasNode(osg::Vec2(), osg::Vec2(atlasWidth, oldAtlasHeight), textureAtlas->_root,const_cast<TextureAtlasNode*>(nodeBottomRight));
		const TextureAtlasNode* nodeTopHalf = new TextureAtlasNode(osg::Vec2(borderWidthInPixels, oldAtlasHeight + borderWidthInPixels), osg::Vec2(atlasWidth, atlasHeight));
		const TextureAtlasNode* nodeMain = new TextureAtlasNode(osg::Vec2(), osg::Vec2(atlasWidth, atlasHeight), const_cast<TextureAtlasNode*>(nodeBottomHalf), const_cast<TextureAtlasNode*>(nodeTopHalf));

		for (int i = 0; i < textureAtlas->textureCoordinates().size(); ++i) {
			BoundingRectangle& texCoord= textureAtlas->textureCoordinates().at(i);
			texCoord.x *= widthRatio;
			texCoord.y *= heightRatio;
			texCoord.w *= widthRatio;
			texCoord.h *= heightRatio;
		}
		
		osg::ref_ptr<osg::Image> newTexture = new osg::Image;
		newTexture->allocateImage(atlasWidth, atlasHeight, 1, textureAtlas->pixelFormat(), GL_UNSIGNED_BYTE);
		for (int imgY = 0; imgY < textureAtlas->_texture->t(); ++imgY) {
			for (int imgX = 0; imgX < textureAtlas->_texture->s(); ++imgX) {
				newTexture->setColor(textureAtlas->_texture->getColor(imgX, imgY), imgX, imgY);
			}
		}
		textureAtlas->_texture = textureAtlas->_texture ? textureAtlas->_texture.release() : textureAtlas->_texture;
		textureAtlas->_texture = newTexture;
		textureAtlas->_root = const_cast<TextureAtlasNode*>(nodeMain);
	}
	else {
		double initialWidth = scalingFactor * (image->s() + 2 * borderWidthInPixels);
		double initialHeight = scalingFactor * (image->t() + 2 * borderWidthInPixels);
		if (initialWidth < textureAtlas->_initialSize.x()) {
			initialWidth = textureAtlas->_initialSize.x();
		}
		if (initialHeight < textureAtlas->_initialSize.y()) {
			initialHeight = textureAtlas->_initialSize.y();
		}
		textureAtlas->_texture = textureAtlas->_texture ? textureAtlas->_texture.release() : textureAtlas->_texture;
		osg::ref_ptr<osg::Image> newTexture = new osg::Image;
		newTexture->allocateImage(initialWidth, initialHeight, 1, textureAtlas->pixelFormat(), GL_UNSIGNED_BYTE);
		textureAtlas->_texture = newTexture;
		textureAtlas->_root = new TextureAtlasNode(osg::Vec2(borderWidthInPixels, borderWidthInPixels), osg::Vec2(initialWidth, initialHeight));
	}

	/*textureAtlas->_texture = textureAtlas->_texture ? textureAtlas->_texture.release() : textureAtlas->_texture;
	osg::ref_ptr<osg::Image> newTexture = new osg::Image;
	newTexture->allocateImage(textureAtlas->_initialSize.x(), textureAtlas->_initialSize.y(), 1, textureAtlas->pixelFormat(), GL_UNSIGNED_BYTE);
	textureAtlas->_texture = newTexture;
	textureAtlas->_root = new TextureAtlasNode(osg::Vec2(textureAtlas->borderWidthInPixels(), textureAtlas->borderWidthInPixels()), osg::Vec2(textureAtlas->_initialSize.x(), textureAtlas->_initialSize.y()));*/
}
TextureAtlasNode* findNode(TextureAtlas* textureAtlas, TextureAtlasNode* node, osg::ref_ptr<osg::Image> image) {
	if (node == NULL) {
		return NULL;
	}
	if (node->childNode1 == NULL && node->childNode2 == NULL) {
		if (node->imageIndex != -1) {
			return NULL;
		}

		const double nodeWidth = node->topRight.x() - node->bottomLeft.x();
		const double nodeHeight = node->topRight.y() - node->bottomLeft.y();
		const double widthDifference = nodeWidth - image->s();
		const double heightDifference = nodeHeight - image->t();
		if (widthDifference < 0 || heightDifference < 0) {
			return NULL;
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
bool addImage(TextureAtlas* textureAtlas, osg::ref_ptr<osg::Image> image, int index) {
	//if (index == 0) {
	//	resizeAtlas(textureAtlas, image);
	//}
	TextureAtlasNode* node = findNode(textureAtlas, textureAtlas->root(), image);
	if (node != NULL) {
		node->imageIndex = index;

		const double atlasWidth = textureAtlas->_texture->s();
		const double atlasHeight = textureAtlas->_texture->t();
		const double nodeWidth = node->topRight.x() - node->bottomLeft.x();
		const double nodeHeight = node->topRight.y() - node->bottomLeft.y();
		const double x = node->bottomLeft.x() / atlasWidth;
		const double y = node->bottomLeft.y() / atlasHeight;
		const double w = nodeWidth / atlasWidth;
		const double h = nodeHeight / atlasHeight;
		textureAtlas->textureCoordinates().push_back(BoundingRectangle(0, 0, 0, 0));
		textureAtlas->textureCoordinates()[index] = BoundingRectangle(x, y, w, h);
		//textureAtlas->texture()->copySubImage(node->bottomLeft.x(), node->bottomLeft.y(), 1, image.get());
		for (int imgY = 0; imgY < image->t(); ++imgY) {
			for (int imgX = 0; imgX < image->s(); ++imgX) {
				textureAtlas->_texture->setColor(image->getColor(imgX, imgY), node->bottomLeft.x() + imgX, node->bottomLeft.y() + imgY);
			}
		}
		textureAtlas->setGuid(generateUUID());
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
	textureAtlas->setGuid(generateUUID());
	return true;
}
int getIndex(TextureAtlas* atlas, osg::ref_ptr<osg::Image> image) {
	if (atlas == NULL) {
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
