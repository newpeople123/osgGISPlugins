#include "3dtiles/hlod/QuadtreeBuilder.h"
int QuadtreeBuilder::chooseSplitAxis(const osg::BoundingBox& bounds)
{
	float xSpan = bounds._max.x() - bounds._min.x();
	float ySpan = bounds._max.y() - bounds._min.y();
	float zSpan = bounds._max.z() - bounds._min.z();

	// 返回跨度最小的轴
	if (xSpan <= ySpan && xSpan <= zSpan) return 0;
	if (ySpan <= xSpan && ySpan <= zSpan) return 1;
	return 2;
}

osg::BoundingBox QuadtreeBuilder::computeChildBounds(const osg::BoundingBox& bounds, const osg::Vec3f& mid, int axis, int a, int b)
{
	osg::BoundingBox childBound;

	// 根据选择的切分轴进行切分
	if (axis == 0) {  // x轴是最小跨度轴
		childBound._min.set(bounds._min.x(), a ? mid.y() : bounds._min.y(), b ? mid.z() : bounds._min.z());
		childBound._max.set(bounds._max.x(), b ? mid.y() : bounds._max.y(), a ? mid.z() : bounds._max.z());
	}
	else if (axis == 1) {  // y轴是最小跨度轴
		childBound._min.set(a ? mid.x() : bounds._min.x(), bounds._min.y(), b ? mid.z() : bounds._min.z());
		childBound._max.set(b ? mid.x() : bounds._max.x(), bounds._max.y(), a ? mid.z() : bounds._max.z());
	}
	else {  // z轴是最小跨度轴
		childBound._min.set(a ? mid.x() : bounds._min.x(), b ? mid.y() : bounds._min.y(), bounds._min.z());
		childBound._max.set(b ? mid.x() : bounds._max.x(), a ? mid.y() : bounds._max.y(), bounds._max.z());
	}

	return childBound;
}

osg::ref_ptr<B3DMTile> QuadtreeBuilder::divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent, const int x, const int y, const int z, const int level)
{

	osg::ref_ptr<B3DMTile> tile = TreeBuilder::divideB3DM(group, bounds, parent, x, y, z, level);

	if (TreeBuilder::processGeometryWithMeshTextureLimit(group, bounds, tile, level))
		return tile;

	const int axis = chooseSplitAxis(bounds);
	const osg::Vec3d mid = (bounds._max + bounds._min) * 0.5f;
	// 根据选择的轴进行分割
	for (size_t a = 0; a < 2; ++a)
	{
		for (size_t b = 0; b < 2; ++b)
		{
			if (group->getNumChildren() > 0)
			{
				const osg::BoundingBox childTileNodeBound = computeChildBounds(bounds, mid, axis, a, b);
				osg::ref_ptr<B3DMTile> childB3DMTile = divideB3DM(group, childTileNodeBound, tile, tile->x * 2 + a, tile->y * 2 + b, 0, level + 1);
				if(childB3DMTile->node.valid())
					tile->children.push_back(childB3DMTile);
			}
			else
				return tile;
		}
	}
	
	return tile;
}

void QuadtreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile) {

	if (!tile.valid() || group.empty()) return;

	const int axis = chooseSplitAxis(bounds);
	const osg::Vec3d mid = (bounds._max + bounds._min) * 0.5f;

	std::vector<osg::BoundingBox> childrenBounds;

	for (int a = 0; a < 2; ++a) {
		for (int b = 0; b < 2; ++b) {
			osg::BoundingBox childBounds = computeChildBounds(bounds, mid, axis, a, b);
			for (auto it = group.begin(); it != group.end();) {
				osg::ref_ptr<I3DMTile> child = it->get();
				if (intersect(childBounds, computeBoundingBox(child->node))) {
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
