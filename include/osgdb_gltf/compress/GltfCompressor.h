#ifndef OSG_GIS_PLUGINS_GLTF_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_COMPRESSOR_H 1
#include "osgdb_gltf/Extensions.h"
#include "osgdb_gltf/GltfProcessor.h"
namespace osgGISPlugins {
    class GltfCompressor :public GltfProcessor
    {
    public:
        struct CompressionOptions {
            int PositionQuantizationBits = 14;
            int TexCoordQuantizationBits = 12;
            int NormalQuantizationBits = 10;
            int ColorQuantizationBits = 8;
        };
        GltfCompressor() = default;
        GltfCompressor(tinygltf::Model& model, const std::string& extensionName) :GltfProcessor(model) {
            model.extensionsRequired.push_back(extensionName);
            model.extensionsUsed.push_back(extensionName);
        }
        virtual ~GltfCompressor() = default;
    };
}
#endif // !OSG_GIS_PLUGINS_GLTF_COMPRESSOR_H
