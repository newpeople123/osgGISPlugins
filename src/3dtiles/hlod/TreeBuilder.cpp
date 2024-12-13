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
	std::map<osg::Geode*, std::vector<osg::ref_ptr<osg::UserDataContainer>>> geodeUseDataMap;
	for (const auto pair : _geodeMatrixMap)
	{
		if (pair.second.size() > 1)
		{
			geodeMatrixMap.insert(pair);
			geodeUseDataMap[pair.first] = _geodeUserDataMap[pair.first];
		}
		else
		{
			/*
			for (size_t i = 0; i < pair.first->getNumChildren(); ++i)
			{
				osg::ref_ptr<osg::Geode> geodeCopy = osg::clone(pair.first, osg::CopyOp::DEEP_COPY_NODES);
				geodeCopy->removeChild(0, pair.first->getNumDrawables());
				geodeCopy->addChild(pair.first->getChild(i));
				osg::ref_ptr<osg::Group> transform = new osg::MatrixTransform(pair.second[0]);
				transform->addChild(geodeCopy);
				_groupsToDivideList->addChild(transform);
			}
			*/
			
			osg::ref_ptr<osg::Group> transform = new osg::MatrixTransform(pair.second[0]);
			transform->addChild(pair.first);
			_groupsToDivideList->addChild(transform);
			
		}
	}
	_geodeMatrixMap = geodeMatrixMap;
	_geodeUserDataMap = geodeUseDataMap;

	osg::ref_ptr<B3DMTile> b3dmTile = generateB3DMTile();
	osg::ref_ptr<I3DMTile> i3dmTile = generateI3DMTile();
	regroupI3DMTile(b3dmTile, i3dmTile);
	
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
	const osg::BoundingSphere bs = node->getBound();
	if (!bs.valid())
		return;
	osg::ref_ptr<osg::UserDataContainer> userData = node->getUserDataContainer();
	for (auto item = _geodeMatrixMap.begin(); item != _geodeMatrixMap.end(); ++item)
	{
		osg::ref_ptr<osg::Geode> geodeItem = item->first;
		if (Utils::compareGeode(*geodeItem, geode))
		{
			item->second.push_back(_currentMatrix);
			_geodeUserDataMap[item->first].push_back(userData);
			return;
		}
	}
	_geodeMatrixMap[node->asGeode()].push_back(_currentMatrix);
	_geodeUserDataMap[node->asGeode()].push_back(userData);

}

osg::ref_ptr<B3DMTile> TreeBuilder::divide(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<Tile> parent, const unsigned int x, const unsigned int y, const unsigned int z, const unsigned int level)
{
	return osg::ref_ptr<B3DMTile>();
}

void TreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile)
{
	
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
	
	/*
	std::map<std::string, osg::ref_ptr<osg::Group>> stateSetGroupMap;
	osg::ref_ptr<osg::Group> largeTextureCoordGroup = new osg::Group;

	for (size_t i = 0; i < _groupsToDivideList->getNumChildren(); ++i)
	{
		osg::ref_ptr<osg::Node> child = _groupsToDivideList->getChild(i);
		osg::ref_ptr<osg::Geometry> geometry = child->asGroup()->getChild(0)->asGeode()->getChild(0)->asGeometry();
		osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();

		std::string filename = "";
		bool hasLargeTextureCoord = false; // Flag to check texture coordinate condition
		if (stateSet.valid())
		{
			osg::ref_ptr<osg::Texture> osgTexture = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
			if (osgTexture.valid())
			{
				const osg::ref_ptr<osg::Image> image = osgTexture->getImage(0);
				if (image->valid())
				{
					filename = GltfOptimizer::TextureAtlasBuilderVisitor::computeImageHash(image);
				}
			}
			if (geometry.valid())
			{
				// Check if texture coordinates are out of range
				osg::ref_ptr<osg::Vec3Array> texCoords = dynamic_cast<osg::Vec3Array*>(geometry->getTexCoordArray(0));
				if (texCoords)
				{
					for (const auto& texCoord : *texCoords)
					{
						if (fabs(texCoord.x()) > 1.0f || fabs(texCoord.y()) > 1.0f)
						{
							hasLargeTextureCoord = true;
							break;
						}
					}
				}
			}
		}

		if (hasLargeTextureCoord)
		{
			largeTextureCoordGroup->addChild(child);
		}
		else
		{
			osg::ref_ptr<osg::Group> group = stateSetGroupMap[filename];
			if (group.valid())
			{
				group->addChild(child);
			}
			else
			{
				group = new osg::Group;
				group->addChild(child);
				stateSetGroupMap[filename] = group;
			}
		}
	}

	_groupsToDivideList->removeChildren(0, _groupsToDivideList->getNumChildren());
	for (const auto& entry : stateSetGroupMap)
	{
		const std::string name = entry.first;
		if (!name.empty())
		{
			std::cout << name << std::endl;
			_groupsToDivideList->addChild(entry.second);
		}
	}
	for (size_t i = 0; i < largeTextureCoordGroup->getNumChildren(); ++i)
	{
		_groupsToDivideList->addChild(largeTextureCoordGroup->getChild(i));
	}
	*/

	osg::ref_ptr<B3DMTile> result = divide(_groupsToDivideList, rootBox);
	return result.release();
}

osg::ref_ptr<I3DMTile> TreeBuilder::generateI3DMTile()
{
	
	std::unordered_map<std::vector<osg::Matrixd>, std::vector<osg::Geode*>, MatrixsHash, MatrixsEqual> groupedGeodes;

	for (const auto& pair : _geodeMatrixMap) {
		osg::Geode* geode = pair.first;
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

	std::vector<osg::ref_ptr<I3DMTile>> i3dmTiles;
	osg::ref_ptr<osg::Group> i3dmTileGroup = new osg::Group;
	size_t index = 0;
	for (const auto& pair : groupedGeodes)
	{
		osg::ref_ptr<I3DMTile> childI3dmTile = new I3DMTile;
		childI3dmTile->node = new osg::Group;
		osg::ref_ptr<osg::Group> group = childI3dmTile->node->asGroup();
		unsigned int userDataIndex = 0;
		for (const auto matrix : pair.first)
		{
			osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
			transform->setMatrix(matrix);
			for (osg::Geode* geode : pair.second)
			{
				osg::ref_ptr<osg::UserDataContainer> userData = _geodeUserDataMap[geode].at(userDataIndex);
				osg::ref_ptr<osg::Geode> geodeCopy = osg::clone(geode, osg::CopyOp::DEEP_COPY_ALL);
				geodeCopy->setUserDataContainer(userData);
				transform->addChild(geodeCopy);
			}
			group->addChild(transform);

			userDataIndex++;
		}
		childI3dmTile->z = index;
		i3dmTiles.push_back(childI3dmTile);
		i3dmTileGroup->addChild(group);
		index++;
	}
	
	osg::ref_ptr<I3DMTile> rootI3dmTile = new I3DMTile;
	divideI3DM(i3dmTiles, boundingSphere2BoundingBox(i3dmTileGroup->getBound()), rootI3dmTile);

	return rootI3dmTile;
}

void TreeBuilder::regroupI3DMTile(osg::ref_ptr<B3DMTile> b3dmTile, osg::ref_ptr<I3DMTile> i3dmTile)
{
	if (!i3dmTile.valid()) return;
	if (!b3dmTile.valid()) return;
	if (b3dmTile->node.valid())
	{
		const osg::BoundingBox b3dmTileBB = boundingSphere2BoundingBox(b3dmTile->node->getBound());
		
		for (auto it=i3dmTile->children.begin();it!= i3dmTile->children.end();)
		{
			const osg::BoundingBox i3dmTileBB = boundingSphere2BoundingBox(it->get()->node->getBound());
			if (intersect(b3dmTileBB, i3dmTileBB))
			{
				b3dmTile->children.push_back(it->get());
				it->get()->parent = b3dmTile;
				it = i3dmTile->children.erase(it);
				continue;
			}
			++it;
		}		
	}
	//TODO:If i3dm tiles are outside the b3dm tree range
	if (i3dmTile->children.size())
	{
		for (auto& childTile : b3dmTile->children)
		{
			osg::ref_ptr<B3DMTile> b3dmChildTile = dynamic_cast<B3DMTile*>(childTile.get());
			if (b3dmChildTile.valid())
			{
				regroupI3DMTile(b3dmChildTile, i3dmTile);
			}
		}
	}
}

osg::BoundingBox TreeBuilder::boundingSphere2BoundingBox(const osg::BoundingSphere& bs)
{
	const osg::Vec3f center = bs.center();
	const float radius = bs.radius();
	const osg::Vec3f min = center - osg::Vec3f(radius, radius, radius);
	const osg::Vec3f max = center + osg::Vec3f(radius, radius, radius);
	return osg::BoundingBox(min, max);
}

float TreeBuilder::calculateBoundingBoxVolume(const osg::BoundingBox& box)
{
	float width = box._max.x() - box._min.x();
	float height = box._max.y() - box._min.y();
	float depth = box._max.z() - box._min.z();

	return (width > 0 && height > 0 && depth > 0) ? (width * height * depth) : 0.0f;
}

bool TreeBuilder::intersect(const osg::BoundingBox& parentBB, const osg::BoundingBox& childBB)
{
	const osg::BoundingBox intersectBB = parentBB.intersect(childBB);
	if (intersectBB.valid())
	{
		if (calculateBoundingBoxVolume(intersectBB) * 4.0 > calculateBoundingBoxVolume(childBB))
		{
			return true;
		}
	}
	return false;
}
