#include "3dtiles/hlod/KDTreeBuilder.h"
float KDTreeBuilder::computeSurfaceArea(const osg::BoundingBox& bounds) const {
    float dx = bounds._max.x() - bounds._min.x();
    float dy = bounds._max.y() - bounds._min.y();
    float dz = bounds._max.z() - bounds._min.z();
    return 2.0f * (dx * dy + dy * dz + dz * dx);
}

float KDTreeBuilder::evaluateSAH(const osg::BoundingBox& bounds,
    const osg::BoundingBox& leftBounds,
    const osg::BoundingBox& rightBounds,
    int leftCount, int rightCount) const {
    // 1. 计算表面积
    float totalArea = computeSurfaceArea(bounds);
    float leftArea = computeSurfaceArea(leftBounds);
    float rightArea = computeSurfaceArea(rightBounds);

    // 2. 计算概率
    float P_left = leftArea / totalArea;
    float P_right = rightArea / totalArea;
    
    // 3. 计算SAH代价
    return TRAVERSAL_COST + // 遍历成本
           INTERSECTION_COST * (
               P_left * leftCount +  // 左子树的期望代价
               P_right * rightCount  // 右子树的期望代价
           );
}

bool KDTreeBuilder::findBestSplit(const osg::ref_ptr<osg::Group>& group,
    const osg::BoundingBox& bounds,
    int& bestAxis,
    float& bestPos,
    float& bestCost) {
    bestCost = FLT_MAX;
    const size_t numObjects = group->getNumChildren();

    // 如果对象太少，不进行分割
    if (numObjects <= 1) return false;

    // 对每个轴进行评估
    for (int axis = 0; axis < 3; ++axis) {
        // 初始化桶
        std::vector<SAHBucket> buckets(NUM_BUCKETS);
        float axisMin = bounds._min[axis];
        float axisMax = bounds._max[axis];
        float axisExtent = axisMax - axisMin;

        // 将对象分配到桶中
        for (size_t i = 0; i < numObjects; ++i) {
            osg::ref_ptr<osg::Node> node = group->getChild(i);
            osg::BoundingBox nodeBounds = computeBoundingBox(node);
            float centroid = (nodeBounds._min[axis] + nodeBounds._max[axis]) * 0.5f;

            int b = NUM_BUCKETS * ((centroid - axisMin) / axisExtent);
            if (b == NUM_BUCKETS) b = NUM_BUCKETS - 1;
            buckets[b].count++;
            buckets[b].bounds.expandBy(nodeBounds);
        }

        // 计算前缀和后缀的包围盒和计数
        std::vector<osg::BoundingBox> leftBoxes(NUM_BUCKETS - 1);
        std::vector<osg::BoundingBox> rightBoxes(NUM_BUCKETS - 1);
        std::vector<int> leftCounts(NUM_BUCKETS - 1);
        std::vector<int> rightCounts(NUM_BUCKETS - 1);

        osg::BoundingBox currentLeft;
        int leftSum = 0;
        for (int i = 0; i < NUM_BUCKETS - 1; ++i) {
            currentLeft.expandBy(buckets[i].bounds);
            leftSum += buckets[i].count;
            leftBoxes[i] = currentLeft;
            leftCounts[i] = leftSum;
        }

        osg::BoundingBox currentRight;
        int rightSum = 0;
        for (int i = NUM_BUCKETS - 1; i > 0; --i) {
            currentRight.expandBy(buckets[i].bounds);
            rightSum += buckets[i].count;
            rightBoxes[i - 1] = currentRight;
            rightCounts[i - 1] = rightSum;
        }

        // 评估所有可能的分割
        for (int i = 0; i < NUM_BUCKETS - 1; ++i) {
            float splitPos = axisMin + axisExtent * (float)(i + 1) / NUM_BUCKETS;

            // 如果任一侧为空，跳过这个分割
            if (leftCounts[i] == 0 || rightCounts[i] == 0) continue;

            float cost = evaluateSAH(bounds, leftBoxes[i], rightBoxes[i],
                leftCounts[i], rightCounts[i]);

            if (cost < bestCost) {
                bestAxis = axis;
                bestPos = splitPos;
                bestCost = cost;
            }
        }
    }

    // 如果分割代价高于不分割的代价，返回false
    float noSplitCost = INTERSECTION_COST * numObjects;
    return bestCost < noSplitCost;
}

osg::ref_ptr<B3DMTile> KDTreeBuilder::divideB3DM(osg::ref_ptr<osg::Group> group,
    const osg::BoundingBox& bounds,
    osg::ref_ptr<B3DMTile> parent,
    const int x, const int y, const int z,
    const int level) {
    osg::ref_ptr<B3DMTile> tile = TreeBuilder::divideB3DM(group, bounds, parent, x, y, z, level);

    // 使用SAH找到最佳分割
    int bestAxis;
    float bestPos;
    float bestCost;

    if (!findBestSplit(group, bounds, bestAxis, bestPos, bestCost)) {
        // 如果不值得分割，直接返回
        tile->node = group;
        return tile;
    }

    if (TreeBuilder::processB3DMWithMeshDrawcallCommandLimit(group, bounds, tile))
        return tile;

    // 创建左右子节点的包围盒
    osg::BoundingBox leftBounds = bounds;
    osg::BoundingBox rightBounds = bounds;
    leftBounds._max[bestAxis] = bestPos;
    rightBounds._min[bestAxis] = bestPos;

    // 创建左右子组
    osg::ref_ptr<osg::Group> leftGroup = new osg::Group;
    osg::ref_ptr<osg::Group> rightGroup = new osg::Group;

    // 分配节点到左右子组
    for (unsigned int i = 0; i < group->getNumChildren(); ++i) {
        osg::ref_ptr<osg::Node> child = group->getChild(i);
        osg::BoundingBox childBB = computeBoundingBox(child);
        float center = (childBB._min[bestAxis] + childBB._max[bestAxis]) * 0.5f;

        if (center <= bestPos) {
            leftGroup->addChild(child);
        }
        else {
            rightGroup->addChild(child);
        }
    }

    // 清空原始组
    group->removeChildren(0, group->getNumChildren());

    // 根据分割轴计算子节点的坐标
    int leftX = x, leftY = y, leftZ = z;
    int rightX = x, rightY = y, rightZ = z;

    // 使用level来确定在哪个维度上增加索引
    switch (bestAxis) {
    case 0: // X轴
        rightX = x + (1 << (level % 3));  // 在X方向上偏移2^(level%3)
        break;
    case 1: // Y轴
        rightY = y + (1 << (level % 3));  // 在Y方向上偏移2^(level%3)
        break;
    case 2: // Z轴
        rightZ = z + (1 << (level % 3));  // 在Z方向上偏移2^(level%3)
        break;
    }

    // 递归处理左右子树
    if (leftGroup->getNumChildren() > 0) {
        osg::ref_ptr<B3DMTile> leftChild = divideB3DM(leftGroup, leftBounds, tile,
            leftX, leftY, leftZ, level + 1);
        if (leftChild->node.valid()) {
            tile->children.push_back(leftChild);
        }
    }

    if (rightGroup->getNumChildren() > 0) {
        osg::ref_ptr<B3DMTile> rightChild = divideB3DM(rightGroup, rightBounds, tile,
            rightX, rightY, rightZ, level + 1);
        if (rightChild->node.valid()) {
            tile->children.push_back(rightChild);
        }
    }

    return tile;
}

void KDTreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile)
{
    if (!tile.valid() || group.empty()) return;

    if (TreeBuilder::processI3DM(group, bounds, tile))
        return;

    // 使用SAH找到最佳分割
    int bestAxis;
    float bestPos;
    float bestCost;

    // 创建临时Group用于SAH计算
    osg::ref_ptr<osg::Group> tempGroup = new osg::Group;
    for (const auto& i3dmTile : group) {
        if (i3dmTile->node.valid()) {
            tempGroup->addChild(i3dmTile->node);
        }
    }

    if (!findBestSplit(tempGroup, bounds, bestAxis, bestPos, bestCost)) {
        // 如果不值得分割，直接返回
        for (const auto& i3dmTile : group) {
            if (i3dmTile->node.valid()) {
                tile->children.push_back(i3dmTile);
            }
        }
        return;
    }

    // 创建左右子节点的包围盒
    osg::BoundingBox leftBounds = bounds;
    osg::BoundingBox rightBounds = bounds;
    leftBounds._max[bestAxis] = bestPos;
    rightBounds._min[bestAxis] = bestPos;

    // 分配实例到左右组
    std::vector<osg::ref_ptr<I3DMTile>> leftGroup, rightGroup;

    for (auto it = group.begin(); it != group.end();) {
        osg::ref_ptr<I3DMTile> instance = *it;
        if (!instance->node.valid()) {
            ++it;
            continue;
        }

        osg::BoundingBox instanceBB = computeBoundingBox(instance->node);
        float center = (instanceBB._min[bestAxis] + instanceBB._max[bestAxis]) * 0.5f;

        if (center <= bestPos) {
            leftGroup.push_back(instance);
        }
        else {
            rightGroup.push_back(instance);
        }
        it = group.erase(it);
    }

    // 为左右子组创建I3DMTile
    if (!leftGroup.empty()) {
        osg::ref_ptr<I3DMTile> leftChild = new I3DMTile;
        leftChild->parent = tile;

        // 设置左子节点的坐标（与父节点相同）
        leftChild->x = tile->x;
        leftChild->y = tile->y;
        leftChild->z = tile->z;

        tile->children.push_back(leftChild);
        divideI3DM(leftGroup, leftBounds, leftChild);
    }

    if (!rightGroup.empty()) {
        osg::ref_ptr<I3DMTile> rightChild = new I3DMTile;
        rightChild->parent = tile;

        // 设置右子节点的坐标（在分割轴上增加偏移）
        int level = 0;
        if (tile->parent) {
            // 计算当前层级
            osg::ref_ptr<Tile> current = tile;
            while (current->parent) {
                level++;
                current = current->parent;
            }
        }

        rightChild->x = tile->x;
        rightChild->y = tile->y;
        rightChild->z = tile->z;

        // 根据分割轴和层级更新坐标
        switch (bestAxis) {
        case 0: // X轴
            rightChild->x += (1 << (level % 3));
            break;
        case 1: // Y轴
            rightChild->y += (1 << (level % 3));
            break;
        case 2: // Z轴
            rightChild->z += (1 << (level % 3));
            break;
        }

        tile->children.push_back(rightChild);
        divideI3DM(rightGroup, rightBounds, rightChild);
    }
}
