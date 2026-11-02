#ifndef OSG_GIS_PLUGINS_SIMPLIFIER_H
#define OSG_GIS_PLUGINS_SIMPLIFIER_H
#include <osgUtil/Simplifier>
#include <vector>
#include <meshoptimizer.h>
namespace osgGISPlugins {
    // 预算驱动简化（手册 7.2）
    //
    // 与旧实现的两点关键差异：
    //  * 支持一个 Geometry 携带多个 primitiveSet：逐个简化后按所有 pset 的
    //    顶点并集做一次统一重映射。旧实现在 psetCount>1 时直接 return，
    //    导致合并后的几何完全不简化，硬预算永远达不到（AUDIT A-26）。
    //  * 绝不清空几何：目标索引数小于一个三角形时跳过该 pset，
    //    而不是把 vertex array 置空（AUDIT A-27 的空代理根因）。
    class Simplifier :public osgUtil::Simplifier
    {
    private:
        bool _aggressive = false, _lockBorders = false;
        float _targetError = 1e-2f, _targetErrorAggressive = 1e-1f;
    public:
        float lodError = 0.0;
        Simplifier(const double sampleRatio, const bool aggressive = false,
            const bool lockBorders = false, const float targetError = 1e-2f) :osgUtil::Simplifier(sampleRatio),
            _aggressive(aggressive), _lockBorders(lockBorders) {
            if (_aggressive)
                _targetErrorAggressive = targetError;
            else
                _targetError = targetError;
        }

        META_NodeVisitor(osgGISPlugins, Simplifier)

        // 只删除精确退化三角形：索引重复、索引越界、顶点非有限、面积精确为 0。
        // 细长但面积非零的三角形合法，必须保留。indices.size()%3 的尾数一并裁掉。
        // 返回删除的三角形数；不改变顶点数组（由调用方做统一重映射）。
        static std::size_t removeExactDegenerateTriangles(const std::vector<float>& positions,
                                                          std::size_t vertexCount,
                                                          std::vector<unsigned int>& indices);
        // 被引用顶点按**旧下标升序**分配新下标（0..n-1），未引用顶点保持 0xFFFFFFFF。
        // 必须与 remapArray 的打包顺序一致：按 pset 首次引用顺序赋号会让索引
        // 与顶点数组错位，输出三角面连接错误顶点（表现为退化/混乱三角网）。
        static std::vector<unsigned int> buildVertexRemap(
            const std::vector<std::vector<unsigned int>>& psets, std::size_t vertexCount);
        std::size_t removedDegenerateTriangles() const { return _removedDegenerateTris; }

        void apply(osg::Geometry& geom) override
        {
            simplifyMesh(geom);
        }
    protected:
        void simplifyMesh(osg::Geometry& geometry);

        // 把索引缓冲简化到 sampleRatio 目标；out 指向 geometry 原始顶点下标。
        // 返回 false 表示该 pset 保持原样（目标过小或不是三角形列表）。
        bool simplifyIndices(const std::vector<unsigned int>& src,
                             const std::vector<float>& positions,
                             std::size_t vertexCount,
                             std::vector<unsigned int>& out);
    private:
        std::size_t _removedDegenerateTris = 0;
    };
}
#endif // !OSG_GIS_PLUGINS_SIMPLIFIER_H
