#ifndef OSG_GIS_PLUGINS_TREEBUILDER
#define OSG_GIS_PLUGINS_TREEBUILDER 1
#include <queue>
#include <osg/Object>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PositionAttitudeTransform>
#include <osgUtil/Simplifier>
#include <osg/ComputeBoundsVisitor>
#include <iostream>
#include <random>
#include <future>
#include <utils/CustomSimplify.h>
#include <utils/GltfUtils.h>
#include <osgDB/WriteFile>
#include "TileNode.h"
#include "GeometricError.h"

class TriangleNumberNodeVisitor :public osg::NodeVisitor
{
public:
	unsigned int count = 0;
	TriangleNumberNodeVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {};

	void apply(osg::Drawable& drawable) override
	{
		if (drawable.asGeometry())
		{
			const osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
			for (unsigned i = 0; i < geom->getNumPrimitiveSets(); ++i) {
				osg::ref_ptr<osg::DrawArrays> drawArrays = dynamic_cast<osg::DrawArrays*>(geom->getPrimitiveSet(i));
				if (drawArrays) {
					const GLenum mode = drawArrays->getMode();
					const unsigned int triangleNumber = drawArrays->getCount();
					switch (mode)
					{
					case GL_TRIANGLES:
						count += triangleNumber / 3;
						break;
					case GL_TRIANGLE_STRIP:
						count += (triangleNumber - 2);
						break;
					case GL_TRIANGLE_FAN:
						count += (triangleNumber - 2);
						break;
					case GL_QUADS:
						count += (triangleNumber / 4) * 2;
						break;
					case GL_QUAD_STRIP:
						count += triangleNumber;
						break;
					case GL_POLYGON:
						count += triangleNumber - 2;
						break;
					default:
						break;
					}
				}

				osg::ref_ptr<osg::DrawElements> drawElements = dynamic_cast<osg::DrawElements*>(geom->getPrimitiveSet(i));
				if (drawElements) {
					count += drawElements->getNumIndices() / 3;
				}
			}
		}
	}

	void apply(osg::Group& group) override
	{
		traverse(group);
	};

};
class RebuildDataNodeVisitor :public osg::NodeVisitor {
public:
	RebuildDataNodeVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
	osg::ref_ptr<osg::Group> output = new osg::MatrixTransform;
	void apply(osg::Drawable& drawable) override
	{
		if(drawable.asGeometry())
			output->addChild(&drawable);
	};
	//void apply(osg::Geode& geode) {
	//	osg::Geode* copyGeode = new osg::Geode;
	//	output->addChild(copyGeode);
	//};
	void apply(osg::Group& group) override
	{
		traverse(group);
	};
};

class GroupByTextureVisitor:public osg::NodeVisitor
{
public:
	GroupByTextureVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
	void apply(osg::Drawable& drawable) override {
		const osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
		const osg::ref_ptr<osg::StateSet> stateset = geom->getStateSet();
		if(stateset.valid())
		{
			const unsigned int textureSize = stateset->getTextureAttributeList().size();
			if (textureSize != 0) {
				for (unsigned int i = 0; i < textureSize; ++i)
				{
					osg::StateAttribute* sa = stateset->getTextureAttribute(i, osg::StateAttribute::TEXTURE);
					osg::Texture* texture = dynamic_cast<osg::Texture*>(sa);
					if (texture)
					{
						const unsigned int numImages = texture->getNumImages();
						if (numImages != 0) {
							for (unsigned int j = 0; j < numImages; j++) {
								const osg::ref_ptr<osg::Image> img = texture->getImage(j);
								std::string filename = img->getFileName();
								if (!filename.empty())
									filename = osgDB::convertStringFromUTF8toCurrentCodePage(filename);
								const auto& item = textureGeometryMap.find(filename);
								if (item != textureGeometryMap.end())
								{
									item->second->addChild(geom);
								}
								else
								{
									osg::ref_ptr<osg::Group> group = new osg::Group;
									group->addChild(geom);
									textureGeometryMap.insert(std::make_pair(filename, group));
								}
							}
						}else
						{
							noTextureGeometry->addChild(geom);
						}
					}
					else
					{
						noTextureGeometry->addChild(geom);

					}
				}
			}else
			{
				noTextureGeometry->addChild(geom);
			}
		}else
		{
			noTextureGeometry->addChild(geom);
		}
	}
	void apply(osg::Group& group) override
	{
		traverse(group);
	}
	std::map<std::string, osg::ref_ptr<osg::Group>> textureGeometryMap;
	osg::ref_ptr<osg::Group> noTextureGeometry = new osg::Group;
};

class RebuildDataNodeVisitorProxy {
public:
	osg::ref_ptr<osg::Group> output;
	RebuildDataNodeVisitorProxy(const osg::ref_ptr<osg::Node>& node)  {
		GeometryNodeVisitor gnv;
		node->accept(gnv);
		TransformNodeVisitor tnv;
		node->accept(tnv);
		RebuildDataNodeVisitor rdnvr;
		node->accept(rdnvr);
		output = rdnvr.output;
		//GroupByTextureVisitor gbtv;
		//node->accept(gbtv);
		//output = new osg::Group;
		//for (auto& item:gbtv.textureGeometryMap)
		//{
		//	output->addChild(item.second);
		//}
		//for(int i=0;i<gbtv.noTextureGeometry->getNumChildren();++i)
		//{
		//	output->addChild(gbtv.noTextureGeometry->getChild((i)));
		//}
	};

};

class TreeBuilder :public osg::Object
{
public:
	osg::ref_ptr<TileNode> rootTreeNode = new TileNode;
	TreeBuilder(const TreeBuilder& node, const osg::CopyOp& = osg::CopyOp::SHALLOW_COPY): _maxTriangleNumber(0),
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
	TreeBuilder(const osg::ref_ptr<osg::Group>& node, const unsigned int maxTriangleNumber, const int maxTreeDepth) :_maxTriangleNumber(maxTriangleNumber), _maxTreeDepth(maxTreeDepth), _maxLevel(0){
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
#endif // !OSG_GIS_PLUGINS_TREEBUILDER
