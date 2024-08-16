#ifndef OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H
#define OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H 1
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <osg/Array>
namespace osgGISPlugins {
    class GltfOptimizer
    {
    protected:
        tinygltf::Model& _model;

        template<typename T>
        std::vector<T> getBufferData(const tinygltf::Accessor& accessor);

        size_t calculateNumComponents(const int type);

        void restoreBuffer(tinygltf::Buffer& buffer, tinygltf::BufferView& bufferView, osg::ref_ptr<osg::Array> newBufferData);
    public:
        GltfOptimizer(tinygltf::Model& model) :_model(model) {}
        virtual void apply() = 0;
    };

    template<typename T>
    inline std::vector<T> GltfOptimizer::getBufferData(const tinygltf::Accessor& accessor)
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
}
#endif // !OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H
