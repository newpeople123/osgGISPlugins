#include "osgdb_gltf/compress/GltfCompressor.h"
size_t GltfCompressor::calculateNumComponents(const int type)
{
	switch (type) {
	case TINYGLTF_TYPE_SCALAR:
		return 1;
	case TINYGLTF_TYPE_VEC2:
		return 2;
	case TINYGLTF_TYPE_VEC3:
		return 3;
	case TINYGLTF_TYPE_VEC4:
		return 4;
		// Add cases for other data types if needed.
	default:
		return 0; // Return 0 for unknown types.
	}
}
