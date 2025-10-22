#include "3dtiles/hlod/TreeBuilder.h"
#include <osg/Group>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osgdb_gltf/material/GltfPbrMRMaterial.h>
#include <osgdb_gltf/material/GltfPbrSGMaterial.h>
#ifdef USE_TBB_PARALLEL
#include <tbb/parallel_for.h>
#endif // USE_TBB_PARALLEL
using namespace osgGISPlugins;
osg::ref_ptr<B3DMTile> TreeBuilder::build()
{
	for (size_t i = 0; i < _geodes.size(); ++i)
	{
		osg::MatrixList matrixs = _matrixs.at(i);
		osg::ref_ptr<osg::Geode> gnode = _geodes.at(i);
		if (matrixs.size() > 1)
		{
			_instanceGeodes.push_back(gnode);
			_instanceMatrixs.push_back(matrixs);
			std::vector<osg::ref_ptr<osg::UserDataContainer>> instanceUserDatas = _dataContainers.at(i);
			_instanceDataContainers.push_back(instanceUserDatas);
		}
		else
		{
			const unsigned int num = gnode->getNumDrawables();
			for (size_t j = 0; j < num; ++j)
			{
				//osg::ref_ptr<osg::Geode> geodeCopy = osg::clone(gnode.get(), osg::CopyOp::DEEP_COPY_NODES | osg::CopyOp::DEEP_COPY_USERDATA);
				//geodeCopy->removeChild(0, num);
				osg::ref_ptr<osg::Geode> geodeCopy = new osg::Geode;
				geodeCopy->setName(gnode->getName());
				geodeCopy->setUserDataContainer(gnode->getUserDataContainer());
				geodeCopy->addChild(gnode->getChild(j));
				for (size_t k = 0; k < matrixs.size(); ++k)
				{
					osg::ref_ptr<osg::Group> transform = new osg::MatrixTransform(matrixs[k]);
					transform->addChild(geodeCopy);
					transform->computeBound();
					_groupsToDivideList->addChild(transform);
				}
			}
		}
	}

	_geodes.clear();
	_matrixs.clear();
	_dataContainers.clear();

	osg::ref_ptr<B3DMTile> rootTile = new B3DMTile;
	osg::ref_ptr<B3DMTile> b3dmTile = generateB3DMTile();
	b3dmTile->parent = rootTile;
	rootTile->children.push_back(b3dmTile);
	osg::ref_ptr<I3DMTile> i3dmTile = generateI3DMTile();
	regroupI3DMTile(b3dmTile, i3dmTile);
	if (i3dmTile->children.size())
	{
		for (auto it = i3dmTile->children.begin(); it != i3dmTile->children.end();)
		{
			it->get()->parent = rootTile;
			rootTile->children.push_back(it->get());
			++it;
		}
	}
	return rootTile;
}

void TreeBuilder::pushMatrix(const osg::Matrix& matrix)
{
	_matrixStack.push_back(_currentMatrix);
	_currentMatrix = matrix * _currentMatrix;
}

void TreeBuilder::popMatrix()
{
	if (!_matrixStack.empty())
	{
		_currentMatrix = _matrixStack.back();
		_matrixStack.pop_back();
	}
}

void TreeBuilder::apply(osg::Transform& transform)
{
	osg::Matrix matrix;
	if (transform.computeLocalToWorldMatrix(matrix, this))
	{
		pushMatrix(matrix);
		traverse(transform);
		popMatrix();
	}
}

void TreeBuilder::apply(osg::Geode& geode)
{
	osg::ref_ptr<osg::Geode> gnode = geode.asGeode();
	if (gnode->getNumDrawables() == 0)
		return;
	osg::ref_ptr<osg::UserDataContainer> userData = gnode->getUserDataContainer();
	int index = -1;
#ifndef USE_TBB_PARALLEL
	for (size_t i = 0; i < _geodes.size(); ++i)
	{
		if (Utils::compareGeode(_geodes.at(i), gnode))
		{
			index = i;
			break;
		}
	}
#else
	std::atomic<size_t> foundIndex(_geodes.size());  // 初始值设为 size() 表示“未找到”
	tbb::task_group_context context;
	tbb::parallel_for(tbb::blocked_range<size_t>(0, _geodes.size()), [&](auto& range) {
		for (size_t i = range.begin(); i != range.end(); ++i) {
			if (context.is_group_execution_cancelled())
				return;
			if (Utils::compareGeode(_geodes[i], gnode))
			{
				foundIndex.store(i, std::memory_order_relaxed);
				context.cancel_group_execution(); // 通知所有任务取消
				return;
			}
		}
		}, context);
	index = (foundIndex.load() == _geodes.size()) ? -1 : foundIndex.load();
#endif // !USE_TBB_PARALLEL

	if (index != -1)
	{
		_matrixs.at(index).push_back(_currentMatrix);
		_dataContainers.at(index).push_back(userData);
	}
	else
	{
		_geodes.push_back(gnode);

		osg::MatrixList matrixList;
		matrixList.push_back(_currentMatrix);
		_matrixs.push_back(matrixList);

		std::vector<osg::ref_ptr<osg::UserDataContainer>> userDatas;
		userDatas.push_back(userData);
		_dataContainers.push_back(userDatas);
	}
}

osg::ref_ptr<B3DMTile> TreeBuilder::divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent, const int x, const int y, const int z, const int level)
{
	// 创建新瓦片并设置基本属性
	osg::ref_ptr<B3DMTile> tile = new B3DMTile(dynamic_cast<B3DMTile*>(parent.get()));
	tile->level = level;
	tile->x = x;
	tile->y = y;
	tile->z = z;
	tile->parent = parent;

	// 子类实现具体的分割逻辑
	return tile;
}

void TreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile)
{
}

osg::ref_ptr<B3DMTile> TreeBuilder::generateB3DMTile()
{
	std::vector<osg::ref_ptr<osg::StateSet>> uniqueStateSets;
	std::vector<osg::ref_ptr<osg::Group>> stateSetGroup;
	osg::ref_ptr<osg::Group> largeTextureCoordGroup = new osg::Group;

	for (size_t i = 0; i < _groupsToDivideList->getNumChildren(); ++i)
	{
		const osg::ref_ptr<osg::Group> child = _groupsToDivideList->getChild(i)->asGroup();
		const osg::ref_ptr<osg::Geode> childGeode = child->getChild(0)->asGeode();
		osg::ref_ptr<osg::Geometry> geometry = childGeode->getChild(0)->asGeometry();
		if (geometry.valid())
		{
			osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();

			bool hasLargeTextureCoord = false; // Flag to check texture coordinate condition
			if (stateSet.valid())
			{
				// Check if texture coordinates are out of range
				osg::ref_ptr<osg::Vec3Array> texCoords = dynamic_cast<osg::Vec3Array*>(geometry->getTexCoordArray(0));
				if (texCoords)
				{
					for (const auto& texCoord : *texCoords)
					{
						// Check if x or y is outside the [0, 1] range
						if (texCoord.x() < 0.0f || texCoord.x() > 1.0f || texCoord.y() < 0.0f || texCoord.y() > 1.0f)
						{
							hasLargeTextureCoord = true;
							break;
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
				int index = -1;
#ifndef USE_TBB_PARALLEL
				for (size_t j = 0; j < uniqueStateSets.size(); j++)
				{
					const osg::ref_ptr<osg::StateSet> key = uniqueStateSets.at(j);
					if (Utils::compareStateSet(key, stateSet))
					{
						index = j;
						break;
					}

				}
#else
				std::atomic<size_t> foundIndex(_geodes.size());  // 初始值表示“未找到”
				tbb::task_group_context context;                // 用于提前取消任务

				tbb::parallel_for(tbb::blocked_range<size_t>(0, uniqueStateSets.size()),
					[&](const tbb::blocked_range<size_t>& range) {
						for (size_t j = range.begin(); j != range.end(); ++j) {
							// 检查是否已取消
							if (context.is_group_execution_cancelled())
								return;

							const osg::ref_ptr<osg::StateSet> key = uniqueStateSets.at(j);
							if (Utils::compareStateSet(key, stateSet))
							{
								foundIndex.store(j, std::memory_order_relaxed);
								context.cancel_group_execution(); // 通知所有任务取消
								return;
							}
						}
					}, context);

				index = (foundIndex.load() == _geodes.size()) ? -1 : foundIndex.load();
#endif // !USE_TBB_PARALLEL

				if (index != -1)
					stateSetGroup[index]->addChild(child);
				else
				{
					osg::ref_ptr<osg::Group> val = new osg::Group;
					val->addChild(child);
					stateSetGroup.push_back(val);
					uniqueStateSets.push_back(stateSet);
				}
			}
		}
	}


	std::vector<osg::Node*> children;
	const unsigned int num = _groupsToDivideList->getNumChildren();
	for (size_t i = 0; i < num; ++i) {
		children.push_back(_groupsToDivideList->getChild(i));
	}
	std::sort(children.begin(), children.end(), sortNodeByRadius);
	osg::ref_ptr<osg::Group> tempGroup = new osg::Group;
	for (size_t i = 0; i < num; ++i) {
		tempGroup->addChild(children[i]);
	}
	_groupsToDivideList = tempGroup;

	double min = FLT_MAX;
	osg::BoundingBox rootBox;
	for (size_t i = 0; i < _groupsToDivideList->getNumChildren(); ++i)
	{
		osg::ref_ptr<osg::Node> item = _groupsToDivideList->getChild(i);
		const osg::BoundingBox bb = computeBoundingBox(item);
		if (bb.valid())
		{
			double xLength = bb.xMax() - bb.xMin();
			xLength = xLength == 0.0 ? FLT_MAX : xLength;
			double yLength = bb.yMax() - bb.yMin();
			yLength = yLength == 0.0 ? FLT_MAX : yLength;
			double zLength = bb.zMax() - bb.zMin();
			zLength = zLength == 0.0 ? FLT_MAX : zLength;
			min = osg::minimum(osg::minimum(osg::minimum(min, xLength), yLength), zLength);
			rootBox.expandBy(bb);
		}
	}

	const double xLength = rootBox.xMax() - rootBox.xMin();
	const double yLength = rootBox.yMax() - rootBox.yMin();
	const double zLength = rootBox.zMax() - rootBox.zMin();
	const double max = osg::maximum(osg::maximum(xLength, yLength), zLength);
	_config.setMaxLevel(std::log2(max / min));
	osg::ref_ptr<B3DMTile> result = divideB3DM(_groupsToDivideList, rootBox);
	if (_groupsToDivideList->getNumChildren())
		OSG_NOTICE << "b3dmTiles' length > 0!" << std::endl;
	return result.release();
}

osg::ref_ptr<I3DMTile> TreeBuilder::generateI3DMTile()
{
	std::vector<osg::ref_ptr<I3DMTile>> i3dmTiles;
	osg::ref_ptr<osg::Group> i3dmTileGroup = new osg::Group;

	for (size_t i = 0; i < _instanceGeodes.size(); i++)
	{
		osg::ref_ptr<I3DMTile> childI3dmTile = new I3DMTile;
		childI3dmTile->node = new osg::Group;
		osg::ref_ptr<osg::Group> group = childI3dmTile->node->asGroup();

		const osg::ref_ptr<osg::Geode> gnode = _instanceGeodes.at(i);
		const osg::MatrixList matrixs = _instanceMatrixs.at(i);
		const std::vector<osg::ref_ptr<osg::UserDataContainer>> userDatas = _instanceDataContainers.at(i);
		for (size_t j = 0; j < matrixs.size(); j++)
		{
			const osg::Matrixd matrix = matrixs.at(j);
			const osg::ref_ptr<osg::UserDataContainer> userData = userDatas.at(j);

			osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
			transform->setMatrix(matrix);
			//osg::ref_ptr<osg::Geode> geodeCopy = osg::clone(gnode.get(), osg::CopyOp::SHALLOW_COPY);
			osg::ref_ptr<osg::Geode> geodeCopy = new osg::Geode;
			geodeCopy->setName(gnode->getName());
			geodeCopy->setUserDataContainer(userData);
			for (size_t k = 0; k < gnode->getNumChildren(); ++k) {
				geodeCopy->addChild(gnode->getChild(k));
			}
			transform->addChild(geodeCopy);
			group->addChild(transform);
		}
		childI3dmTile->z = i;
		i3dmTiles.push_back(childI3dmTile);
		i3dmTileGroup->addChild(group);
	}

	osg::ref_ptr<I3DMTile> rootI3dmTile = new I3DMTile;
	std::sort(i3dmTiles.begin(), i3dmTiles.end(), sortTileNodeByRadius);
	divideI3DM(i3dmTiles, computeBoundingBox(i3dmTileGroup), rootI3dmTile);
	if (i3dmTiles.size())
		OSG_NOTICE << "i3dmTiles' length > 0!" << std::endl;
	return rootI3dmTile;
}

void TreeBuilder::regroupI3DMTile(osg::ref_ptr<B3DMTile> b3dmTile, osg::ref_ptr<I3DMTile> i3dmTile)
{
	if (!i3dmTile.valid()) return;
	if (!b3dmTile.valid()) return;
	if (b3dmTile->node.valid())
	{
		const osg::BoundingBox b3dmTileBB = computeBoundingBox(b3dmTile->node);

		for (auto it = i3dmTile->children.begin(); it != i3dmTile->children.end();)
		{
			const osg::BoundingBox i3dmTileBB = computeBoundingBox(it->get()->node);
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

osg::BoundingBox TreeBuilder::computeBoundingBox(const osg::ref_ptr<osg::Node> node)
{
	osg::ComputeBoundsVisitor cbv;
	node->accept(cbv);
	const osg::BoundingBox bbox = cbv.getBoundingBox();
	return bbox;
}

double TreeBuilder::calculateBoundingBoxVolume(const osg::BoundingBox& box)
{
	float width = box._max.x() - box._min.x();
	float height = box._max.y() - box._min.y();
	float depth = box._max.z() - box._min.z();

	return (width > 0 && height > 0 && depth > 0) ? (width * height * depth) : 0.0f;
}

bool TreeBuilder::intersect(const osg::BoundingBox& tileBBox, const osg::BoundingBox& nodeBBox)
{
	if (!tileBBox.valid() || !nodeBBox.valid())
		return false;

	if (tileBBox.contains(nodeBBox._min) && tileBBox.contains(nodeBBox._max))
		return true;

	if (tileBBox.intersects(nodeBBox))
	{
		const osg::Vec3d center = nodeBBox.center();
		if (tileBBox.contains(center))
			return true;
	}

	return false;
}

bool TreeBuilder::sortTileNodeByRadius(const osg::ref_ptr<Tile>& a, const osg::ref_ptr<Tile>& b)
{
	const float radiusA = a->node->getBound().radius();
	const float radiusB = b->node->getBound().radius();
	return radiusA > radiusB;// 按照半径从大到小排序
}

bool TreeBuilder::sortNodeByRadius(const osg::ref_ptr<osg::Node>& a, const osg::ref_ptr<osg::Node>& b)
{
	const float radiusA = a->getBound().radius();
	const float radiusB = b->getBound().radius();
	return radiusA > radiusB;// 按照半径从大到小排序
}

void TreeBuilder::processOverSizedNodes()
{
	std::vector<osg::ref_ptr<osg::Node>> oversizedNodes;

	const unsigned int size = _groupsToDivideList->getNumChildren();
#ifdef USE_TBB_PARALLEL
	std::vector<unsigned int> triangleCounts(size);

	tbb::parallel_for(size_t(0), (size_t)size, [&](size_t i) {
		osg::ref_ptr<osg::Node> node = _groupsToDivideList->getChild(static_cast<unsigned int>(i));
		Utils::TriangleCounterVisitor cv;
		node->accept(cv);
		triangleCounts[i] = cv.count;
		});

	// 第一步：识别需要拆分的节点
	for (unsigned int i = 0; i < size; ++i)
	{
		osg::ref_ptr<osg::Node> node = _groupsToDivideList->getChild(i);
		// 检查三角形数量
		if (triangleCounts[i] > _config.getMaxTriangleCount())
		{
			if (node->asGroup()->getNumChildren() > 1)
				oversizedNodes.push_back(node);
		}
	}
#else
	// 第一步：识别需要拆分的节点
	for (unsigned int i = 0; i < size; ++i)
	{
		osg::ref_ptr<osg::Node> node = _groupsToDivideList->getChild(i);
		// 检查三角形数量
		Utils::TriangleCounterVisitor cv;
		node->accept(cv);
		if (cv.count > _config.getMaxTriangleCount())
		{
			if (node->asGroup()->getNumChildren() > 1)
				oversizedNodes.push_back(node);
		}
	}
#endif // USE_TBB_PARALLEL

	// 第二步：处理需要拆分的节点
	for (auto& node : oversizedNodes)
	{
		// 从原始列表中移除
		_groupsToDivideList->removeChild(node);

		// 获取节点的包围盒
		osg::BoundingBox bb = computeBoundingBox(node);

		// 创建空间划分网格
		const int gridSize = 2; // 可以根据需要调整
		std::vector<osg::ref_ptr<osg::Group>> subGroups;
		for (int i = 0; i < gridSize * gridSize * gridSize; ++i)
		{
			subGroups.push_back(new osg::Group);
		}

		// 遍历节点中的几何体，根据空间位置分配到不同的子组
		osg::Group* group = node->asGroup();
		if (group)
		{
			for (unsigned int i = 0; i < group->getNumChildren(); ++i)
			{
				osg::ref_ptr<osg::Node> child = group->getChild(i);
				osg::BoundingSphere bs = child->getBound();
				osg::Vec3 center = bs.center();

				// 计算在哪个子空间
				int xIndex = (center.x() - bb._min.x()) / (bb._max.x() - bb._min.x()) * gridSize;
				int yIndex = (center.y() - bb._min.y()) / (bb._max.y() - bb._min.y()) * gridSize;
				int zIndex = (center.z() - bb._min.z()) / (bb._max.z() - bb._min.z()) * gridSize;

				xIndex = osg::clampBetween(xIndex, 0, gridSize - 1);
				yIndex = osg::clampBetween(yIndex, 0, gridSize - 1);
				zIndex = osg::clampBetween(zIndex, 0, gridSize - 1);

				int index = xIndex + yIndex * gridSize + zIndex * gridSize * gridSize;
				subGroups[index]->addChild(child);
			}
		}

		// 将非空的子组添加回主列表
		for (auto& subGroup : subGroups)
		{
			if (subGroup->getNumChildren() > 0)
			{
				_groupsToDivideList->addChild(subGroup);
			}
		}
	}

	if (oversizedNodes.size() > 0) processOverSizedNodes();
}

bool TreeBuilder::processB3DMWithMeshDrawcallCommandLimit(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, const osg::ref_ptr<B3DMTile> tile)
{
	if (tile->level >= _config.getMaxLevel())
	{
		tile->node = new osg::Group;
		const unsigned int size = group->getNumChildren();
		for (size_t i = 0; i < size; i++)
		{
			tile->node->asGroup()->addChild(group->getChild(i));
		}
		group->removeChildren(0, size);
		return true;
	}

	unsigned int maxDrawcallCommandCount = (tile->level + 1) * _config.InitDrawcallCommandCount;
	maxDrawcallCommandCount = maxDrawcallCommandCount > _config.getMaxDrawcallCommandCount() ? _config.getMaxDrawcallCommandCount() : maxDrawcallCommandCount;

	osg::ref_ptr<osg::Group> childGroup;
	if (!tile->node.valid())
		tile->node = new osg::Group;
	childGroup = tile->node->asGroup();


	std::vector<osg::ref_ptr<osg::Node>> needToRemove;

	// 获取所有子节点并按包围球的半径进行排序
	std::vector<osg::ref_ptr<osg::Node>> children;
	for (unsigned int i = 0; i < group->getNumChildren(); ++i)
	{
		children.push_back(group->getChild(i));
	}
	std::sort(children.begin(), children.end(), sortNodeByRadius);

	unsigned int drawcallCommandCount = 0, triangleCount = 0;
	osg::ref_ptr<osg::Node> outTriangleCountNode = nullptr;

	const unsigned int size = children.size();
#ifdef USE_TBB_PARALLEL
	std::vector<unsigned int> triangleCounts(size);

	tbb::parallel_for(size_t(0), (size_t)size, [&](size_t i) {
		osg::ref_ptr<osg::Node> node = children[i];
		Utils::TriangleCounterVisitor cv;
		node->accept(cv);
		triangleCounts[i] = cv.count;
		});

	// 将排序后的子节点添加到子组，并记录需要移除的子节点
	for (size_t i = 0; i < size; ++i)
	{
		auto& child = children[i];
		if (triangleCount >= _config.getMaxTriangleCount())
			break;

		const osg::BoundingBox childBB = computeBoundingBox(child);
		if (intersect(bounds, childBB))
		{
			childGroup->addChild(child);
			Utils::DrawcallCommandCounterVisitor dccv;
			childGroup->accept(dccv);
			if (dccv.getCount() <= maxDrawcallCommandCount)
			{
				if (triangleCounts[i] + triangleCount <= _config.getMaxTriangleCount())
				{
					triangleCount += triangleCounts[i];
					drawcallCommandCount = dccv.getCount();
					needToRemove.push_back(child);
				}
				else
				{
					childGroup->removeChild(child);
					if (!outTriangleCountNode.valid())
						outTriangleCountNode = child;
				}
			}
			else
			{
				childGroup->removeChild(child);
				if (!outTriangleCountNode.valid())
					outTriangleCountNode = child;
			}
		}
	}
#else
	// 将排序后的子节点添加到子组，并记录需要移除的子节点
	for (size_t i = 0; i < size; ++i)
	{
		auto& child = children[i];
		if (triangleCount >= _config.getMaxTriangleCount())
			break;

		const osg::BoundingBox childBB = computeBoundingBox(child);
		if (intersect(bounds, childBB))
		{
			childGroup->addChild(child);
			Utils::DrawcallCommandCounterVisitor dccv;
			childGroup->accept(dccv);

			Utils::TriangleCounterVisitor cv;
			child->accept(cv);
			if (dccv.getCount() <= maxDrawcallCommandCount)
			{
				if (cv.count + triangleCount <= _config.getMaxTriangleCount())
				{
					triangleCount += cv.count;
					drawcallCommandCount = dccv.getCount();
					needToRemove.push_back(child);
				}
				else
				{
					childGroup->removeChild(child);
					if (!outTriangleCountNode.valid())
						outTriangleCountNode = child;
				}
			}
			else
			{
				childGroup->removeChild(child);
				if (!outTriangleCountNode.valid())
					outTriangleCountNode = child;
			}
		}
	}
#endif // USE_TBB_PARALLEL


	if (outTriangleCountNode.valid() && childGroup->getNumChildren() == 0)
	{
		childGroup->addChild(outTriangleCountNode);
		group->removeChild(outTriangleCountNode);
	}

	if (needToRemove.size())
	{
		for (auto& child : needToRemove)
		{
			group->removeChild(child);
		}
	}
	if (childGroup->getNumChildren() == 0)
	{
		tile->node = nullptr;
		return true;
	}

	return group->getNumChildren() == 0;
}

bool TreeBuilder::processI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, const osg::ref_ptr<I3DMTile> tile)
{
	for (auto it = group.begin(); it != group.end();) {
		osg::ref_ptr<I3DMTile> child = it->get();
		if (intersect(bounds, computeBoundingBox(child->node))) {
			child->parent = tile;
			tile->children.push_back(child);
			it = group.erase(it);
			continue;
		}
		++it;
	}
	if (tile->children.size())
		return false;
	return true;
}
