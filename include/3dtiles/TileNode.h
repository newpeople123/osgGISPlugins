#pragma once
#ifndef OSG_GIS_PLUGINS_TILENODE_H
#define OSG_GIS_PLUGINS_TILENODE_H 1
#include <osg/Node>
#include <utils/UUID.h>
#include <osg/Group>
#include <osg/ComputeBoundsVisitor>

osg::BoundingBox GetBoundingBox(const osg::ref_ptr<osg::Node>& node) {
	osg::ComputeBoundsVisitor cbbv;
	node->accept(cbbv);
	return cbbv.getBoundingBox();
}
class TileNode :public osg::Node
{
public:
	osg::ref_ptr<osg::Group> nodes;//all descendant nodes
	osg::ref_ptr<osg::Group> children;//direct child node 
	osg::ref_ptr<osg::Group> currentNodes;//current node contains nodes
	int level;//current node's level
	TileNode* parentTreeNode;
	int x, y, z;
	std::string uuid;
	double upperGeometricError, lowerGeometricError;
	std::vector<double> box;
	std::vector<double> region;
	int singleTextureMaxSize;
	std::string refine;
	TileNode() : level(0), x(0), y(0), z(0), upperGeometricError(0), lowerGeometricError(0), singleTextureMaxSize(128)
	{
		nodes = new osg::Group;
		children = new osg::Group;
		currentNodes = new osg::Group;
		parentTreeNode = nullptr;
		uuid = generateUUID();
		refine = "REPLACE";
	}
	double size;
};
#endif // !OSG_GIS_PLUGINS_TILENODE_H
