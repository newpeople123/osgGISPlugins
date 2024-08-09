#ifndef OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H 1
#include "osgdb_gltf/compress/GltfCompressor.h"

class GltfMeshOptCompressor :public GltfCompressor {
private:
    bool _compressmore = true;
	void compressMesh(tinygltf::Mesh& mesh);

    std::vector<unsigned char> encodeVertexBuffer(const tinygltf::Accessor& attributeAccessor, const unsigned int byteStride);
public:
    EXT_meshopt_compression meshOptExtension;
    GltfMeshOptCompressor(tinygltf::Model& model) :GltfCompressor(model, "EXT_meshopt_compression") {}
    void apply() override;
};
#endif // !OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H
