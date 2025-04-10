#include "osgdb_gltf/compress/GltfMeshOptCompressor.h"
#include <meshoptimizer.h>
using namespace osgGISPlugins;
void GltfMeshOptCompressor::compressMesh(const tinygltf::Mesh& mesh)
{
	for (const tinygltf::Primitive& primitive : mesh.primitives)
	{
		for (const auto& attribute : primitive.attributes) {
			if (attribute.second == -1) return;
			const tinygltf::Accessor& accessor = _model.accessors[attribute.second];
			const tinygltf::Accessor attributeAccessor(accessor);
			tinygltf::BufferView& bufferView = _model.bufferViews[attributeAccessor.bufferView];
			tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];

			unsigned int byteStride = bufferView.byteStride;
			const int num = attribute.first == "TANGENT" ? 4 : attribute.first == "POSITION" || attribute.first == "NORMAL" ? 3 : attribute.first == "TEXCOORD_0" ? 2 : 1;
			if (byteStride == 0) {
				switch (accessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_BYTE:
					byteStride = sizeof(int8_t) * num;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					byteStride = sizeof(uint8_t) * num;
					break;
				case TINYGLTF_COMPONENT_TYPE_SHORT:
					byteStride = sizeof(int16_t) * num;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					byteStride = sizeof(uint16_t) * num;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					byteStride = sizeof(uint32_t) * num;
					break;
				default:
					byteStride = sizeof(float) * num;
					break;
				}
			}
			//POSITION、NORMAL、TANGENT、TEXCOORD_0、_BATCHID
			std::vector<unsigned char> vbuf = encodeVertexBuffer(attributeAccessor, byteStride);

			buffer.data.resize(vbuf.size());
			for (unsigned int i = 0; i < vbuf.size(); ++i)
				buffer.data[i] = vbuf[i];

			meshOptExtension.setBuffer(bufferView.buffer);
			meshOptExtension.setByteLength(vbuf.size());
			meshOptExtension.setByteStride(byteStride);
			meshOptExtension.setCount(attributeAccessor.count);
			if (attribute.first == "NORMAL" && _compressmore && bufferView.target == TINYGLTF_TARGET_ARRAY_BUFFER)
				meshOptExtension.Set("filter", "OCTAHEDRAL");
			meshOptExtension.setMode("ATTRIBUTES");
			bufferView.extensions[meshOptExtension.name] = meshOptExtension.GetValue();

			if (attribute.first == "POSITION") {

				if (primitive.indices != -1) {
					const tinygltf::Accessor& indicesAccessor = _model.accessors[primitive.indices];
					tinygltf::BufferView& indicesBufferView = _model.bufferViews[indicesAccessor.bufferView];
					tinygltf::Buffer& indicesBuffer = _model.buffers[indicesBufferView.buffer];
					const int stride = indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 4;
					std::vector<unsigned char> ibuf(meshopt_encodeIndexBufferBound(indicesAccessor.count, indicesAccessor.count));

					if (stride == 2) {
						ibuf.resize(meshopt_encodeIndexBuffer(ibuf.data(), ibuf.size(), reinterpret_cast<const uint16_t*>(indicesBuffer.data.data()), indicesAccessor.count));
					}
					else
					{
						ibuf.resize(meshopt_encodeIndexBuffer(ibuf.data(), ibuf.size(), reinterpret_cast<const uint32_t*>(indicesBuffer.data.data()), indicesAccessor.count));
					}
					indicesBuffer.data.resize(ibuf.size());
					for (unsigned int i = 0; i < ibuf.size(); ++i)
						indicesBuffer.data[i] = ibuf[i];

					meshOptExtension.setBuffer(indicesBufferView.buffer);
					meshOptExtension.setByteLength(ibuf.size());
					meshOptExtension.setByteStride(stride);
					meshOptExtension.setCount(indicesAccessor.count);
					meshOptExtension.setMode("TRIANGLES");
					indicesBufferView.extensions[meshOptExtension.name] = meshOptExtension.GetValue();
				}
			}
		}
	}
}

std::vector<unsigned char> GltfMeshOptCompressor::encodeVertexBuffer(const tinygltf::Accessor& attributeAccessor, const unsigned int byteStride) const {
	const tinygltf::BufferView bufferView = _model.bufferViews[attributeAccessor.bufferView];
	const tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
	const size_t bufferSize = meshopt_encodeVertexBufferBound(attributeAccessor.count, byteStride);
	std::vector<unsigned char> vbuf(bufferSize);
	vbuf.resize(meshopt_encodeVertexBuffer(vbuf.data(), vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, byteStride));
	return vbuf;
}

void GltfMeshOptCompressor::apply()
{
	for (tinygltf::Mesh& mesh : _model.meshes) {
		compressMesh(mesh);
	}
}
