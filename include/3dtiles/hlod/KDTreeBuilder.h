#ifndef OSG_GIS_PLUGINS_KDTREE_BUILDER_H
#define OSG_GIS_PLUGINS_KDTREE_BUILDER_H
#include "3dtiles/hlod/TreeBuilder.h"
using namespace osgGISPlugins;
namespace osgGISPlugins
{
    /*
    * SAH代价函数的基本形式
    * Cost = Ct + Ci * (P(left) * N(left) + P(right) * N(right))
    * 其中：
    * Ct = 遍历成本（访问节点的开销）
    * Ci = 相交测试成本（与几何体求交的开销）
    * P(child) = SA(child) / SA(parent) // 射线击中子节点的概率
    * N(child) = 子节点中的图元数量
    * SA = Surface Area（表面积）
    */
    class KDTreeBuilder : public TreeBuilder {
    public:
        META_NodeVisitor(osgGISPlugins, KDTreeBuilder)

        KDTreeBuilder() :TreeBuilder() {}

        KDTreeBuilder(BuilderConfig config) :TreeBuilder(config) {}
    private:
        struct SAHBucket {
            osg::BoundingBox bounds;     // 桶的包围盒
            size_t count = 0;            // 桶中的对象数量
        };

        static constexpr int NUM_BUCKETS = 12;  // SAH桶的数量
        static constexpr float TRAVERSAL_COST = 1.0f;  // 遍历成本
        static constexpr float INTERSECTION_COST = 1.5f;  // 相交测试成本

        // 包围盒表面积
        float computeSurfaceArea(const osg::BoundingBox& bounds) const;
        float evaluateSAH(const osg::BoundingBox& bounds,
            const osg::BoundingBox& leftBounds,
            const osg::BoundingBox& rightBounds,
            int leftCount, int rightCount) const;

        // 使用SAH找到最佳分割
        bool findBestSplit(const osg::ref_ptr<osg::Group>& group,
            const osg::BoundingBox& bounds,
            int& bestAxis,
            float& bestPos,
            float& bestCost);

        osg::ref_ptr<B3DMTile> divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent = nullptr, const int x = 0, const int y = 0, const int z = 0, const int level = 0) override;

        void divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile) override;
    };
}
#endif // !OSG_GIS_PLUGINS_KDTREE_BUILDER_H
