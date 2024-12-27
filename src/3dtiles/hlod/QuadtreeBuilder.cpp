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

osg::ref_ptr<B3DMTile> QuadtreeBuilder::divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent, const int x, const int y, const int z, const int level)
{
	osg::ref_ptr<B3DMTile> tile = TreeBuilder::divideB3DM(group, bounds, parent, x, y, z, level);

	tile->axis = chooseSplitAxis(bounds);
	if (TreeBuilder::processGeometryWithTextureLimit(group, bounds, tile, level))
		return tile;

	const osg::Vec3f mid = (bounds._max + bounds._min) * 0.5f;
	// 根据选择的轴进行分割
	for (int a = 0; a < 2; ++a)
	{
		for (int b = 0; b < 2; ++b)
		{
			const osg::BoundingBox childTileNodeBound = computeChildBounds(bounds, mid, tile->axis, a, b);
			osg::ref_ptr<B3DMTile> childB3DMTile = divideB3DM(group, childTileNodeBound, tile, tile->x * 2 + a, tile->y * 2 + b, 0, level + 1);
			tile->children.push_back(childB3DMTile);
		}
	}

	return tile;
}

void QuadtreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile) {
	
	if (!tile.valid() || group.empty()) return;

	tile->axis = chooseSplitAxis(bounds);
	const osg::Vec3f mid = (bounds._max + bounds._min) * 0.5f;

	std::vector<osg::BoundingBox> childrenBounds;

	for (int a = 0; a < 2; ++a) {
		for (int b = 0; b < 2; ++b) {
			osg::BoundingBox childBounds = computeChildBounds(bounds, mid, tile->axis, a, b);
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
