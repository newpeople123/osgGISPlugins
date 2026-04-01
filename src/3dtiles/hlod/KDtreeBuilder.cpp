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
               P_left * static_cast<float>(leftCount) +  // 左子树的期望代价
               P_right * static_cast<float>(rightCount)  // 右子树的期望代价
           );
}

bool KDTreeBuilder::findBestSplit(const std::vector<NodeProperty>& nodeProps,
    const osg::BoundingBox& bounds,
    int& bestAxis,
    float& bestPos,
    float& bestCost) {

    bestCost = FLT_MAX;
    const size_t numObjects = nodeProps.size();
    if (numObjects <= 1) return false;

    // 遍历三个轴：X, Y, Z
    for (int axis = 0; axis < 3; ++axis) {
        float axisMin = bounds._min[axis];
        float axisMax = bounds._max[axis];
        float axisExtent = axisMax - axisMin;
        if (axisExtent < 1e-4) continue; // 轴过短，跳过

        std::vector<SAHBucket> buckets(NUM_BUCKETS);

        // 1. 将对象分配到桶中 (使用预存的 BoundingBox)
        for (const auto& prop : nodeProps) {
            float centroid = (prop.bb._min[axis] + prop.bb._max[axis]) * 0.5f;
            int b = static_cast<int>(NUM_BUCKETS * (centroid - axisMin) / axisExtent);
            b = std::max(0, std::min(b, NUM_BUCKETS - 1));
            buckets[b].count++;
            buckets[b].bounds.expandBy(prop.bb);
        }

        // 2. 预计算前缀和后缀代价
        std::vector<osg::BoundingBox> leftBoxes(NUM_BUCKETS - 1);
        std::vector<int> leftCounts(NUM_BUCKETS - 1);
        osg::BoundingBox currentLeft;
        int leftSum = 0;
        for (int i = 0; i < NUM_BUCKETS - 1; ++i) {
            currentLeft.expandBy(buckets[i].bounds);
            leftSum += buckets[i].count;
            leftBoxes[i] = currentLeft;
            leftCounts[i] = leftSum;
        }

        std::vector<osg::BoundingBox> rightBoxes(NUM_BUCKETS - 1);
        std::vector<int> rightCounts(NUM_BUCKETS - 1);
        osg::BoundingBox currentRight;
        int rightSum = 0;
        for (int i = NUM_BUCKETS - 1; i > 0; --i) {
            currentRight.expandBy(buckets[i].bounds);
            rightSum += buckets[i].count;
            rightBoxes[i - 1] = currentRight;
            rightCounts[i - 1] = rightSum;
        }

        // 3. 评估每一个分割点
        for (int i = 0; i < NUM_BUCKETS - 1; ++i) {
            if (leftCounts[i] == 0 || rightCounts[i] == 0) continue;

            float cost = evaluateSAH(bounds, leftBoxes[i], rightBoxes[i], leftCounts[i], rightCounts[i]);
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestPos = axisMin + axisExtent * (static_cast<float>(i + 1) / NUM_BUCKETS);
            }
        }
    }

    // 如果分割的期望代价还不如不分割（直接渲染全部），则返回 false
    return bestCost < (INTERSECTION_COST * numObjects);
}

osg::ref_ptr<B3DMTile> KDTreeBuilder::divideB3DM(std::vector<NodeProperty>& nodeProps,
    const osg::BoundingBox& bounds,
    osg::ref_ptr<B3DMTile> parent,
    const int x, const int y, const int z,
    const int level) {

    // 1. 创建当前 Tile
    osg::ref_ptr<B3DMTile> tile = new B3DMTile(parent.get());
    tile->level = level;
    tile->x = x; tile->y = y; tile->z = z;

    // 2. 提取当前层级 LOD 物体 (重要：根据预算抽走大物体)
    // 注意：这个方法会修改 nodeProps，移除已被抽走的节点
    TreeBuilder::processB3DMWithMeshDrawcallCommandLimit(nodeProps, bounds, tile);

    // 如果所有物体都被当前瓦片消化了，直接返回
    if (nodeProps.empty()) return tile;

    // 3. 寻找 SAH 最佳分割点
    int bestAxis = -1;
    float bestPos = 0.0f;
    float bestCost = 0.0f;

    bool shouldSplit = (level < _config.getMaxLevel()) &&
        findBestSplit(nodeProps, bounds, bestAxis, bestPos, bestCost);

    if (!shouldSplit) {
        // 如果不值得继续分割，将剩余所有物体放入当前 Tile 的节点中
        if (!tile->node.valid()) tile->node = new osg::Group;
        for (auto& prop : nodeProps) {
            tile->node->asGroup()->addChild(prop.node.get());
        }
        nodeProps.clear();
        return tile;
    }

    // 4. 执行空间切分
    std::vector<NodeProperty> leftProps, rightProps;
    osg::BoundingBox leftBounds = bounds;
    osg::BoundingBox rightBounds = bounds;
    leftBounds._max[bestAxis] = bestPos;
    rightBounds._min[bestAxis] = bestPos;

    for (auto& prop : nodeProps) {
        float centroid = (prop.bb._min[bestAxis] + prop.bb._max[bestAxis]) * 0.5f;
        if (centroid <= bestPos) {
            leftProps.push_back(prop);
        }
        else {
            rightProps.push_back(prop);
        }
    }
    nodeProps.clear(); // 清空原始列表，防止内存冗余

    // 5. 递归构建子树
    // 计算子 Tile 的坐标索引 (针对 KD-Tree 的二叉结构进行逻辑偏移)
    auto createChild = [&](std::vector<NodeProperty>& props, const osg::BoundingBox& b, int offX, int offY, int offZ) {
        if (!props.empty()) {
            osg::ref_ptr<B3DMTile> child = divideB3DM(props, b, tile, x + offX, y + offY, z + offZ, level + 1);
            if (child->node.valid() || !child->children.empty()) {
                tile->children.push_back(child);
            }
        }
        };

    int dx = (bestAxis == 0) ? 1 : 0;
    int dy = (bestAxis == 1) ? 1 : 0;
    int dz = (bestAxis == 2) ? 1 : 0;

    createChild(leftProps, leftBounds, 0, 0, 0);
    createChild(rightProps, rightBounds, dx, dy, dz);

    return tile;
}

void KDTreeBuilder::divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile)
{
    if (!tile.valid() || group.empty()) { return; }

    if (TreeBuilder::processI3DM(group, bounds, tile))
    {
        return;
    }

    // 使用SAH找到最佳分割
    int bestAxis;
    float bestPos;
    float bestCost;

    // 创建临时Group用于SAH计算
    std::vector<NodeProperty> nodeProps;
    nodeProps.reserve(group.size());
    for (const auto& i3dmTile : group) {
        if (i3dmTile->node.valid()) {
            NodeProperty np;
            np.node = i3dmTile->node;
            np.bb= computeBoundingBox(i3dmTile->node);
            nodeProps.push_back(np);
        }
    }

    if (!findBestSplit(nodeProps, bounds, bestAxis, bestPos, bestCost)) {
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
