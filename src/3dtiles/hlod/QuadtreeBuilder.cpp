#include "3dtiles/hlod/QuadtreeBuilder.h"
osg::ref_ptr<Tile> QuadtreeBuilder::divide(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<Tile> parent, const unsigned int x, const unsigned int y, const unsigned int z, const unsigned int level)
{
    osg::ref_ptr<Tile> tile = new Tile(parent);
    tile->level = level;
    tile->x = x;
    tile->y = y;
    tile->z = z;
    tile->parent = parent;

    const unsigned int numChildren = group->getNumChildren();
    if (numChildren <= 2)
    {
        tile->node = group;
        return tile;
    }

    TextureCountVisitor textureCv;
    group->accept(textureCv);

    TriangleCountVisitor tcv;
    group->accept(tcv);

    if (textureCv.count <= _maxTextureCount && tcv.count <= _maxTriangleCount)
    {
        tile->node = group;
        return tile;
    }

    if (level >= _maxLevel || numChildren == 0)
    {
        tile->node = group;
        return tile;
    }

    // 计算每个轴上的节点分布范围
    osg::Vec3f minPos = bounds._min;
    osg::Vec3f maxPos = bounds._max;

    float xSpan = maxPos.x() - minPos.x();
    float ySpan = maxPos.y() - minPos.y();
    float zSpan = maxPos.z() - minPos.z();

    // 根据最大跨度选择分割轴
    int axis = 0; // 默认x-y轴
    if (ySpan > xSpan && ySpan > zSpan)
    {
        axis = 1; // 选择y-z轴
    }
    else if (zSpan > xSpan && zSpan > ySpan)
    {
        axis = 2; // 选择x-z轴
    }
    tile->axis = axis;

    // 计算分割的中点
    osg::Vec3f mid = (bounds._max + bounds._min) * 0.5f;

    // 根据选择的轴进行分割
    for (int a = 0; a < 2; ++a)
    {
        for (int b = 0; b < 2; ++b)
        {
            osg::BoundingBox childBounds;
            if (axis == 0)  // x-y轴分割
            {
                childBounds._min.set(
                    a ? mid.x() : bounds._min.x(),
                    b ? mid.y() : bounds._min.y(),
                    bounds._min.z()
                );
                childBounds._max.set(
                    a ? bounds._max.x() : mid.x(),
                    b ? bounds._max.y() : mid.y(),
                    bounds._max.z()
                );
            }
            else if (axis == 1)  // y-z轴分割
            {
                childBounds._min.set(
                    bounds._min.x(),
                    a ? mid.y() : bounds._min.y(),
                    b ? mid.z() : bounds._min.z()
                );
                childBounds._max.set(
                    bounds._max.x(),
                    a ? bounds._max.y() : mid.y(),
                    b ? bounds._max.z() : mid.z()
                );
            }
            else if (axis == 2)  // x-z轴分割
            {
                childBounds._min.set(
                    a ? mid.x() : bounds._min.x(),
                    bounds._min.y(),
                    b ? mid.z() : bounds._min.z()
                );
                childBounds._max.set(
                    a ? bounds._max.x() : mid.x(),
                    bounds._max.y(),
                    b ? bounds._max.z() : mid.z()
                );
            }

            osg::ref_ptr<osg::Group> childGroup = new osg::Group;
            for (unsigned int i = 0; i < group->getNumChildren(); ++i)
            {
                osg::ref_ptr<osg::Node> child = group->getChild(i);
                if (childBounds.contains(child->getBound().center()))
                {
                    childGroup->addChild(child);
                    group->removeChild(child);
                    i--;
                }
            }
            if (childGroup->getNumChildren() > 0)
            {
                tile->children.push_back(divide(childGroup, childBounds, tile, tile->x * 2 + a, tile->y * 2 + b, 0, level + 1));
            }
        }
    }

    return tile;
}
