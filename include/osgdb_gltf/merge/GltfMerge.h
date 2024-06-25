#ifndef OSG_GIS_PLUGINS_GLTF_MERGE_H
#define OSG_GIS_PLUGINS_GLTF_MERGE_H 1
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
void mergeBuffers(tinygltf::Model& model);
void mergeMeshes(tinygltf::Model& model);
#endif // !OSG_GIS_PLUGINS_GLTF_MERGE_H
