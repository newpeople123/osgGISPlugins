#include "3dtiles/hlod/TreeBuilder.h"
#include <osg/Group>
#include <osg/Geode>
#include <osg/MatrixTransform>
using namespace osgGISPlugins;
osg::ref_ptr<Tile> TreeBuilder::build()
{
	osg::ref_ptr<osg::Group> node = new osg::Group;
	double min = FLT_MAX;
	for (auto& item : _groupsToDivideList)
	{
		osg::BoundingBox bb;
		bb.expandBy(item->getBound());
		const double xLength = bb.xMax() - bb.xMin();
		const double yLength = bb.yMax() - bb.yMin();
		const double zLength = bb.zMax() - bb.zMin();
		min = osg::minimum(osg::minimum(osg::minimum(min, xLength), yLength), zLength);
		node->addChild(item);
	}
	for (auto& item : _geodesToDivideList)
	{
		osg::BoundingBox bb;
		bb.expandBy(item->getBound());
		const double xLength = bb.xMax() - bb.xMin();
		const double yLength = bb.yMax() - bb.yMin();
		const double zLength = bb.zMax() - bb.zMin();
		min = osg::minimum(osg::minimum(osg::minimum(min, xLength), yLength), zLength);
		node->addChild(item);
	}

	osg::BoundingBox rootBox;
	rootBox.expandBy(node->getBound());
	const double xLength = rootBox.xMax() - rootBox.xMin();
	const double yLength = rootBox.yMax() - rootBox.yMin();
	const double zLength = rootBox.zMax() - rootBox.zMin();
	const double max = osg::maximum(osg::maximum(xLength, yLength), zLength);
	_maxLevel = std::log2(max / min);

	return divide(node, rootBox);
}

void TreeBuilder::apply(osg::Group& group)
{
	// 备份当前矩阵
	osg::Matrix previousMatrix = _currentMatrix;

	// 如果是Transform节点，累积变换矩阵
	if (osg::Transform* transform = group.asTransform())
	{
		osg::Matrix localMatrix;
		transform->computeLocalToWorldMatrix(localMatrix, this);
		_currentMatrix.preMult(localMatrix);
	}

	// 继续遍历子节点
	traverse(group);

	// 恢复到之前的矩阵状态
	_currentMatrix = previousMatrix;
}

void TreeBuilder::apply(osg::Geode& geode)
{
	if (_currentMatrix == osg::Matrix::identity())
		_geodesToDivideList.insert(&geode);
	else
	{
		osg::Group* transform = new osg::MatrixTransform(_currentMatrix);
		transform->addChild(&geode);
		_groupsToDivideList.insert(transform);
	}
}
