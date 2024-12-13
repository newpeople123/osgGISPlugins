#include "3dtiles/hlod/QuadtreeBuilder.h"
int QuadtreeBuilder::chooseSplitAxis(const osg::BoundingBox& bounds)
{
	float xSpan = bounds._max.x() - bounds._min.x();
	float ySpan = bounds._max.y() - bounds._min.y();
	float zSpan = bounds._max.z() - bounds._min.z();
	if (ySpan > xSpan && ySpan > zSpan) return 1;
	if (zSpan > xSpan && zSpan > ySpan) return 2;
	return 0;
}

osg::BoundingBox QuadtreeBuilder::computeChildBounds(const osg::BoundingBox& bounds, const osg::Vec3f& mid, int axis, int a, int b)
{
	osg::BoundingBox childBound;
	if (axis == 0) {
		childBound._min.set(a ? mid.x() : bounds._min.x(), b ? mid.y() : bounds._min.y(), bounds._min.z());
		childBound._max.set(a ? bounds._max.x() : mid.x(), b ? bounds._max.y() : mid.y(), bounds._max.z());
	}
	else if (axis == 1) {
		childBound._min.set(bounds._min.x(), a ? mid.y() : bounds._min.y(), b ? mid.z() : bounds._min.z());
		childBound._max.set(bounds._max.x(), a ? bounds._max.y() : mid.y(), b ? bounds._max.z() : mid.z());
	}
	else {
		childBound._min.set(a ? mid.x() : bounds._min.x(), bounds._min.y(), b ? mid.z() : bounds._min.z());
		childBound._max.set(a ? bounds._max.x() : mid.x(), bounds._max.y(), b ? bounds._max.z() : mid.z());
	}
	return childBound;
}

osg::ref_ptr<B3DMTile> QuadtreeBuilder::divide(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<Tile> parent, const unsigned int x, const unsigned int y, const unsigned int z, const unsigned int level)
{
	osg::ref_ptr<B3DMTile> tile = new B3DMTile(dynamic_cast<B3DMTile*>(parent.get()));
	tile->level = level;
	tile->x = x;
	tile->y = y;
	tile->z = z;
	tile->parent = parent;


	if (level >= _maxLevel)
	{
		tile->node = group;
		return tile;
	}

	osg::ref_ptr<osg::Group> childGroup;
	if (!tile->node.valid())
		tile->node = new osg::Group;
	childGroup = tile->node->asGroup();

	tile->axis = chooseSplitAxis(bounds);
	osg::Vec3f mid = (bounds._max + bounds._min) * 0.5f;
	std::vector<std::tuple<int, int, osg::BoundingBox>> tileNodeChildren;
	// 根据选择的轴进行分割
	for (int a = 0; a < 2; ++a)
	{
		for (int b = 0; b < 2; ++b)
		{
			osg::BoundingBox childTileNodeBound = computeChildBounds(bounds, mid, tile->axis, a, b);

			std::vector<osg::ref_ptr<osg::Node>> needToRemove;

			// 获取所有子节点并按包围球的半径进行排序
			std::vector<osg::ref_ptr<osg::Node>> children;
			for (unsigned int i = 0; i < group->getNumChildren(); ++i)
			{
				osg::Node* node = group->getChild(i);
				children.push_back(node);
			}

			// 按照包围球的半径进行排序
			std::sort(children.begin(), children.end(), [](const osg::ref_ptr<osg::Node>& a, const osg::ref_ptr<osg::Node>& b) {
				// 计算球体半径
				const float radiusA = a->getBound().radius();
				const float radiusB = b->getBound().radius();
				return radiusA > radiusB;// 按照半径从大到小排序
				});


			size_t textureCount = 0;
			// 将排序后的子节点添加到子组，并记录需要移除的子节点
			for (auto& child : children)
			{
				const osg::BoundingSphere childBS = child->getBound();
				const osg::BoundingBox childBB = boundingSphere2BoundingBox(childBS);

				//自适应四叉树
				if (intersect(childTileNodeBound, childBB))
				{
					TextureCountVisitor tcv;
					child->accept(tcv);
					if ((textureCount + tcv.count) < 16)
					{
						textureCount += tcv.count;
						childTileNodeBound.expandBy(childBB);

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
				std::tuple<int, int, osg::BoundingBox> item = std::make_tuple(a, b, childTileNodeBound);
				tileNodeChildren.push_back(item);
			}
		}
	}

	if (childGroup->getNumChildren())
	{
		tile->refine = Refinement::ADD;
	}

	for (auto& item : tileNodeChildren)
	{
		if (group->getNumChildren() > 0)
		{
			const int a = std::get<0>(item), b = std::get<1>(item);
			const osg::BoundingBox childTileNodeBound = std::get<2>(item);
			tile->children.push_back(divide(group, childTileNodeBound, tile, tile->x * 2 + a, tile->y * 2 + b, 0, level + 1));
		}
	}
	return tile;
}

void QuadtreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile) {
	
	if (!tile.valid() || group.empty()) return;

	tile->axis = chooseSplitAxis(bounds);
	osg::Vec3f mid = (bounds._max + bounds._min) * 0.5f;

	std::vector<osg::BoundingBox> childrenBounds;

	for (int a = 0; a < 2; ++a) {
		for (int b = 0; b < 2; ++b) {
			osg::BoundingBox childBounds = computeChildBounds(bounds, mid, tile->axis, a, b);
			// 按照包围球的半径进行排序
			std::sort(group.begin(), group.end(), [](const osg::ref_ptr<I3DMTile>& a, const osg::ref_ptr<I3DMTile>& b) {
				// 计算球体半径
				const float radiusA = a->node->getBound().radius();
				const float radiusB = b->node->getBound().radius();
				return radiusA > radiusB;// 按照半径从大到小排序
				});
			std::vector<osg::ref_ptr<I3DMTile>> childGroup;
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
