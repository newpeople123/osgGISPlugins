#ifndef OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H 1
#include "osgdb_gltf/compress/GltfCompressor.h"
class GltfMeshOptCompressor :public GltfCompressor {
private:
    void quantizeMesh(tinygltf::Mesh& mesh);
public:
    KHR_mesh_quantization meshQuanExtension;
    EXT_meshopt_compression meshOptExtension;
    GltfMeshOptCompressor(tinygltf::Model& model) :GltfCompressor(model)
    {
        model.extensionsRequired.push_back(meshQuanExtension.name);
        //model.extensionsRequired.push_back(meshOptExtension.name);
        model.extensionsUsed.push_back(meshQuanExtension.name);
        //model.extensionsUsed.push_back(meshOptExtension.name);

    }
};
#endif // !OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H
