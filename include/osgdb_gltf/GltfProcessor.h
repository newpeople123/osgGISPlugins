#ifndef OSG_GIS_PLUGINS_GLTF_PROCESSOR_H
#define OSG_GIS_PLUGINS_GLTF_PROCESSOR_H 1
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <osg/Array>
namespace osgGISPlugins {
    class GltfProcessor
    {
    protected:
        tinygltf::Model& _model;

        template<typename T>
        std::vector<T> getBufferData(const tinygltf::Buffer& buffer, size_t byteOffset, size_t numComponents, size_t count);

        template<typename T>
        std::vector<T> getBufferData(const tinygltf::Accessor& accessor);

        static size_t calculateNumComponents(int type);

        static void restoreBuffer(tinygltf::Buffer& buffer, tinygltf::BufferView& bufferView, const osg::ref_ptr<osg::Array>& newBufferData);
    public:
        GltfProcessor(tinygltf::Model& model) :_model(model) {}
        virtual void apply() = 0;
    };

    template<typename T>
    std::vector<T> GltfProcessor::getBufferData(const tinygltf::Buffer& buffer, const size_t byteOffset,const size_t numComponents, const size_t count)
    {
        std::vector<T> values;
        const void* data_ptr = buffer.data.data() + byteOffset;
        values.assign(static_cast<const T*>(data_ptr),
            static_cast<const T*>(data_ptr) + count * numComponents);
        return values;
    }

    template<typename T>
    std::vector<T> GltfProcessor::getBufferData(const tinygltf::Accessor& accessor)
    {
        const size_t numComponents = calculateNumComponents(accessor.type);
        const tinygltf::BufferView& bufferView = _model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& gltfBuffer = _model.buffers[bufferView.buffer];
        return getBufferData<T>(gltfBuffer, bufferView.byteOffset, accessor.count, numComponents);
    }
}
#endif // !OSG_GIS_PLUGINS_GLTF_PROCESSOR_H
