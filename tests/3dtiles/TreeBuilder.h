#ifndef OSG_GIS_PLUGINS_TREE_BUILDER_H
#define OSG_GIS_PLUGINS_TREE_BUILDER_H
#include "3dtiles/Tileset.h"
#include <osg/Object>
class TreeBuilder :public osg::Object
{
public:
	osg::ref_ptr<TileNode> rootTreeNode = new TileNode;
	TreeBuilder(const TreeBuilder& node, const osg::CopyOp & = osg::CopyOp::SHALLOW_COPY) : _maxTriangleNumber(0),
		_maxTreeDepth(0),
		_maxLevel(0)
	{
	}

	TreeBuilder() :_maxTriangleNumber(40000), _maxTreeDepth(8), _maxLevel(0) {
	}

	TreeBuilder(const unsigned int maxTriangleNumber, const int maxTreeDepth) :_maxTriangleNumber(maxTriangleNumber), _maxTreeDepth(maxTreeDepth), _maxLevel(0) {
	}
	TreeBuilder(const osg::ref_ptr<osg::Group>& node) :_maxTriangleNumber(40000), _maxTreeDepth(8), _maxLevel(0) {
	}
	TreeBuilder(const osg::ref_ptr<osg::Group>& node, const unsigned int maxTriangleNumber, const int maxTreeDepth) :_maxTriangleNumber(maxTriangleNumber), _maxTreeDepth(maxTreeDepth), _maxLevel(0) {
	}
	~TreeBuilder() override = default;
	Object* cloneType() const override { return new TreeBuilder(); }
	Object* clone(const osg::CopyOp& copyop) const override { return new TreeBuilder(*this, copyop); }
	const char* libraryName() const override { return "osgGisPluginsTools"; }
	const char* className() const override { return "model23dtiles"; }

	std::string testOutput;
	osg::ref_ptr<osgDB::Options> option;

	void setOutput(const std::string& val)
	{
		testOutput = val;
	}
	void setOption(const osg::ref_ptr<osgDB::Options>& val)
	{
		option = val;
	}
protected:
	unsigned int _maxTriangleNumber;
	int _maxTreeDepth;
	int _maxLevel;
};
#endif // !OSG_GIS_PLUGINS_TREE_BUILDER_H
