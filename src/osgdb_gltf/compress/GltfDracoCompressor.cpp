#include "osgdb_gltf/compress/GltfDracoCompressor.h"
#include "draco/compression/decode.h"

draco::GeometryAttribute::Type GltfDracoCompressor::getTypeFromAttributeName(const std::string& name)
{
	if (name == "POSITION")
	{
		return draco::GeometryAttribute::Type::POSITION;
	}
	if (name == "NORMAL")
	{
		return draco::GeometryAttribute::Type::NORMAL;
	}
	if (name == "TEXCOORD_0")
	{
		return draco::GeometryAttribute::Type::TEX_COORD;
	}
	if (name == "TEXCOORD_1")
	{
		return draco::GeometryAttribute::Type::TEX_COORD;
	}
	if (name == "COLOR_0")
	{
		return draco::GeometryAttribute::Type::COLOR;
	}
	if (name == "JOINTS_0")
	{
		return draco::GeometryAttribute::Type::GENERIC;
	}
	if (name == "WEIGHTS_0")
	{
		return draco::GeometryAttribute::Type::GENERIC;
	}
	if (name == "TANGENT")
	{
		return draco::GeometryAttribute::Type::GENERIC;
	}
	return draco::GeometryAttribute::Type::GENERIC;
}

draco::DataType GltfDracoCompressor::getDataType(const int componentType)
{
	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_BYTE: return draco::DataType::DT_INT8;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return draco::DataType::DT_UINT8;
	case TINYGLTF_COMPONENT_TYPE_SHORT: return draco::DataType::DT_INT16;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return draco::DataType::DT_UINT16;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return draco::DataType::DT_UINT32;
	case TINYGLTF_COMPONENT_TYPE_FLOAT: return draco::DataType::DT_FLOAT32;
	}
	return draco::DataType::DT_INVALID;
}

void GltfDracoCompressor::compressMesh(tinygltf::Mesh& mesh)
{
	draco::Encoder encoder;
	/*
	low level
	options.PositionQuantizationBits = 16.0;
	options.TexCoordQuantizationBits = 14.0;
	options.NormalQuantizationBits = 12.0;
	options.ColorQuantizationBits = 10.0;
	options.GenericQuantizationBits = 18.0;
	*/

	/*
	medium level
	options.PositionQuantizationBits = 14.0;
	options.TexCoordQuantizationBits = 12.0;
	options.NormalQuantizationBits = 10.0;
	options.ColorQuantizationBits = 8.0;
	options.GenericQuantizationBits = 16.0;
	*/

	/*
	high level
	options.PositionQuantizationBits = 12.0;
	options.TexCoordQuantizationBits = 10.0;
	options.NormalQuantizationBits = 8.0;
	options.ColorQuantizationBits = 8.0;
	options.GenericQuantizationBits = 14.0;
	*/

	/*
	vertex lossless compression
	options.PositionQuantizationBits = 0.0;
	*/

	std::unordered_set<int> bufferViewsToRemove;
	setDracoEncoderOptions(encoder);

	for (auto& j : mesh.primitives)
	{
		const auto& primitive = j;
		if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
			continue;
		draco::Mesh dracoMesh;

		if (primitive.indices != -1) {
			tinygltf::Accessor& indicesAccessor = _model.accessors[primitive.indices];
			const tinygltf::BufferView& indicesBv = _model.bufferViews[indicesAccessor.bufferView];
			const tinygltf::Buffer& indicesBuffer = _model.buffers[indicesBv.buffer];

			switch (indicesAccessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_BYTE:
				initDracoMeshFaces<int8_t>(indicesAccessor, dracoMesh);
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				initDracoMeshFaces<uint8_t>(indicesAccessor, dracoMesh);
				break;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
				initDracoMeshFaces<int16_t>(indicesAccessor, dracoMesh);
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				initDracoMeshFaces<uint16_t>(indicesAccessor, dracoMesh);
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				initDracoMeshFaces<uint32_t>(indicesAccessor, dracoMesh);
				break;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				initDracoMeshFaces<float>(indicesAccessor, dracoMesh);
				break;
			case TINYGLTF_COMPONENT_TYPE_DOUBLE:
				initDracoMeshFaces<double>(indicesAccessor, dracoMesh);
				break;
			default:
				initDracoMeshFaces<uint32_t>(indicesAccessor, dracoMesh);
				// Handle unsupported component types or throw an error.
				break;
			}

			bufferViewsToRemove.emplace(indicesAccessor.bufferView);
			indicesAccessor.bufferView = -1;
			indicesAccessor.byteOffset = 0;
		}

		else {
			// 对于gl_triangles和drawArrays的情况
			std::vector<uint32_t> indices;
			const size_t vertexCount = _model.accessors[primitive.attributes.begin()->second].count;

			for (size_t i = 0; i < vertexCount; i += 3) {
				indices.push_back(static_cast<uint32_t>(i));
				indices.push_back(static_cast<uint32_t>(i + 1));
				indices.push_back(static_cast<uint32_t>(i + 2));
			}

			const size_t numFaces = vertexCount / 3;
			dracoMesh.SetNumFaces(numFaces);
			for (size_t i = 0; i < numFaces; i++)
			{
				draco::Mesh::Face face;
				face[0] = indices[(i * 3) + 0];
				face[1] = indices[(i * 3) + 1];
				face[2] = indices[(i * 3) + 2];
				dracoMesh.SetFace(draco::FaceIndex(i), face);
			}
		}

		for (const auto& attribute : primitive.attributes) {
			if (attribute.second != -1) {
				const auto& accessor = _model.accessors[attribute.second];
				tinygltf::Accessor attributeAccessor(accessor);
				int attId = -1;

				switch (accessor.componentType) {
				case TINYGLTF_COMPONENT_TYPE_BYTE:
					attId = initPointAttribute<int8_t>(dracoMesh, attribute.first, attributeAccessor);
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					attId = initPointAttribute<uint8_t>(dracoMesh, attribute.first, attributeAccessor);
					break;
				case TINYGLTF_COMPONENT_TYPE_SHORT:
					attId = initPointAttribute<int16_t>(dracoMesh, attribute.first, attributeAccessor);
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					attId = initPointAttribute<uint16_t>(dracoMesh, attribute.first, attributeAccessor);
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					attId = initPointAttribute<uint32_t>(dracoMesh, attribute.first, attributeAccessor);
					break;
				case TINYGLTF_COMPONENT_TYPE_FLOAT:
					attId = initPointAttribute<float>(dracoMesh, attribute.first, attributeAccessor);
					break;
				case TINYGLTF_COMPONENT_TYPE_DOUBLE:
					attId = initPointAttribute<double>(dracoMesh, attribute.first, attributeAccessor);
					break;
				default:
					// Handle unsupported component types or throw an error.
					break;
				}

				if (attId != -1) {
					bufferViewsToRemove.emplace(accessor.bufferView);
					attributeAccessor.bufferView = -1;
					attributeAccessor.byteOffset = 0;
					_model.accessors[attribute.second] = attributeAccessor;
					extension.Set(attribute.first, static_cast<int>(dracoMesh.attribute(attId)->unique_id()));
				}
			}
		}

		if (!primitive.targets.empty()) {
			encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
		}

		draco::EncoderBuffer encoderBuffer;
		const draco::Status status = encoder.EncodeMeshToBuffer(dracoMesh, &encoderBuffer);
		if (!status.ok()) {
			std::cerr << "draco:Failed to encode the mesh: " << status.error_msg() << '\n';
		}

		
		//draco::Decoder decoder;
		//draco::DecoderBuffer decoderBuffer;
		//decoderBuffer.Init(encoderBuffer.data(), encoderBuffer.size());
		//auto decodeResult = decoder.DecodeMeshFromBuffer(&decoderBuffer);
		//if (!decodeResult.ok()) {
		//	std::cerr << "draco:Draco failed to decode mesh" << '\n';
		//	return;
		//}
		

		if (primitive.indices != -1) {
			assert(_model.accessors.size() > primitive.indices);
			tinygltf::Accessor encodedIndexAccessor(_model.accessors[primitive.indices]);
			encodedIndexAccessor.count = encoder.num_encoded_faces() * 3;
			_model.accessors[primitive.indices] = encodedIndexAccessor;
		}

		for (const auto& dracoAttribute : extension.GetValueObject()) {
			auto accessorAttr = primitive.attributes.find(dracoAttribute.first);
			if (accessorAttr == primitive.attributes.end())
				continue;
			const int accessorId = accessorAttr->second;
			assert(_model.accessors.size() > accessorId);

			tinygltf::Accessor encodedAccessor(_model.accessors[accessorId]);
			encodedAccessor.count = encoder.num_encoded_points();
			_model.accessors[accessorId] = encodedAccessor;
		}

		_model.buffers.emplace_back();
		tinygltf::Buffer& gltfBuffer = _model.buffers.back();
		int bufferId = _model.buffers.size() - 1;
		gltfBuffer.data.resize(encoderBuffer.size());
		const char* dracoBuffer = encoderBuffer.data();
		for (unsigned int i = 0; i < encoderBuffer.size(); ++i)
			gltfBuffer.data[i] = *dracoBuffer++;
		_model.bufferViews.emplace_back();
		tinygltf::BufferView& bv = _model.bufferViews.back();
		int id = _model.bufferViews.size() - 1;
		bv.buffer = bufferId;
		bv.byteLength = encoderBuffer.size();
		bv.byteOffset = 0;

		tinygltf::Value::Object dracoExtensionObj;
		dracoExtensionObj["attributes"] = extension.GetValue();
		dracoExtensionObj["bufferView"] = tinygltf::Value(id);
		tinygltf::Primitive resultPrimitive(primitive);
		resultPrimitive.extensions.insert(std::make_pair(extension.name, tinygltf::Value(dracoExtensionObj)));
		j = resultPrimitive;
	}

	removeBufferViews(bufferViewsToRemove);
}

void GltfDracoCompressor::removeBufferViews(const std::unordered_set<int>& bufferViewsToRemove)
{
	std::vector<int> bufferViewsToRemoveVector(bufferViewsToRemove.begin(), bufferViewsToRemove.end());
	for (int i = 0; i < bufferViewsToRemoveVector.size(); ++i) {
		int bufferViewId = bufferViewsToRemoveVector.at(i);
		tinygltf::BufferView bufferView = _model.bufferViews[bufferViewId];
		const int bufferId = bufferView.buffer;
		_model.buffers.erase(_model.buffers.begin() + bufferId);
		for (auto& bv : _model.bufferViews) {
			if (bv.buffer > bufferId) {
				bv.buffer -= 1;
			}
		}

		_model.bufferViews.erase(_model.bufferViews.begin() + bufferViewId);
		for (auto& accessor : _model.accessors) {
			if (accessor.bufferView > bufferViewId) {
				accessor.bufferView -= 1;
			}
		}
		for (auto& image : _model.images) {
			if (image.bufferView > bufferViewId) {
				image.bufferView -= 1;
			}
		}
		for (auto& mesh : _model.meshes) {
			for (auto& primitive : mesh.primitives) {
				auto dracoExtension = primitive.extensions.find(extension.name);
				if (dracoExtension != primitive.extensions.end()) {
					auto dracoBufferViewId = dracoExtension->second.Get("bufferView").GetNumberAsInt();

					tinygltf::Value::Object obj;
					obj.insert(std::make_pair("bufferView", tinygltf::Value(dracoBufferViewId - 1)));
					tinygltf::Value attributesValue = dracoExtension->second.Get("attributes");
					tinygltf::Value::Object attributesObject = attributesValue.Get<tinygltf::Value::Object>();
					obj.insert(std::make_pair("attributes", attributesObject));
					dracoExtension->second = tinygltf::Value(obj);
				}
			}
		}

		for (int j = i; j < bufferViewsToRemoveVector.size(); ++j) {
			int id = bufferViewsToRemoveVector.at(j);
			if (id > bufferViewId) {
				bufferViewsToRemoveVector.at(j) -= 1;
			}
		}
	}
}

void GltfDracoCompressor::setDracoEncoderOptions(draco::Encoder& encoder)
{
	encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, _compressionOptions.PositionQuantizationBits);
	encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, _compressionOptions.TexCoordQuantizationBits);
	encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, _compressionOptions.NormalQuantizationBits);
	encoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, _compressionOptions.ColorQuantizationBits);
	encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, _compressionOptions.GenericQuantizationBits);
	encoder.SetSpeedOptions(_compressionOptions.EncodeSpeed, _compressionOptions.DecodeSpeed);
	encoder.SetTrackEncodedProperties(true);
}

