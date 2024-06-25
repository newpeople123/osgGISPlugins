#ifndef OSG_GIS_PLUGINS_TEXTUREPACKER_H
#define OSG_GIS_PLUGINS_TEXTUREPACKER_H
#include <osg/Image>
#include <osg/observer_ptr>
class TexturePacker :public osg::Referenced
{
public:
	TexturePacker(int maxW, int maxH) : _maxWidth(maxW), _maxHeight(maxH), _dictIndex(0) {}
	void setMaxSize(int w, int h) { _maxWidth = w; _maxHeight = h; }
	void clear();

	size_t addElement(osg::Image* image);
	size_t addElement(int width, int height);
	void removeElement(size_t id);

	osg::Image* pack(size_t& numImages, bool generateResult, bool stopIfFailed = false);
	bool getPackingData(size_t id, double& x, double& y, int& w, int& h);
	size_t getId(osg::Image* image) const;
protected:
	typedef std::pair<osg::observer_ptr<osg::Image>, osg::Vec4> InputPair;
	std::map<size_t, InputPair> _input, _result;
	int _maxWidth, _maxHeight, _dictIndex;
};
#endif // OSG_GIS_PLUGINS_TEXTUREPACKER_H
