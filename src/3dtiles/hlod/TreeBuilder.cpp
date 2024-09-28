#include "3dtiles/hlod/TreeBuilder.h"
#include <osg/Group>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osgdb_gltf/material/GltfPbrMRMaterial.h>
#include <osgdb_gltf/material/GltfPbrSGMaterial.h>
using namespace osgGISPlugins;
osg::ref_ptr<B3DMTile> TreeBuilder::build()
{
	std::map<osg::Geode*, std::vector<osg::Matrixd>> geodeMatrixMap;
	for (const auto pair : _geodeMatrixMap)
	{
		if (pair.second.size() > 1)
		{
			geodeMatrixMap.insert(pair);
		}
		else
		{
			osg::ref_ptr<osg::Group> transform = new osg::MatrixTransform(pair.second[0]);
			transform->addChild(pair.first);
			_groupsToDivideList->addChild(transform);
		}
	}
	_geodeMatrixMap = geodeMatrixMap;

	osg::ref_ptr<B3DMTile> b3dmTile = generateB3DMTile();
	osg::ref_ptr<I3DMTile> i3dmTile = generateI3DMTile();
	i3dmTile->parent = b3dmTile;
	b3dmTile->children.push_back(i3dmTile);
	return b3dmTile;
}

void TreeBuilder::apply(osg::Group& group)
{
	// 备份当前矩阵
	//osg::Matrix previousMatrix = _currentMatrix;

	_currentMatrix = osg::Matrix::identity();
	// 如果是Transform节点，累积变换矩阵
	if (osg::Transform* transform = group.asTransform())
	{
		//Plan A
		//osg::Matrix localMatrix;
		//transform->computeLocalToWorldMatrix(localMatrix, this);
		//_currentMatrix.preMult(localMatrix);

		//Plan B
		//_currentMatrix = transform->asMatrixTransform()->getMatrix() * _currentMatrix;
		
		//Plan C
		const osg::MatrixList matrix_list = transform->getWorldMatrices();
		for (const osg::Matrixd& matrix : matrix_list) {
			_currentMatrix = _currentMatrix * matrix;
		}
	}

	// 继续遍历子节点
	traverse(group);

	// 恢复到之前的矩阵状态
	//_currentMatrix = previousMatrix;
}

void TreeBuilder::apply(osg::Geode& geode)
{
	osg::ref_ptr<osg::Node> node = geode.asNode();

	for (auto item = _geodeMatrixMap.begin(); item != _geodeMatrixMap.end(); ++item)
	{
		osg::ref_ptr<osg::Geode> geodeItem = item->first;
		if (Utils::compareGeode(*geodeItem, geode))
		{
			item->second.push_back(_currentMatrix);
			return;
		}
	}
	_geodeMatrixMap[node->asGeode()].push_back(_currentMatrix);

}

osg::ref_ptr<B3DMTile> TreeBuilder::divide(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<Tile> parent, const unsigned int x, const unsigned int y, const unsigned int z, const unsigned int level)
{
	return osg::ref_ptr<B3DMTile>();
}

osg::ref_ptr<B3DMTile> TreeBuilder::generateB3DMTile()
{
	double min = FLT_MAX;
	for (size_t i = 0; i < _groupsToDivideList->getNumChildren(); ++i)
	{
		osg::ref_ptr<osg::Group> item = _groupsToDivideList->getChild(i)->asGroup();
		osg::BoundingBox bb;
		bb.expandBy(item->getBound());
		const double xLength = bb.xMax() - bb.xMin();
		const double yLength = bb.yMax() - bb.yMin();
		const double zLength = bb.zMax() - bb.zMin();
		min = osg::minimum(osg::minimum(osg::minimum(min, xLength), yLength), zLength);
	}

	osg::BoundingBox rootBox;
	rootBox.expandBy(_groupsToDivideList->getBound());
	const double xLength = rootBox.xMax() - rootBox.xMin();
	const double yLength = rootBox.yMax() - rootBox.yMin();
	const double zLength = rootBox.zMax() - rootBox.zMin();
	const double max = osg::maximum(osg::maximum(xLength, yLength), zLength);
	_maxLevel = std::log2(max / min);

	osg::ref_ptr<B3DMTile> result = divide(_groupsToDivideList, rootBox);
	return result.release();
}

osg::ref_ptr<I3DMTile> TreeBuilder::generateI3DMTile()
{
	
	std::unordered_map<std::vector<osg::Matrixd>, std::vector<osg::ref_ptr<osg::Geode>>, MatrixsHash, MatrixsEqual> groupedGeodes;

	for (const auto& pair : _geodeMatrixMap) {
		const auto& geode = pair.first;
		const auto& matrices = pair.second;

		// 查找是否已经有一个具有相同矩阵列表的组  
		auto it = groupedGeodes.find(matrices);
		if (it == groupedGeodes.end()) {
			// 如果没有找到，创建一个新组  
			groupedGeodes[matrices].push_back(geode);
		}
		else {
			// 如果已经存在，添加geode到这个组  
			it->second.push_back(geode);
		}
	}

	osg::ref_ptr<I3DMTile> i3dmTile = new I3DMTile;
	i3dmTile->node = new osg::Group;
	size_t index = 0;
	for (const auto& pair : groupedGeodes)
	{
		osg::ref_ptr<I3DMTile> childI3dmTile = new I3DMTile(i3dmTile);
		childI3dmTile->node = new osg::Group;

		osg::ref_ptr<I3DMTile> grandChildI3dmTile = new I3DMTile(i3dmTile);
		grandChildI3dmTile->node = new osg::Group;
		childI3dmTile->children.push_back(grandChildI3dmTile);
		osg::ref_ptr<osg::Group> group = grandChildI3dmTile->node->asGroup();
		for (const auto matrix : pair.first)
		{
			osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
			transform->setMatrix(matrix);
			for (const osg::ref_ptr<osg::Geode> geode : pair.second)
			{
				transform->addChild(geode);
			}
			group->addChild(transform);
		}
		grandChildI3dmTile->z = index;
		i3dmTile->children.push_back(childI3dmTile);
		index++;
	}
	


	return i3dmTile;
}
