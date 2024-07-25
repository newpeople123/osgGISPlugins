#include "osgdb_gltf/compress/GltfMeshOptCompressor.h"
#include <meshoptimizer.h>
void GltfMeshOptCompressor::compressMesh(tinygltf::Mesh& mesh)
{
	for (const auto& primitive : mesh.primitives)
	{
		tinygltf::Value::Object meshoptExtensionObj;
		if (primitive.indices != -1) {
			const tinygltf::Accessor indicesAccessor = _model.accessors[primitive.indices];
			const tinygltf::BufferView indicesBufferView = _model.bufferViews[indicesAccessor.bufferView];
			tinygltf::Buffer indicesBuffer = _model.buffers[indicesBufferView.buffer];
		}
		for (const auto& attribute : primitive.attributes) {
			const auto& accessor = _model.accessors[attribute.second];
			const tinygltf::Accessor attributeAccessor(accessor);
			tinygltf::BufferView& bufferView = _model.bufferViews[attributeAccessor.bufferView];
			tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
			auto getEncodeVertexBuffer = [&accessor](const tinygltf::Accessor& attributeAccessor, const tinygltf::Model& model)->std::vector<unsigned char> {
				const tinygltf::BufferView bufferView = model.bufferViews[attributeAccessor.bufferView];
				const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
				size_t bufferSize = meshopt_encodeVertexBufferBound(attributeAccessor.count, bufferView.byteStride);
				std::vector<unsigned char> vbuf(bufferSize);
				vbuf.resize(meshopt_encodeVertexBuffer(vbuf.data(), vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, bufferView.byteStride));
				return vbuf;
				};
			if (attribute.first == "POSITION") {

				std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model);
				buffer.data.resize(vbuf.size());
				for (unsigned int i = 0; i < vbuf.size(); ++i)
					buffer.data[i] = vbuf[i];

				meshoptExtensionObj = tinygltf::Value::Object();
				meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
				meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value(static_cast<int>(vbuf.size()))));
				meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(bufferView.byteStride))));
				meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value(static_cast<int>(attributeAccessor.count))));
				meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value(static_cast<std::string>("ATTRIBUTES"))));
				bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));

				if (primitive.indices != -1) {
					const tinygltf::Accessor& indicesAccessor = _model.accessors[primitive.indices];
					tinygltf::BufferView& indicesBufferView = _model.bufferViews[indicesAccessor.bufferView];
					tinygltf::Buffer& indicesBuffer = _model.buffers[indicesBufferView.buffer];

					meshoptExtensionObj = tinygltf::Value::Object();
					const int stride = indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 4;
					switch (indicesAccessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)stride)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)stride)));
						break;
					default:
						break;
					}
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

					meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)indicesBufferView.buffer)));
					meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value(static_cast<int>(ibuf.size()))));
					meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value(static_cast<int>(indicesAccessor.count))));
					meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value(static_cast<std::string>("TRIANGLES"))));
					indicesBufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
				}
			}
			else if (attribute.first == "NORMAL" || attribute.first == "TANGENT") {
				std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model);
				buffer.data.resize(vbuf.size());
				for (unsigned int i = 0; i < vbuf.size(); ++i)
					buffer.data[i] = vbuf[i];
				meshoptExtensionObj = tinygltf::Value::Object();
				meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
				meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value(static_cast<int>(vbuf.size()))));
				meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(bufferView.byteStride))));
				meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value(static_cast<int>(attributeAccessor.count))));
				meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value(static_cast<std::string>("ATTRIBUTES"))));
				bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
			}
			else if (attribute.first == "TEXCOORD_0") {
				std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model);
				buffer.data.resize(vbuf.size());
				for (unsigned int i = 0; i < vbuf.size(); ++i)
					buffer.data[i] = vbuf[i];
				meshoptExtensionObj = tinygltf::Value::Object();
				meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
				meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value(static_cast<int>(vbuf.size()))));
				meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(bufferView.byteStride))));
				meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value(static_cast<int>(attributeAccessor.count))));
				meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value(static_cast<std::string>("ATTRIBUTES"))));
				bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
			}
			else if (attribute.first == "_BATCHID") {
				std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model);
				buffer.data.resize(vbuf.size());
				for (unsigned int i = 0; i < vbuf.size(); ++i)
					buffer.data[i] = vbuf[i];
				meshoptExtensionObj = tinygltf::Value::Object();
				meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
				meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value(static_cast<int>(vbuf.size()))));
				meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(bufferView.byteStride))));
				meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value(static_cast<int>(attributeAccessor.count))));
				meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value(static_cast<std::string>("ATTRIBUTES"))));
				bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
			}
		}
	}
}
