#ifndef OSG_GIS_PLUGINS_TEXTURE_OPTIMIZIER_H
#define OSG_GIS_PLUGINS_TEXTURE_OPTIMIZIER_H 1
#include <osg/NodeVisitor>
#include <osg/Texture>
#include <utils/TextureAtlas.h>
struct ImageSortKey
{
	GLenum pixelFormat;
	unsigned int packing;
	bool operator<(const ImageSortKey& other) const {
		if (pixelFormat < other.pixelFormat) {
			return true;
		}
		else if (pixelFormat > other.pixelFormat) {
			return false;
		}

		return packing < other.packing;
	}
};
typedef std::map<ImageSortKey, std::vector<osg::Image*>> ImageSortMap;
class TextureVisitor :public osg::NodeVisitor
{
public:
	TextureVisitor() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {

	}
	void apply(osg::Node& node) {
		osg::StateSet* ss = node.getStateSet();
		if (ss) {
			apply(*ss);
		}
		traverse(node);
	}
	void apply(osg::StateSet& stateset) {
		for (unsigned int i = 0; i < stateset.getTextureAttributeList().size(); ++i)
		{
			osg::StateAttribute* sa = stateset.getTextureAttribute(i, osg::StateAttribute::TEXTURE);
			osg::Texture* texture = dynamic_cast<osg::Texture*>(sa);
			if (texture)
			{
				apply(*texture);
			}
		}
	}
	void apply(osg::Texture& texture) {
		for (unsigned int i = 0; i < texture.getNumImages(); i++) {
			osg::Image* img = texture.getImage(i);
			bool isAddToImgs = true;
			for (int j = 0; j < imgs.size(); ++j) {
				if (euqalImage(img, imgs.at(j))) {
					isAddToImgs = false;
				}
			}
			if (isAddToImgs)
			{
				//img->flipVertical();
				imgs.push_back(img);
			}
		}
	}
	bool euqalImage(osg::Image* img1, osg::Image* img2) {
		const std::string filename1 = img1->getFileName();
		const std::string filename2 = img2->getFileName();

		if (filename1!=""|| filename2!="")
		{
			if (filename1 == filename2)
				return true;
			return false;
		}
		else if(filename1== filename2&& filename1==""){
			if (img1->s() != img2->s()|| img1->r() != img2->r() || img1->t() != img2->t()) {
				return false;
			}
			if (img1->getPixelFormat() != img2->getPixelFormat()) {
				return false;
			}
			if (img1->getPacking() != img2->getPacking()) {
				return false;
			}
			if (img1->getTotalDataSize() != img2->getTotalDataSize()) {
				return false;
			}
			const unsigned char* data1 = img1->data();
			std::vector<unsigned char> dataVector1(data1, data1 + img1->getTotalDataSize());
			const unsigned char* data2 = img2->data();
			std::vector<unsigned char> dataVector2(data2, data2 + img2->getTotalDataSize());

			for (unsigned int i = 0; i < dataVector1.size(); ++i) {
				if (dataVector1[i] != dataVector2[i]) {
					return false;
				}
			}
			return true;

		}
		return false;
	}
	void groupImages() {
		for (auto& img : imgs) {
			ImageSortKey key;
			key.pixelFormat = img->getPixelFormat();
			key.packing = img->getPacking();
			auto& val = ism.find(key);
			if (val != ism.end()) {
				val->second.push_back(img);
			}
			else {
				std::vector<osg::Image*> images;
				images.push_back(img);
				ism.insert(std::make_pair(key, images));
			}
		}

		for (auto& pair : ism) {
			std::vector<osg::Image*>& value = pair.second;
			std::sort(value.begin(), value.end(), [](const osg::Image* img1, const osg::Image* img2) {
				if (img1->s() * img1->t() < img2->s() * img2->t()) {
					return true;
				}
				else if (img1->s() * img1->t() == img2->s() * img2->t()) {
					if (img1->s() < img2->s()) {
						return true;
					}
					else if (img1->s() > img2->s()) {
						return false;
					}
					else {
						return img1->t() < img2->t();
					}
				}
				else {
					return false;
				}
				});
			
		}
	}
	std::vector<osg::Image*> imgs;
	ImageSortMap ism;
	~TextureVisitor() {};

private:

};
class TextureOptimizerResolve :public osg::NodeVisitor {
	std::vector<TextureAtlas*> textureAtlases;
public:
	TextureOptimizerResolve(std::vector<TextureAtlas*> textureAtlases) :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {
		this->textureAtlases = textureAtlases;
	}
	void apply(osg::Drawable& geom) {
		osg::StateSet* ss = geom.getStateSet();
		osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom.asGeometry()->getTexCoordArray(0));
		if (ss) {
			osg::StateSet* newStateset = new osg::StateSet;
			for (unsigned int i = 0; i < ss->getTextureAttributeList().size(); ++i)
			{
				osg::StateAttribute* sa = ss->getTextureAttribute(i, osg::StateAttribute::TEXTURE);
				osg::Texture* texture = dynamic_cast<osg::Texture*>(sa);
				if (texture)
				{
					osg::Texture* newTexture = new osg::Texture2D;
					for (unsigned int j = 0; j < texture->getNumImages(); j++) {
						osg::Image* img = texture->getImage(j);
						const std::string filename = img->getFileName();
						for (int k = 0; k < textureAtlases.size(); ++k) {
							TextureAtlas* textureAtlas = textureAtlases.at(k);
							auto& item = textureAtlas->imgUVRangeMap.find(filename);
							if (item != textureAtlas->imgUVRangeMap.end()) {
								UVRange uvRangle = item->second;
								newTexture->setImage(j, textureAtlas->texture());
								for (unsigned int q = 0; q < texCoords->size(); ++q) {
									osg::Vec2 coord = texCoords->at(q);
									osg::Vec2 newCoord(uvRangle.startU + (uvRangle.endU - uvRangle.startU) * coord.x(), uvRangle.startV + (uvRangle.endV - uvRangle.startV) * coord.y());
									texCoords->at(q) = newCoord;
								}
								geom.asGeometry()->setTexCoordArray(0, texCoords);
								break;
							}
						}
					}
					newStateset->setTextureAttribute(i, newTexture);
				}
			}
			geom.setStateSet(newStateset);
		}
	}
};
class TextureOptimizer {
public:
	TextureOptimizer(osg::ref_ptr<osg::Node> node) {
		node->accept(tv);
		tv.groupImages();
		buildTextureAtlas();
		TextureOptimizerResolve tos(textureAtlases);
		node->accept(tos);
	}
	void buildTextureAtlas() {
		for (auto& item : tv.ism) {
			while (item.second.size()!=0)
			{
				TextureAtlas* ta = new TextureAtlas(TextureAtlasOptions(osg::Vec2(128, 128), item.first.pixelFormat, item.first.packing));
				for (int i = 0; i < item.second.size(); ++i) {
					int index = ta->addImage(generateUUID(), item.second.at(i));
					if (index != -1) {
						item.second.erase(item.second.begin() + i);
						i--;
					}
				}
				if (ta->_texture.valid()) {
					textureAtlases.push_back(ta);
				}
			}
		}
	}
	TextureVisitor tv;
	std::vector<TextureAtlas*> textureAtlases;
private:
};

#endif // !OSG_GIS_PLUGINS_TEXTURE_OPTIMIZIER_H
