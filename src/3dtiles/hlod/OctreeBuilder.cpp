#include "3dtiles/hlod/OctreeBuilder.h"
osg::ref_ptr<B3DMTile> OctreeBuilder::divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent, const int x, const int y, const int z, const int level)
{
	osg::ref_ptr<B3DMTile> tile = TreeBuilder::divideB3DM(group, bounds, parent, x, y, z, level);

	if (level >= _config.maxLevel)
	{
		tile->node = group;
		return tile;
	}

	osg::ref_ptr<osg::Group> childGroup;
	if (!tile->node.valid())
		tile->node = new osg::Group;
	childGroup = tile->node->asGroup();


	std::vector<osg::ref_ptr<osg::Node>> needToRemove;

	// 获取所有子节点并按包围球的半径进行排序
	std::vector<osg::ref_ptr<osg::Node>> children;
	for (unsigned int i = 0; i < group->getNumChildren(); ++i)
	{
		osg::Node* node = group->getChild(i);
		children.push_back(node);
	}

	osg::BoundingBox bb(bounds);
	unsigned int textureCount = 0;
	// 将排序后的子节点添加到子组，并记录需要移除的子节点
	for (auto& child : children)
	{
		const osg::BoundingSphere childBS = child->getBound();
		const osg::BoundingBox childBB = boundingSphere2BoundingBox(childBS);
		if (textureCount >= _config.maxTextureCount)
			break;
		//自适应四叉树
		if (intersect(bb, childBB))
		{
			Utils::TextureCounterVisitor tcv;
			child->accept(tcv);
			if ((textureCount + tcv.getCount()) <= 15)
			{
				textureCount += tcv.getCount();
				bb.expandBy(childBB);

				childGroup->addChild(child);
				needToRemove.push_back(child);
			}
		}
	}

	if (needToRemove.size())
	{
		for (auto& child : needToRemove)
		{
			group->removeChild(child);
		}
	}
	if (!childGroup->getNumChildren())
	{
		return tile;
	}

	const osg::Vec3f mid = (bounds._max + bounds._min) * 0.5f;
	// 根据选择的轴进行分割
	for (int a = 0; a < 2; ++a)
	{
		for (int b = 0; b < 2; ++b)
		{
			for (int c = 0; c < 2; ++c)
			{
				osg::BoundingBox childTileNodeBound;

				// 计算子空间的包围盒
				childTileNodeBound._min.set(
					a ? mid.x() : bounds._min.x(),
					b ? mid.y() : bounds._min.y(),
					c ? mid.z() : bounds._min.z()
				);
				childTileNodeBound._max.set(
					a ? bounds._max.x() : mid.x(),
					b ? bounds._max.y() : mid.y(),
					c ? bounds._max.z() : mid.z()
				);
				osg::ref_ptr<B3DMTile> childB3DMTile = divideB3DM(group, childTileNodeBound, tile, tile->x * 2 + a, tile->y * 2 + b, tile->z * 2 + c, level + 1);
				tile->children.push_back(childB3DMTile);
			}
		}
	}

	return tile;
}

void OctreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile)
{
	if (!tile.valid() || group.empty()) return;

	const osg::Vec3f mid = (bounds._max + bounds._min) * 0.5f;

	std::vector<osg::BoundingBox> childrenBounds;

	for (int a = 0; a < 2; ++a) {
		for (int b = 0; b < 2; ++b) {
			for (int c = 0; c < 2; ++c) {
				osg::BoundingBox childBounds;

				// 计算子空间的包围盒
				childBounds._min.set(
					a ? mid.x() : bounds._min.x(),
					b ? mid.y() : bounds._min.y(),
					c ? mid.z() : bounds._min.z()
				);
				childBounds._max.set(
					a ? bounds._max.x() : mid.x(),
					b ? bounds._max.y() : mid.y(),
					c ? bounds._max.z() : mid.z()
				);
				for (auto it = group.begin(); it != group.end();) {
					osg::ref_ptr<I3DMTile> child = it->get();
					if (intersect(childBounds, boundingSphere2BoundingBox(child->node->getBound()))) {
						child->parent = tile;
						tile->children.push_back(child);
						it = group.erase(it);
						continue;
					}
					++it;
				}
				if (tile->children.size())
					childrenBounds.push_back(childBounds);
			}
		}
	}

	for (auto& item : childrenBounds) {
		for (size_t i = 0; i < tile->children.size(); ++i)
		{
			if (group.size() > 0)
			{
				divideI3DM(group, item, dynamic_cast<I3DMTile*>(tile->children[i].get()));
			}
		}
	}
}
