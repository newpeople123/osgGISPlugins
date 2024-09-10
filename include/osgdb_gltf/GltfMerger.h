#ifndef OSG_GIS_PLUGINS_GLTF_MERGE_H
#define OSG_GIS_PLUGINS_GLTF_MERGE_H 1
#include "osgdb_gltf/GltfProcessor.h"
#include <osg/Matrixd>
#include <unordered_map>
namespace osgGISPlugins {
    const double EPSILON = 0.001;

    struct MatrixHash {
        std::size_t operator()(const osg::Matrixd& matrix) const {
            std::size_t seed = 0;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    seed ^= std::hash<double>()(matrix(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                }
            }
            return seed;
        }
    };

    struct MatrixEqual {
        bool operator()(const osg::Matrixd& lhs, const osg::Matrixd& rhs) const {
            const double* ptr1 = lhs.ptr();
            const double* ptr2 = rhs.ptr();

            for (size_t i = 0; i < 16; ++i) {
                if (osg::absolute((*ptr1++) - (*ptr2++)) > EPSILON) return false;
            }
            return true;
        }
    };

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
        static osg::Matrixd convertGltfNodeToOsgMatrix(const tinygltf::Node& node);
        static void decomposeMatrix(const osg::Matrixd& matrix, tinygltf::Node& node);
    };
}
#endif // !OSG_GIS_PLUGINS_GLTF_MERGE_H
