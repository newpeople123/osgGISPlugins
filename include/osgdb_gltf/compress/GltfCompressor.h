#ifndef OSG_GIS_PLUGINS_GLTF_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_COMPRESSOR_H 1
#include "osgdb_gltf/Extensions.h"
class GltfCompressor
{
protected:
    tinygltf::Model& _model;

    template<typename T>
    std::vector<T> getBufferData(const tinygltf::Accessor& accessor);

    size_t calculateNumComponents(const int type);
public:
    GltfCompressor() = default;
    GltfCompressor(tinygltf::Model& model) :_model(model) {}
    virtual ~GltfCompressor() = default;
};

template<typename T>
inline std::vector<T> GltfCompressor::getBufferData(const tinygltf::Accessor& accessor)
{
    const auto numComponents = calculateNumComponents(accessor.type);
    const auto& bufferView = _model.bufferViews[accessor.bufferView];
    const auto& gltfBuffer = _model.buffers[bufferView.buffer];
    std::vector<T> values;
    const void* data_ptr = gltfBuffer.data.data() + bufferView.byteOffset;
    values.assign(static_cast<const T*>(data_ptr),
        static_cast<const T*>(data_ptr) + accessor.count * numComponents);
    return values;
}
#endif // !OSG_GIS_PLUGINS_GLTF_COMPRESSOR_H
