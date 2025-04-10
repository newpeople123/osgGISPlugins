#include "3dtiles/hlod/OcTreeBuilder.h"

osg::ref_ptr<B3DMTile> OcTreeBuilder::divideB3DM(const osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, const osg::ref_ptr<B3DMTile> parent, const int x, const int y, const int z, const int level)
{
	osg::ref_ptr<B3DMTile> tile = TreeBuilder::divideB3DM(group, bounds, parent, x, y, z, level);

	if (TreeBuilder::processB3DMWithMeshDrawcallCommandLimit(group, bounds, tile))
		return tile;

	const osg::Vec3d mid = bounds.center();
	// 根据选择的轴进行分割
	for (int a = 0; a < 2; ++a)
	{
		for (int b = 0; b < 2; ++b)
		{
			for (int c = 0; c < 2; ++c)
			{
				if (group->getNumChildren() > 0)
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
					if (childB3DMTile->node.valid())
						tile->children.push_back(childB3DMTile);
				}
				else
					return tile;
			}
		}
	}

	return tile;
}

void OcTreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, const osg::ref_ptr<I3DMTile> tile)
{
	if (!tile.valid() || group.empty()) return;

	if (TreeBuilder::processI3DM(group, bounds, tile))
		return;

	const osg::Vec3d mid = bounds.center();

		for (size_t i = 0; i < tile->children.size(); ++i)
		{
			for (int a = 0; a < 2; ++a) {
				for (int b = 0; b < 2; ++b) {
					for (int c = 0; c < 2; ++c) {
						if (group.size() > 0)
						{
							osg::BoundingBox childBound;

							// 计算子空间的包围盒
							childBound._min.set(
								a ? mid.x() : bounds._min.x(),
								b ? mid.y() : bounds._min.y(),
								c ? mid.z() : bounds._min.z()
							);
							childBound._max.set(
								a ? bounds._max.x() : mid.x(),
								b ? bounds._max.y() : mid.y(),
								c ? bounds._max.z() : mid.z()
							);

							divideI3DM(group, childBound, dynamic_cast<I3DMTile*>(tile->children.at(i).get()));
						}
						else
							return;
					}
				}
			}
		}
}

