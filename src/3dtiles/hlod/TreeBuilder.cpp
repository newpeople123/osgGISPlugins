#include "3dtiles/hlod/TreeBuilder.h"
#include <osg/Group>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osgViewer/Viewer>
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
	OSG_NOTICE << test.size() << " " << _groupsToDivideList.size() << " " << _geodesToDivideList.size() << std::endl;
	for (auto item = test.begin(); item != test.end(); ++item)
	{
		if (item->second.size() > 1)
		{
			OSG_NOTICE << item->second.size() << std::endl;
		}
	}
	return divide(node, rootBox);
}

bool osgGISPlugins::TreeBuilder::compareStateSet(osg::ref_ptr<osg::StateSet> stateSet1, osg::ref_ptr<osg::StateSet> stateSet2)
{
	if (!stateSet1.valid() || !stateSet2.valid())
		return false;

	if (stateSet1->getAttributeList().size() != stateSet2->getAttributeList().size())
		return false;
}

bool TreeBuilder::comparePrimitiveSet(osg::ref_ptr<osg::PrimitiveSet> pSet1, osg::ref_ptr<osg::PrimitiveSet> pSet2)
{
	if (pSet1->getMode() != pSet2->getMode())
		return false;
	if (pSet1->getType() != pSet2->getType())
		return false;
	return true;
}

bool TreeBuilder::compareGeometry(osg::ref_ptr<osg::Geometry> geom1, osg::ref_ptr<osg::Geometry> geom2)
{
	if (!(geom1.valid() == geom2.valid() && geom1.valid()))
		return false;

	//compare boundingbox
	const osg::BoundingBox bb1 = geom1->getBoundingBox();
	const osg::BoundingBox bb2 = geom2->getBoundingBox();
	if (bb1 != bb2)
		return false;

	//compare positions
	osg::ref_ptr<osg::Array> positions1 = geom1->getVertexArray();
	osg::ref_ptr<osg::Array> positions2 = geom2->getVertexArray();
	if (!(positions1.valid() == positions2.valid() && positions1.valid()))
		return false;
	if (positions1->getElementSize() != positions2->getElementSize())
		return false;
	if (positions1->getTotalDataSize() != positions2->getTotalDataSize())
		return false;


	//compare texCoords
	osg::ref_ptr<osg::Array> texCoords1 = geom1->getTexCoordArray(0);
	osg::ref_ptr<osg::Array> texCoords2 = geom2->getTexCoordArray(0);
	if (!(texCoords1.valid() == texCoords2.valid()))
		return false;
	if (texCoords1.valid())
	{
		if (texCoords1->getElementSize() != texCoords2->getElementSize())
			return false;
	}

	//compare stateset
	osg::ref_ptr<osg::StateSet> stateSet1 = geom1->getStateSet();
	osg::ref_ptr<osg::StateSet> stateSet2 = geom2->getStateSet();
	if (!compareStateSet(stateSet1, stateSet2))
		return false;

	//compare colors
	osg::ref_ptr<osg::Array> colors1 = geom1->getColorArray();
	osg::ref_ptr<osg::Array> colors2 = geom2->getColorArray();
	if (!(colors1.valid() == colors2.valid()))
		return false;
	if (colors1.valid())
	{
		if (colors1->getElementSize() != colors2->getElementSize())
			return false;
	}

	//compare primitiveset
	if (geom1->getNumPrimitiveSets() != geom2->getNumPrimitiveSets())
		return false;
	for (size_t i = 0; i < geom1->getNumPrimitiveSets(); ++i)
	{
		if (!comparePrimitiveSet(geom1->getPrimitiveSet(i), geom2->getPrimitiveSet(i)))
			return false;
	}

	return true;
}

bool TreeBuilder::compareGeode(osg::Geode& geode1, osg::Geode& geode2)
{
	if (geode1.getNumChildren() != geode2.getNumChildren())
		return false;

	const osg::BoundingBox bb1 = geode1.getBoundingBox();
	const osg::BoundingBox bb2 = geode2.getBoundingBox();
	if (bb1 != bb2)
		return false;

	for (size_t i = 0; i < geode1.getNumChildren(); ++i)
	{
		osg::ref_ptr<osg::Geometry> geom1 = geode1.getChild(i)->asGeometry();
		osg::ref_ptr<osg::Geometry> geom2 = geode2.getChild(i)->asGeometry();
		if (!compareGeometry(geom1, geom2))
			return false;	
	}

	return true;
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
	{
		test[&geode].push_back(osg::Matrix::identity());
	}
	else
	{
		osg::Group* transform = new osg::MatrixTransform(_currentMatrix);
		transform->addChild(&geode);
		_groupsToDivideList.insert(transform);

		bool bAdd = true;
		for (auto item = test.begin(); item != test.end(); ++item)
		{
			osg::Geode* geodeItem = item->first;
			if (compareGeode(*geodeItem, geode))
			{
				bAdd = false;
				item->second.push_back(_currentMatrix);
				break;
			}
		}
		if (bAdd)
		{
			test[&geode].push_back(_currentMatrix);
		}
	}
}
