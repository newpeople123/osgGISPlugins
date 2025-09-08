#include "3dtiles/hlod/QuadTreeBuilder.h"
int QuadTreeBuilder::chooseSplitAxis(const osg::BoundingBox& bounds)
{
	float xSpan = bounds._max.x() - bounds._min.x();
	float ySpan = bounds._max.y() - bounds._min.y();
	float zSpan = bounds._max.z() - bounds._min.z();

	// 返回跨度最小的轴
	if (xSpan <= ySpan && xSpan <= zSpan) return 0;
	if (ySpan <= xSpan && ySpan <= zSpan) return 1;
	return 2;
}

osg::BoundingBox QuadTreeBuilder::computeChildBounds(const osg::BoundingBox& bounds, int axis, int a, int b)
{
	osg::BoundingBox childBound;

	// 根据选择的切分轴进行切分
	if (axis == 0) {  // x轴是最小跨度轴
		childBound._min.set(bounds._min.x(), a ? bounds.center().y() : bounds._min.y(), b ? bounds.center().z() : bounds._min.z());
		childBound._max.set(bounds._max.x(), a ? bounds._max.y() : bounds.center().y(), b ? bounds._max.z() : bounds.center().z());
	}
	else if (axis == 1) {  // y轴是最小跨度轴
		childBound._min.set(a ? bounds.center().x() : bounds._min.x(), bounds._min.y(), b ? bounds.center().z() : bounds._min.z());
		childBound._max.set(a ? bounds._max.x() : bounds.center().x(), bounds._max.y(), b ? bounds._max.z() : bounds.center().z());
	}
	else {  // z轴是最小跨度轴
		childBound._min.set(a ? bounds.center().x() : bounds._min.x(), b ? bounds.center().y() : bounds._min.y(), bounds._min.z());
		childBound._max.set(a ? bounds._max.x() : bounds.center().x(), b ? bounds._max.y() : bounds.center().y(), bounds._max.z());
	}

	return childBound;
}

osg::ref_ptr<B3DMTile> QuadTreeBuilder::divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent, const int x, const int y, const int z, const int level)
{

	osg::ref_ptr<B3DMTile> tile = TreeBuilder::divideB3DM(group, bounds, parent, x, y, z, level);

	if (TreeBuilder::processB3DMWithMeshDrawcallCommandLimit(group, bounds, tile))
		return tile;

	const int axis = chooseSplitAxis(bounds);
	// 根据选择的轴进行分割
	for (size_t a = 0; a < 2; ++a)
	{
		for (size_t b = 0; b < 2; ++b)
		{
			if (group->getNumChildren() > 0)
			{
				const osg::BoundingBox childTileNodeBound = computeChildBounds(bounds, axis, a, b);
				osg::ref_ptr<B3DMTile> childB3DMTile = divideB3DM(group, childTileNodeBound, tile, tile->x * 2 + a, tile->y * 2 + b, 0, level + 1);
				if (childB3DMTile->node.valid())
					tile->children.push_back(childB3DMTile);
			}
			else
				return tile;
		}
	}

	return tile;
}

void QuadTreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile) {

	if (!tile.valid() || group.empty()) return;

	if (TreeBuilder::processI3DM(group, bounds, tile))
		return;

	const int axis = chooseSplitAxis(bounds);
	for (size_t i = 0; i < tile->children.size(); ++i)
	{
		for (int a = 0; a < 2; ++a) {
			for (int b = 0; b < 2; ++b) {
				if (group.size() > 0)
				{
					const osg::BoundingBox childBounds = computeChildBounds(bounds, axis, a, b);
					divideI3DM(group, childBounds, dynamic_cast<I3DMTile*>(tile->children[i].get()));
				}
				else
					return;
			}
		}

	}
}
