#ifndef OSG_GIS_PLUGINS_GLTF_MERGE_H
#define OSG_GIS_PLUGINS_GLTF_MERGE_H 1
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <osg/Matrixd>
#include <unordered_map>
const double TOLERANCE = 0.001;

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
            if (osg::absolute((*ptr1++) - (*ptr2++)) > TOLERANCE) return false;
        }
        return true;
    }
};

class GltfMerger
{
private:
    tinygltf::Model& _model;
public:
    GltfMerger(tinygltf::Model& model) :_model(model) {}
    void mergeMeshes();
    void mergeBuffers();
    static osg::Matrixd convertGltfNodeToOsgMatrix(const tinygltf::Node& node);
    static void decomposeMatrix(const osg::Matrixd& matrix, tinygltf::Node& node);
private:
    void findMeshNodes(size_t index, std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual>& matrixPrimitiveMap, osg::Matrixd& matrix);
    void reindexBufferAndAccessor(const tinygltf::Accessor& accessor, tinygltf::BufferView& bufferView, tinygltf::Buffer& buffer, tinygltf::Accessor& newAccessor, unsigned int sum = 0, bool isIndices = false);
};
#endif // !OSG_GIS_PLUGINS_GLTF_MERGE_H
