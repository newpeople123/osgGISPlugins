#ifndef OSG_GIS_PLUGINS_GLTF_MERGE_H
#define OSG_GIS_PLUGINS_GLTF_MERGE_H 1
#include "osgdb_gltf/GltfProcessor.h"
#include "utils/Utils.h"
#include <osg/Matrixd>
#include <unordered_map>
namespace osgGISPlugins {

    class GltfMerger :public GltfProcessor
    {
    public:
        GltfMerger(tinygltf::Model& model) :GltfProcessor(model), _bMergeMaterials(true), _bMergeMeshes(true) {}
        GltfMerger(tinygltf::Model& model, const bool bMergeMaterials, const bool bMergeMeshes) :GltfProcessor(model), _bMergeMaterials(bMergeMaterials), _bMergeMeshes(bMergeMeshes) {}
        void apply() override;
        //减少缓存切换次数
        void mergeBuffers();
    private:
        bool _bMergeMaterials = true, _bMergeMeshes = true;
        void collectMeshNodes(size_t index, std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual>& matrixPrimitiveMap, osg::Matrixd& matrix);
        void reindexBufferAndAccessor(const tinygltf::Accessor& accessor, tinygltf::BufferView& bufferView, tinygltf::Buffer& buffer, tinygltf::Accessor& newAccessor, unsigned int sum = 0, bool isIndices = false);
        //合并mesh和primitive但是会破坏node树的结构
        void mergeMeshes();
        //合并后能减少draw call次数，但是纹理坐标会改变，而且纹理占用的GPU内存量会上涨(合并后导致增加了纹理绑定次数)，需要权衡
        //如果GPU内存即显存充足推荐开启该功能
        void mergeMaterials();

        std::vector<tinygltf::Mesh> mergePrimitives(
            const std::pair<int,std::vector<tinygltf::Primitive>>& materialWithPrimitves,
            std::vector<tinygltf::Accessor>& newAccessors,
            std::vector<tinygltf::BufferView>& newBufferViews,
            std::vector<tinygltf::Buffer>& newBuffers
            );

        template<typename TNew, typename TOld, typename TIndexArray>
        void mergeIndices(tinygltf::Accessor& newAccessor, tinygltf::Buffer& newBuffer, const tinygltf::Accessor& oldIndiceAccessor, osg::ref_ptr<TIndexArray>& indices, unsigned int positionCount);

        void mergeIndice(
            tinygltf::Accessor& newIndiceAccessor,
            tinygltf::BufferView& newIndiceBV,
            tinygltf::Buffer& newIndiceBuffer,
            const tinygltf::Accessor oldIndiceAccessor,
            const unsigned int positionCount
        );

        void mergeAttribute(
            tinygltf::Accessor& newAttributeAccessor, 
            tinygltf::BufferView& newAttributeBV, 
            tinygltf::Buffer& newAttributeBuffer, 
            const tinygltf::Accessor& oldAttributeAccessor
        );

        static osg::Matrixd convertGltfNodeToOsgMatrix(const tinygltf::Node& node);
        static void decomposeMatrix(const osg::Matrixd& matrix, tinygltf::Node& node);
    };

    template<typename TNew, typename TOld, typename TIndexArray>
    inline void GltfMerger::mergeIndices(tinygltf::Accessor& newAccessor, tinygltf::Buffer& newBuffer, const tinygltf::Accessor& oldIndiceAccessor, osg::ref_ptr<TIndexArray>& indices, unsigned int positionCount)
    {
        // 获取新旧索引
        auto newIndices = getBufferData<TNew>(newBuffer, 0, newAccessor.count, calculateNumComponents(newAccessor.type));
        auto oldIndices = getBufferData<TOld>(oldIndiceAccessor);

        // 预分配内存，提升性能
        indices->reserve(newIndices.size() + oldIndices.size());

        // 添加新索引
        for (auto index : newIndices) {
            indices->push_back(static_cast<typename TIndexArray::value_type>(index));
        }

        // 添加旧索引并增加偏移
        for (auto index : oldIndices) {
            indices->push_back(static_cast<typename TIndexArray::value_type>(positionCount + index));
        }
    }
}
#endif // !OSG_GIS_PLUGINS_GLTF_MERGE_H
