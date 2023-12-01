#pragma once
#ifndef OSGDB_GLTF_UTILS
#define OSGDB_GLTF_UTILS 1

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <draco/compression/encode.h>
#include <draco/compression/expert_encode.h>
#include <draco/compression/mesh/mesh_encoder.h>
#include <draco/compression/mesh/mesh_edgebreaker_encoder.h>
#include <draco/compression/mesh/mesh_sequential_encoder.h>
#include <unordered_set>
#include <osg/Material>
#include <utils/GltfPbrMetallicRoughnessMaterial.h>
#include <utils/GltfPbrSpecularGlossinessMaterial.h>
#include <draco/compression/decode.h>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include <osgDB/ReadFile>
#include <osgDB/ConvertUTF>
#include <osgDB/FileNameUtils>
#include <ktx/ktx.h>
#include <meshoptimizer.h>
#include <osg/MatrixTransform>
//#include <utils/TextureAtlas.h>
class GeometryNodeVisitor :public osg::NodeVisitor {
public:
	GeometryNodeVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {

	};
	void apply(osg::Drawable& drawable) {
		osg::MatrixList matrixList = drawable.getWorldMatrices();
		osg::Matrixd mat;
		for (const osg::Matrixd& matrix : matrixList) {
			mat = mat * matrix;
		}
		if (mat != osg::Matrixd::identity()) {
			osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(drawable.asGeometry()->getVertexArray());
			for (unsigned int i = 0; i < positions->size(); ++i) {
				positions->at(i) = positions->at(i) * mat;
			}
		}
	}
	void apply(osg::Group& group)
	{
		traverse(group);
	}

};
class TransformNodeVisitor :public osg::NodeVisitor {
public:
	TransformNodeVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {};
	void apply(osg::Group& group)
	{
		traverse(group);
	}
	void apply(osg::Transform& mtransform) {
		//TestNodeVisitor tnv(mtransform.asMatrixTransform()->getMatrix());
		//mtransform.accept(tnv);
		mtransform.asMatrixTransform()->setMatrix(osg::Matrixd::identity());
		apply(static_cast<osg::Group&>(mtransform));
	};
};
struct Stringify
{
	operator std::string() const
	{
		std::string result;
		result = buf.str();
		return result;
	}

	template<typename T>
	Stringify& operator << (const T& val) { buf << val; return (*this); }

	Stringify& operator << (const Stringify& val) { buf << (std::string)val; return (*this); }

protected:
	std::stringstream buf;
};
unsigned hashString(const std::string input)
{
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	unsigned int len = input.length();
	const char* data = input.c_str();
	unsigned int h = m ^ len; // using "m" as the seed.

	while (len >= 4)
	{
		unsigned int k = *(unsigned int*)data;
		k *= m;
		k ^= k >> r;
		k *= m;
		h *= m;
		h ^= k;
		data += 4;
		len -= 4;
	}

	switch (len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
		h *= m;
	};

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}
template<> inline
Stringify& Stringify::operator << <bool>(const bool& val) { buf << (val ? "true" : "false"); return (*this); }

template<> inline
Stringify& Stringify::operator << <osg::Vec3f>(const osg::Vec3f& val) {
	buf << val.x() << " " << val.y() << " " << val.z(); return (*this);
}

template<> inline
Stringify& Stringify::operator << <osg::Vec3d>(const osg::Vec3d& val) {
	buf << val.x() << " " << val.y() << " " << val.z(); return (*this);
}

template<> inline
Stringify& Stringify::operator << <osg::Vec4f>(const osg::Vec4f& val) {
	buf << val.r() << " " << val.g() << " " << val.b() << " " << val.a(); return (*this);
}


enum CompressionType
{
	NONE,
	DRACO,
	MESHOPT
};
enum TextureType
{
	PNG,
	JPG,
	WEBP,
	KTX,
	KTX2
};
class GltfUtils {
private:
	tinygltf::Model& _model;
	std::vector<osg::ref_ptr<osg::Texture>> _textures;
public:
	struct VertexCompressionOptions {
		int PositionQuantizationBits = 14;
		int TexCoordQuantizationBits = 12;
		int NormalQuantizationBits = 10;
		int ColorQuantizationBits = 8;
		int GenericQuantizationBits = 16;
		int Speed = 0;
	};
	struct DracoCompressionOptions :VertexCompressionOptions {
		//Default value is medium level
		int PositionQuantizationBits = 14;
		int TexCoordQuantizationBits = 12;
		int NormalQuantizationBits = 10;
		int ColorQuantizationBits = 8;
		int GenericQuantizationBits = 16;
		int Speed = 0;
	};
	struct MeshoptCompressionOptions :VertexCompressionOptions {

	};
private:
	draco::GeometryAttribute::Type GetTypeFromAttributeName(const std::string& name) {
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
	draco::DataType GetDataType(const tinygltf::Accessor& accessor)
	{
		switch (accessor.componentType)
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
	size_t CalculateNumComponents(const tinygltf::Accessor& accessor) {
		switch (accessor.type) {
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
	template<typename T>
	int InitializePointAttribute(draco::Mesh& dracoMesh, const std::string& attributeName, tinygltf::Accessor& accessor) {
		auto numComponents = CalculateNumComponents(accessor);
		auto stride = sizeof(T) * numComponents;

		auto buffer1 = std::make_unique<draco::DataBuffer>();
		draco::GeometryAttribute atrribute;
		atrribute.Init(GetTypeFromAttributeName(attributeName), &*buffer1, numComponents, GetDataType(accessor), accessor.normalized, stride, 0);
		dracoMesh.set_num_points(static_cast<unsigned int>(accessor.count));
		int attId = dracoMesh.AddAttribute(atrribute, true, dracoMesh.num_points());
		auto attrActual = dracoMesh.attribute(attId);

		auto& bufferView = _model.bufferViews[accessor.bufferView];
		auto& buffer = _model.buffers[bufferView.buffer];
		std::vector<T> values;

		const void* data_ptr = buffer.data.data() + bufferView.byteOffset;


		switch (accessor.componentType) {
		case TINYGLTF_COMPONENT_TYPE_BYTE:
			values.assign(reinterpret_cast<const int8_t*>(data_ptr),
				reinterpret_cast<const int8_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			values.assign(reinterpret_cast<const uint8_t*>(data_ptr),
				reinterpret_cast<const uint8_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:
			values.assign(reinterpret_cast<const int16_t*>(data_ptr),
				reinterpret_cast<const int16_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			values.assign(reinterpret_cast<const uint16_t*>(data_ptr),
				reinterpret_cast<const uint16_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			values.assign(reinterpret_cast<const uint32_t*>(data_ptr),
				reinterpret_cast<const uint32_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			values.assign(reinterpret_cast<const float*>(data_ptr),
				reinterpret_cast<const float*>(data_ptr) + accessor.count * numComponents);
			break;
		default:
			std::cerr << "Unknown component type." << std::endl;
			break;
		}
		if (accessor.count == 25)
			std::cout << std::endl;
		for (draco::PointIndex i(0); i < static_cast<uint32_t>(accessor.count); ++i)
		{
			attrActual->SetAttributeValue(attrActual->mapped_index(i), &values[i.value() * numComponents]);
		}
		if (dracoMesh.num_points() == 0)
		{
			dracoMesh.set_num_points(static_cast<unsigned int>(accessor.count));
		}
		else if (dracoMesh.num_points() != accessor.count)
		{
			std::cerr << "Inconsistent points count." << std::endl;
		}

		return attId;
	}
	void CompressMeshByDraco(tinygltf::Mesh& mesh, std::unordered_set<int>& bufferViewsToRemove, VertexCompressionOptions vco) {
		draco::Encoder encoder;
		//low level
		//options.PositionQuantizationBits = 16.0;
		//options.TexCoordQuantizationBits = 14.0;
		//options.NormalQuantizationBits = 12.0;
		//options.ColorQuantizationBits = 10.0;
		//options.GenericQuantizationBits = 18.0;
		//medium level
		//options.PositionQuantizationBits = 14.0;
		//options.TexCoordQuantizationBits = 12.0;
		//options.NormalQuantizationBits = 10.0;
		//options.ColorQuantizationBits = 8.0;
		//options.GenericQuantizationBits = 16.0;
		//high level
		//options.PositionQuantizationBits = 12.0;
		//options.TexCoordQuantizationBits = 10.0;
		//options.NormalQuantizationBits = 8.0;
		//options.ColorQuantizationBits = 8.0;
		//options.GenericQuantizationBits = 14.0;
		//vertex lossless compression
		//options.PositionQuantizationBits = 0.0;
		setDracoEncoderOptions(encoder, vco);

		for (int j = 0; j < mesh.primitives.size(); j++) {
			const auto& primitive = mesh.primitives.at(j);
			tinygltf::Value::Object dracoExtensionObj;

			tinygltf::Value::Object dracoExtensionAttrObj;

			draco::Mesh dracoMesh;
			if (primitive.indices != -1) {
				tinygltf::Accessor& indicesAccessor = _model.accessors[primitive.indices];
				tinygltf::BufferView& indices_bv = _model.bufferViews[indicesAccessor.bufferView];
				tinygltf::Buffer& indices_buffer = _model.buffers[indices_bv.buffer];

				if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) {
					auto indices_ptr = reinterpret_cast<const int8_t*>(indices_buffer.data.data() + indices_bv.byteOffset);
					std::vector<int8_t> indices(indices_ptr, indices_ptr + indicesAccessor.count);
					size_t numFaces = indices.size() / 3;
					dracoMesh.SetNumFaces(numFaces);
					for (uint32_t i = 0; i < numFaces; i++)
					{
						draco::Mesh::Face face;
						face[0] = indices[(i * 3) + 0];
						face[1] = indices[(i * 3) + 1];
						face[2] = indices[(i * 3) + 2];
						dracoMesh.SetFace(draco::FaceIndex(i), face);
					}
				}
				else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
					auto indices_ptr = reinterpret_cast<const uint8_t*>(indices_buffer.data.data() + indices_bv.byteOffset);
					std::vector<uint8_t> indices(indices_ptr, indices_ptr + indicesAccessor.count);
					size_t numFaces = indices.size() / 3;
					dracoMesh.SetNumFaces(numFaces);
					for (uint32_t i = 0; i < numFaces; i++)
					{
						draco::Mesh::Face face;
						face[0] = indices[(i * 3) + 0];
						face[1] = indices[(i * 3) + 1];
						face[2] = indices[(i * 3) + 2];
						dracoMesh.SetFace(draco::FaceIndex(i), face);
					}
				}
				else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) {
					auto indices_ptr = reinterpret_cast<const int16_t*>(indices_buffer.data.data() + indices_bv.byteOffset);
					std::vector<int16_t> indices(indices_ptr, indices_ptr + indicesAccessor.count);
					size_t numFaces = indices.size() / 3;
					dracoMesh.SetNumFaces(numFaces);
					for (uint32_t i = 0; i < numFaces; i++)
					{
						draco::Mesh::Face face;
						face[0] = indices[(i * 3) + 0];
						face[1] = indices[(i * 3) + 1];
						face[2] = indices[(i * 3) + 2];
						dracoMesh.SetFace(draco::FaceIndex(i), face);
					}
				}
				else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					auto indices_ptr = reinterpret_cast<const uint16_t*>(indices_buffer.data.data() + indices_bv.byteOffset);
					std::vector<uint16_t> indices(indices_ptr, indices_ptr + indicesAccessor.count);
					size_t numFaces = indices.size() / 3;
					dracoMesh.SetNumFaces(numFaces);
					for (uint32_t i = 0; i < numFaces; i++)
					{
						draco::Mesh::Face face;
						face[0] = indices[(i * 3) + 0];
						face[1] = indices[(i * 3) + 1];
						face[2] = indices[(i * 3) + 2];
						dracoMesh.SetFace(draco::FaceIndex(i), face);
					}
				}
				else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					auto indices_ptr = reinterpret_cast<const uint32_t*>(indices_buffer.data.data() + indices_bv.byteOffset);
					std::vector<uint32_t> indices(indices_ptr, indices_ptr + indicesAccessor.count);
					size_t numFaces = indices.size() / 3;
					dracoMesh.SetNumFaces(numFaces);
					for (uint32_t i = 0; i < numFaces; i++)
					{
						draco::Mesh::Face face;
						face[0] = indices[(i * 3) + 0];
						face[1] = indices[(i * 3) + 1];
						face[2] = indices[(i * 3) + 2];
						dracoMesh.SetFace(draco::FaceIndex(i), face);
					}
				}
				else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
					auto indices_ptr = reinterpret_cast<const float*>(indices_buffer.data.data() + indices_bv.byteOffset);
					std::vector<float> indices(indices_ptr, indices_ptr + indicesAccessor.count);
					size_t numFaces = indices.size() / 3;
					dracoMesh.SetNumFaces(numFaces);
					for (uint32_t i = 0; i < numFaces; i++)
					{
						draco::Mesh::Face face;
						face[0] = indices[(i * 3) + 0];
						face[1] = indices[(i * 3) + 1];
						face[2] = indices[(i * 3) + 2];
						dracoMesh.SetFace(draco::FaceIndex(i), face);
					}
				}


				bufferViewsToRemove.emplace(indicesAccessor.bufferView);
				indicesAccessor.bufferView = -1;
				indicesAccessor.byteOffset = 0;
			}
			for (const auto& attribute : primitive.attributes) {
				const auto& accessor = _model.accessors[attribute.second];
				tinygltf::Accessor attributeAccessor(accessor);
				int attId;
				switch (accessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_BYTE:attId = InitializePointAttribute<int8_t>(dracoMesh, attribute.first, attributeAccessor); break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:attId = InitializePointAttribute<uint8_t>(dracoMesh, attribute.first, attributeAccessor); break;
				case TINYGLTF_COMPONENT_TYPE_SHORT:attId = InitializePointAttribute<int16_t>(dracoMesh, attribute.first, attributeAccessor); break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:attId = InitializePointAttribute<uint16_t>(dracoMesh, attribute.first, attributeAccessor); break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:attId = InitializePointAttribute<uint32_t>(dracoMesh, attribute.first, attributeAccessor); break;
				case TINYGLTF_COMPONENT_TYPE_FLOAT:attId = InitializePointAttribute<float>(dracoMesh, attribute.first, attributeAccessor); break;
				default:std::cerr << "Unknown component type." << std::endl; break;
				}

				bufferViewsToRemove.emplace(accessor.bufferView);
				attributeAccessor.bufferView = -1;
				attributeAccessor.byteOffset = 0;
				_model.accessors[attribute.second] = attributeAccessor;
				dracoExtensionAttrObj.insert(std::make_pair(attribute.first, tinygltf::Value((int)dracoMesh.attribute(attId)->unique_id())));

			}

			if (primitive.targets.size() > 0) {
				encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
			}

			draco::EncoderBuffer encoderBuffer;
			const draco::Status status = encoder.EncodeMeshToBuffer(dracoMesh, &encoderBuffer);
			if (!status.ok()) {
				std::cerr << "Failed to encode the mesh: " << status.error_msg() << std::endl;
			}

			draco::Decoder decoder;
			draco::DecoderBuffer decoderBuffer;
			decoderBuffer.Init(encoderBuffer.data(), encoderBuffer.size());
			auto decodeResult = decoder.DecodeMeshFromBuffer(&decoderBuffer);
			if (!decodeResult.ok()) {
				std::cerr << "Draco failed to decode mesh" << std::endl;
			}
			if (primitive.indices != -1) {
				tinygltf::Accessor encodedIndexAccessor(_model.accessors[primitive.indices]);
				encodedIndexAccessor.count = encoder.num_encoded_faces() * 3;
				_model.accessors[primitive.indices] = encodedIndexAccessor;
			}
			for (const auto& dracoAttribute : dracoExtensionAttrObj) {
				auto accessorId = primitive.attributes.at(dracoAttribute.first);
				tinygltf::Accessor encodedAccessor(_model.accessors[accessorId]);
				encodedAccessor.count = encoder.num_encoded_points();
				_model.accessors[accessorId] = encodedAccessor;
			}
			dracoExtensionObj.insert(std::make_pair("attributes", dracoExtensionAttrObj));

			_model.buffers.push_back(tinygltf::Buffer());
			tinygltf::Buffer& gltfBuffer = _model.buffers.back();
			int bufferId = _model.buffers.size() - 1;
			gltfBuffer.data.resize(encoderBuffer.size());
			const char* dracoBuffer = encoderBuffer.data();
			for (unsigned int i = 0; i < encoderBuffer.size(); ++i)
				gltfBuffer.data[i] = *dracoBuffer++;
			_model.bufferViews.push_back(tinygltf::BufferView());
			tinygltf::BufferView& bv = _model.bufferViews.back();
			int id = _model.bufferViews.size() - 1;
			bv.buffer = bufferId;
			bv.byteLength = encoderBuffer.size();
			bv.byteOffset = 0;
			dracoExtensionObj.insert(std::make_pair("bufferView", tinygltf::Value(id)));

			tinygltf::Primitive resultPrimitive(primitive);
			resultPrimitive.extensions.insert(std::make_pair("KHR_draco_mesh_compression", tinygltf::Value(dracoExtensionObj)));
			mesh.primitives.at(j) = resultPrimitive;



		}

	}
	void CompressMeshByMeshopt(tinygltf::Mesh& mesh, const VertexCompressionOptions& vco) {
		// 1、Indexing
		// 2、Vertex cache optimization
		// 3、Overdraw optimization
		// 4、Vertex fetch optimization
		// 5、Vertex quantization
		// 6、Vertex/index buffer compression

		for (int j = 0; j < mesh.primitives.size(); ++j) {
			const auto& primitive = mesh.primitives.at(j);
			tinygltf::Value::Object meshoptExtensionObj;
			if (primitive.indices != -1) {
				tinygltf::Accessor indicesAccessor = _model.accessors[primitive.indices];
				tinygltf::BufferView indicesBufferView = _model.bufferViews[indicesAccessor.bufferView];
				tinygltf::Buffer indicesBuffer = _model.buffers[indicesBufferView.buffer];
			}
			for (const auto& attribute : primitive.attributes) {
				const auto& accessor = _model.accessors[attribute.second];
				tinygltf::Accessor attributeAccessor(accessor);
				tinygltf::BufferView& bufferView = _model.bufferViews[attributeAccessor.bufferView];
				tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
				auto getEncodeVertexBuffer = [&accessor](tinygltf::Accessor attributeAccessor, tinygltf::Model& model, int num = 3)->std::vector<unsigned char> {
					tinygltf::BufferView bufferView = model.bufferViews[attributeAccessor.bufferView];
					tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
					size_t bufferSize;
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE: bufferSize = meshopt_encodeVertexBufferBound(attributeAccessor.count, sizeof(int8_t) * num); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:bufferSize = meshopt_encodeVertexBufferBound(attributeAccessor.count, sizeof(uint8_t) * num); break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:bufferSize = meshopt_encodeVertexBufferBound(attributeAccessor.count, sizeof(int16_t) * num); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:bufferSize = meshopt_encodeVertexBufferBound(attributeAccessor.count, sizeof(uint16_t) * num); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:bufferSize = meshopt_encodeVertexBufferBound(attributeAccessor.count, sizeof(uint32_t) * num); break;
					default:
						bufferSize = meshopt_encodeVertexBufferBound(attributeAccessor.count, sizeof(float) * num);
						break;
					}
					std::vector<unsigned char> vbuf(bufferSize);
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE: vbuf.resize(meshopt_encodeVertexBuffer(&vbuf[0], vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(int8_t) * num)); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:vbuf.resize(meshopt_encodeVertexBuffer(&vbuf[0], vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(uint8_t) * num)); break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:vbuf.resize(meshopt_encodeVertexBuffer(&vbuf[0], vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(int16_t) * num)); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:vbuf.resize(meshopt_encodeVertexBuffer(&vbuf[0], vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(uint16_t) * num)); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:vbuf.resize(meshopt_encodeVertexBuffer(&vbuf[0], vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(uint32_t) * num)); break;
					default:vbuf.resize(meshopt_encodeVertexBuffer(&vbuf[0], vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(float) * num)); break;
					}
					return vbuf;
					};
				if (attribute.first == "POSITION") {

					//if (primitive.indices != -1) {
					//	tinygltf::Accessor& indicesAccessor = _model.accessors[primitive.indices];
					//	tinygltf::BufferView& indicesBufferView = _model.bufferViews[indicesAccessor.bufferView];
					//	tinygltf::Buffer& indicesBuffer = _model.buffers[indicesBufferView.buffer];
					//	std::vector<unsigned char> indices = indicesBuffer.data;
					//	//Vertex cache optimization
					//	meshopt_optimizeVertexCache(&indices[0], &indices[0], indicesAccessor.count, attributeAccessor.count);
					//	//Overdraw optimization
					//	//meshopt_optimizeOverdraw(indicesBuffer.data.data(), indicesBuffer.data.data(), indicesAccessor.count, &vertices[0].x, attributeAccessor.count, sizeof(Vertex), 1.05f);
					//	//Vertex fetch optimization
					//	std::vector<unsigned int> remap(attributeAccessor.count);
					//	size_t unique_vertices = meshopt_optimizeVertexFetchRemap(&remap[0], &indices[0], indicesAccessor.count, attributeAccessor.count);
					//	meshopt_remapIndexBuffer(&indices[0], &indices[0], indicesAccessor.count, &remap[0]);
					//	for (unsigned int i = 0; i < indices.size(); ++i)
					//		indicesBuffer.data[i] = indices[i];
					//}

					std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model);
					//tinygltf::Buffer testBuffer(buffer);
					//testBuffer.name = "10";
					//int resvb = meshopt_decodeVertexBuffer(testBuffer.data.data(), attributeAccessor.count, sizeof(float) * 3, &vbuf[0], vbuf.size());
					//bool t = testBuffer.data.data() == buffer.data.data();
					buffer.data.resize(vbuf.size());
					for (unsigned int i = 0; i < vbuf.size(); ++i)
						buffer.data[i] = vbuf[i];

					meshoptExtensionObj = tinygltf::Value::Object();
					meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
					meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value((int)vbuf.size())));
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(int8_t) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint8_t) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(int16_t) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint16_t) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint32_t) * 3)));
						break;
					default:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(float) * 3)));
						break;
					}
					meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value((int)attributeAccessor.count)));
					meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value((std::string)"ATTRIBUTES")));
					bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));

					if (primitive.indices != -1) {
						tinygltf::Accessor& indicesAccessor = _model.accessors[primitive.indices];
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
							ibuf.resize(meshopt_encodeIndexBuffer(&ibuf[0], ibuf.size(), reinterpret_cast<const uint16_t*>(indicesBuffer.data.data()), indicesAccessor.count));
						}
						else
						{
							ibuf.resize(meshopt_encodeIndexBuffer(&ibuf[0], ibuf.size(), reinterpret_cast<const uint32_t*>(indicesBuffer.data.data()), indicesAccessor.count));
						}
						indicesBuffer.data.resize(ibuf.size());
						for (unsigned int i = 0; i < ibuf.size(); ++i)
							indicesBuffer.data[i] = ibuf[i];

						meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)indicesBufferView.buffer)));
						meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value((int)ibuf.size())));
						meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value((int)indicesAccessor.count)));
						meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value((std::string)"TRIANGLES")));
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
					meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value((int)vbuf.size())));
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(int8_t) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint8_t) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(int16_t) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint16_t) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint32_t) * 3)));
						break;
					default:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(float) * 3)));
						break;
					}
					meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value((int)attributeAccessor.count)));
					meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value((std::string)"ATTRIBUTES")));
					bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
				}
				else if (attribute.first == "TEXCOORD_0") {
					std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model, 2);
					buffer.data.resize(vbuf.size());
					for (unsigned int i = 0; i < vbuf.size(); ++i)
						buffer.data[i] = vbuf[i];
					meshoptExtensionObj = tinygltf::Value::Object();
					meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
					meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value((int)vbuf.size())));
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(int8_t) * 2)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint8_t) * 2)));
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(int16_t) * 2)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint16_t) * 2)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint32_t) * 2)));
						break;
					default:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(float) * 2)));
						break;
					}
					meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value((int)attributeAccessor.count)));
					meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value((std::string)"ATTRIBUTES")));
					bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
				}
				else if (attribute.first == "_BATCHID") {
					std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model, 1);
					buffer.data.resize(vbuf.size());
					for (unsigned int i = 0; i < vbuf.size(); ++i)
						buffer.data[i] = vbuf[i];
					meshoptExtensionObj = tinygltf::Value::Object();
					meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
					meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value((int)vbuf.size())));
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(int8_t))));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint8_t))));
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(int16_t))));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint16_t))));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(uint32_t))));
						break;
					default:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value((int)sizeof(float))));
						break;
					}
					meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value((int)attributeAccessor.count)));
					meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value((std::string)"ATTRIBUTES")));
					bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
				}
			}
		}
	}
	void setDracoEncoderOptions(draco::Encoder& encoder, const VertexCompressionOptions& options) {
		encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, options.PositionQuantizationBits);
		encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, options.TexCoordQuantizationBits);
		encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, options.NormalQuantizationBits);
		encoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, options.ColorQuantizationBits);
		encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, options.GenericQuantizationBits);
		encoder.SetSpeedOptions(options.Speed, options.Speed);
		encoder.SetTrackEncodedProperties(true);
	}
	bool geometryCompression(const std::string& extensionName, const VertexCompressionOptions& vco) {
		_model.extensionsRequired.push_back(extensionName);
		_model.extensionsUsed.push_back(extensionName);

		if (extensionName == "KHR_draco_mesh_compression") {
			for (auto& mesh : _model.meshes) {
				std::unordered_set<int> bufferViewsToRemove;

				CompressMeshByDraco(mesh, bufferViewsToRemove, vco);
				//TODO:this can be simplified such as meshopt
				std::vector<int> bufferViewsToRemoveVector(bufferViewsToRemove.begin(), bufferViewsToRemove.end());
				for (int i = 0; i < bufferViewsToRemoveVector.size(); ++i) {
					int bufferViewId = bufferViewsToRemoveVector.at(i);
					auto bufferView = _model.bufferViews[bufferViewId];
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
							auto dracoExtension = primitive.extensions.find(extensionName);
							if (dracoExtension != primitive.extensions.end()) {
								auto dracoBufferViewId = dracoExtension->second.Get("bufferView").GetNumberAsInt();
								dracoExtension->second.Get("attributes");

								tinygltf::Value::Object obj;
								obj.insert(std::make_pair("bufferView", tinygltf::Value(dracoBufferViewId - 1)));
								auto attributesValue = dracoExtension->second.Get("attributes");
								auto attributesObject = attributesValue.Get<tinygltf::Value::Object>();
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
			return true;
		}
		else {
			for (auto& mesh : _model.meshes) {
				std::unordered_set<int> buffersToRemove;
				CompressMeshByMeshopt(mesh, vco);
			}
			return true;
		}
		return false;
	}
	int getOrCreateGltfTexture(osg::Texture* osgTexture, const TextureType& type) {
		if (osgTexture == NULL) {
			return -1;
		}
		if (osgTexture->getNumImages() < 1) {
			return -1;
		}
		osg::ref_ptr<osg::Image> osgImage = osgTexture->getImage(0);
		if (!osgImage.valid()) {
			return -1;
		}
		for (unsigned int i = 0; i < _textures.size(); i++)
		{
			const osg::Texture* existTexture = _textures[i].get();
			const std::string existPathName = existTexture->getImage(0)->getFileName();
			osg::Texture::WrapMode existWrapS = existTexture->getWrap(osg::Texture::WRAP_S);
			osg::Texture::WrapMode existWrapT = existTexture->getWrap(osg::Texture::WRAP_T);
			osg::Texture::WrapMode existWrapR = existTexture->getWrap(osg::Texture::WRAP_R);
			osg::Texture::FilterMode existMinFilter = existTexture->getFilter(osg::Texture::MIN_FILTER);
			osg::Texture::FilterMode existMaxFilter = existTexture->getFilter(osg::Texture::MAG_FILTER);

			const std::string newPathName = osgImage->getFileName();
			osg::Texture::WrapMode newWrapS = osgTexture->getWrap(osg::Texture::WRAP_S);
			osg::Texture::WrapMode newWrapT = osgTexture->getWrap(osg::Texture::WRAP_T);
			osg::Texture::WrapMode newWrapR = osgTexture->getWrap(osg::Texture::WRAP_R);
			osg::Texture::FilterMode newMinFilter = osgTexture->getFilter(osg::Texture::MIN_FILTER);
			osg::Texture::FilterMode newMaxFilter = osgTexture->getFilter(osg::Texture::MAG_FILTER);
			if (existPathName == newPathName
				&& existWrapS == newWrapS
				&& existWrapT == newWrapT
				&& existWrapR == newWrapR
				&& existMinFilter == newMinFilter
				&& existMaxFilter == newMaxFilter
				)
			{
				return i;
			}
		}
		int index = _model.textures.size();
		_textures.push_back(osgTexture);
		// Flip the image before writing
		osg::ref_ptr< osg::Image > flipped = new osg::Image(*osgImage);
		//need to forbid filpVertical when use texture atlas 
		//flipped->flipVertical();
		//textureOptimize(flipped);

		std::string filename;
		std::string ext = "png";
		std::string ktxVersion = "2.0";
		std::string mimeType = "";
		switch (type)
		{
		case TextureType::KTX:
			ext = "ktx";
			ktxVersion = "1.0";
			mimeType = "image/ktx";
			break;
		case TextureType::KTX2:
			ext = "ktx";
			mimeType = "image/ktx2";
			break;
		case TextureType::PNG:
			ext = "png";
			mimeType = "image/png";
			break;
		case TextureType::JPG:
			ext = "jpg";
			mimeType = "image/jpeg";
			break;
		case TextureType::WEBP:
			ext = "webp";
			mimeType = "image/webp";
			break;
		default:
			break;
		}
		// If the image has a filename try to hash it so we only write out one copy of it.  
		if (!osgImage->getFileName().empty())
		{
			std::string data(reinterpret_cast<char const*>(osgImage->data()));
			filename = Stringify() << std::hex << hashString(data) << "." << ext;
			std::ifstream fileExists(filename);
			if ((!fileExists.good())|| (fileExists.peek() == std::ifstream::traits_type::eof()))
			{
				if (type == TextureType::KTX) {
					osg::ref_ptr<osgDB::Options> option = new osgDB::Options;
					option->setOptionString("Version=" + ktxVersion);
					if (!(osgDB::writeImageFile(*flipped.get(), filename, option.get()))) {
						osg::notify(osg::FATAL) << std::endl;
					}
				}
				else {
					if (!(osgDB::writeImageFile(*flipped.get(), filename))) {
						mimeType = "image/png";
						filename = Stringify() << std::hex << hashString(data) << ".png";
						std::ifstream fileExistsPng(filename);
						if ((!fileExistsPng.good()) || (fileExistsPng.peek() == std::ifstream::traits_type::eof()))
						{
							if (!(osgDB::writeImageFile(*flipped.get(), filename))) {
								osg::notify(osg::FATAL) << std::endl;
							}
						}
						fileExistsPng.close();
					}
				}
			}
			fileExists.close();
		}
		else
		{
			std::ifstream fileExists(filename);
			// Otherwise just find a filename that doesn't exist
			int fileNameInc = 0;
			do
			{
				std::stringstream ss;
				ss << fileNameInc << "." << ext;
				filename = ss.str();
				fileNameInc++;
			} while (fileExists.good());
			fileExists.close();
			if (type == TextureType::KTX) {
				osg::ref_ptr<osgDB::Options> option = new osgDB::Options;
				option->setOptionString("Version=" + ktxVersion);
				if (!(osgDB::writeImageFile(*flipped.get(), filename, option.get()))) {
					osg::notify(osg::FATAL) << std::endl;
				}
			}
			else {
				if (!(osgDB::writeImageFile(*flipped.get(), filename))) {
					osg::notify(osg::FATAL) << std::endl;
				}
			}
		}

		tinygltf::Image image;
		//image.name = osgDB::convertStringFromCurrentCodePageToUTF8(osgDB::getSimpleFileName(osgImage->getFileName()));if defined image by buffer,name must be not defined;
		std::ifstream file(filename, std::ios::binary);
		std::vector<unsigned char> imageData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		tinygltf::BufferView gltfBufferView;
		int bufferViewIndex = -1;
		bufferViewIndex = static_cast<int>(_model.bufferViews.size());
		gltfBufferView.buffer = _model.buffers.size();
		gltfBufferView.byteOffset = 0;
		gltfBufferView.byteLength = static_cast<int>(imageData.size());
		gltfBufferView.target = TINYGLTF_TEXTURE_TARGET_TEXTURE2D;
		gltfBufferView.name = filename;
		_model.bufferViews.push_back(gltfBufferView);

		tinygltf::Buffer gltfBuffer;
		gltfBuffer.data = imageData;
		_model.buffers.push_back(gltfBuffer);

		image.mimeType = mimeType;
		image.bufferView = bufferViewIndex; //_model.bufferViews.size() the only bufferView in this model
		_model.images.push_back(image);
		file.close();
		//remove(filename.c_str());

		// Add the sampler
		tinygltf::Sampler sampler;
		osg::Texture::WrapMode wrapS = osgTexture->getWrap(osg::Texture::WRAP_S);
		osg::Texture::WrapMode wrapT = osgTexture->getWrap(osg::Texture::WRAP_T);
		osg::Texture::WrapMode wrapR = osgTexture->getWrap(osg::Texture::WRAP_R);

		// Validate the clamp mode to be compatible with webgl
		if ((wrapS == osg::Texture::CLAMP) || (wrapS == osg::Texture::CLAMP_TO_BORDER))
		{
			wrapS = osg::Texture::CLAMP_TO_EDGE;
		}
		if ((wrapT == osg::Texture::CLAMP) || (wrapT == osg::Texture::CLAMP_TO_BORDER))
		{
			wrapT = osg::Texture::CLAMP_TO_EDGE;
		}
		//if ((wrapR == osg::Texture::CLAMP) || (wrapR == osg::Texture::CLAMP_TO_BORDER))
		//{
		//	wrapR = osg::Texture::CLAMP_TO_EDGE;
		//}
		sampler.wrapS = wrapS;
		sampler.wrapT = wrapT;
		//sampler.wrapR = wrapR;
		sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR; //osgTexture->getFilter(osg::Texture::MIN_FILTER);
		sampler.magFilter = osgTexture->getFilter(osg::Texture::MAG_FILTER);
		int samplerIndex = -1;
		for (int i = 0; i < _model.samplers.size(); ++i) {
			const tinygltf::Sampler existSampler = _model.samplers.at(i);
			if (existSampler.wrapR == sampler.wrapR && existSampler.wrapT == sampler.wrapT && existSampler.minFilter == sampler.minFilter && existSampler.magFilter == sampler.magFilter) {
				samplerIndex = i;
			}
		}
		if (samplerIndex == -1) {
			samplerIndex = _model.samplers.size();
			_model.samplers.push_back(sampler);
		}
		// Add the texture
		tinygltf::Texture texture;
		if (type == TextureType::KTX2)
		{
			_model.extensionsRequired.push_back("KHR_texture_basisu");
			_model.extensionsUsed.push_back("KHR_texture_basisu");
			texture.source = -1;
			tinygltf::Value::Object ktxExtensionObj;
			ktxExtensionObj.insert(std::make_pair("source", index));
			texture.extensions.insert(std::make_pair("KHR_texture_basisu", tinygltf::Value(ktxExtensionObj)));
		}
		else if (type == TextureType::WEBP) {
			_model.extensionsRequired.push_back("EXT_texture_webp");
			_model.extensionsUsed.push_back("EXT_texture_webp");
			texture.source = -1;
			tinygltf::Value::Object webpExtensionObj;
			webpExtensionObj.insert(std::make_pair("source", index));
			texture.extensions.insert(std::make_pair("EXT_texture_webp", tinygltf::Value(webpExtensionObj)));
		}
		else {
			texture.source = index;
		}
		texture.sampler = samplerIndex;
		_model.textures.push_back(texture);
		return index;
	}
	void setGeneralGltfMaterialProperties(tinygltf::Material& gltfMaterial, const osg::ref_ptr<GltfMaterial>& material, const TextureType& type) {
		osg::ref_ptr<osg::Texture2D> normalTexture = material->normalTexture;
		if (normalTexture) {
			gltfMaterial.normalTexture.index = getOrCreateGltfTexture(normalTexture, type);
			gltfMaterial.normalTexture.texCoord = 0;
		}
		osg::ref_ptr<osg::Texture2D> occlusionTexture = material->occlusionTexture;
		if (occlusionTexture) {
			gltfMaterial.occlusionTexture.index = getOrCreateGltfTexture(occlusionTexture, type);
			gltfMaterial.occlusionTexture.texCoord = 0;
		}
		osg::ref_ptr<osg::Texture2D> emissiveTexture = material->emissiveTexture;
		if (emissiveTexture) {
			gltfMaterial.emissiveTexture.index = getOrCreateGltfTexture(emissiveTexture, type);
			gltfMaterial.emissiveTexture.texCoord = 0;
		}
		gltfMaterial.emissiveFactor.push_back(material->emissiveFactor[0]);
		gltfMaterial.emissiveFactor.push_back(material->emissiveFactor[1]);
		gltfMaterial.emissiveFactor.push_back(material->emissiveFactor[2]);

		if (material->enable_KHR_materials_clearcoat) {
			_model.extensionsRequired.push_back("KHR_materials_clearcoat");
			_model.extensionsUsed.push_back("KHR_materials_clearcoat");

			tinygltf::Value::Object obj;
			obj.insert(std::make_pair("clearcoatFactor", tinygltf::Value(material->clearcoatFactor)));
			obj.insert(std::make_pair("clearcoatRoughnessFactor", tinygltf::Value(material->clearcoatRoughnessFactor)));

			tinygltf::Value::Object clearcoatTextureInfo;
			clearcoatTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(material->clearcoatTexture, type)));
			clearcoatTextureInfo.insert(std::make_pair("texCoord", 0));
			obj.insert(std::make_pair("clearcoatTexture", tinygltf::Value(clearcoatTextureInfo)));

			tinygltf::Value::Object clearcoatRoughnessTextureInfo;
			clearcoatRoughnessTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(material->clearcoatRoughnessTexture, type)));
			clearcoatRoughnessTextureInfo.insert(std::make_pair("texCoord", 0));
			obj.insert(std::make_pair("clearcoatRoughnessTexture", tinygltf::Value(clearcoatRoughnessTextureInfo)));

			tinygltf::Value::Object clearcoatNormalTextureInfo;
			clearcoatNormalTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(material->clearcoatNormalTexture, type)));
			clearcoatNormalTextureInfo.insert(std::make_pair("texCoord", 0));
			obj.insert(std::make_pair("clearcoatNormalTexture", tinygltf::Value(clearcoatNormalTextureInfo)));
			gltfMaterial.extensions["KHR_materials_clearcoat"] = tinygltf::Value(obj);
		}

		if (material->enable_KHR_materials_anisotropy) {
			_model.extensionsRequired.push_back("KHR_materials_anisotropy");
			_model.extensionsUsed.push_back("KHR_materials_anisotropy");

			tinygltf::Value::Object obj;
			obj.insert(std::make_pair("anisotropyStrength", tinygltf::Value(material->anisotropyStrength)));
			obj.insert(std::make_pair("anisotropyRotation", tinygltf::Value(material->anisotropyRotation)));

			tinygltf::Value::Object anisotropyTextureInfo;
			anisotropyTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(material->anisotropyTexture, type)));
			anisotropyTextureInfo.insert(std::make_pair("texCoord", 0));
			obj.insert(std::make_pair("anisotropyTexture", tinygltf::Value(anisotropyTextureInfo)));
			gltfMaterial.extensions["KHR_materials_anisotropy"] = tinygltf::Value(obj);
		}

		if (material->enable_KHR_materials_emissive_strength) {
			_model.extensionsRequired.push_back("KHR_materials_emissive_strength");
			_model.extensionsUsed.push_back("KHR_materials_emissive_strength");

			tinygltf::Value::Object obj;
			obj.insert(std::make_pair("emissiveStrength", tinygltf::Value(material->emissiveStrength)));
			gltfMaterial.extensions["KHR_materials_emissive_strength"] = tinygltf::Value(obj);
		}
	}

	//convert image size to the power of 2
	void textureOptimize(osg::ref_ptr<osg::Image> img) {
		auto findNearestGreaterPowerOfTwo = [](int n) {
			int powerOfTwo = 1;
			while (powerOfTwo < n) {
				powerOfTwo <<= 1;
			}
			return powerOfTwo;
			};

		const int oldS = img->s();
		const int oldR = img->t();
		const int newS = findNearestGreaterPowerOfTwo(oldS);
		const int newR = findNearestGreaterPowerOfTwo(oldR);
		img->scaleImage(newS, newR, 1);
	}
	void mergeBuffers() {
		tinygltf::Buffer totalBuffer;
		tinygltf::Buffer fallbackBuffer;
		fallbackBuffer.name = "fallback";
		int byteOffset = 0, fallbackByteOffset = 0;
		for (auto& bufferView : _model.bufferViews) {
			auto& meshoptExtension = bufferView.extensions.find("EXT_meshopt_compression");
			if (meshoptExtension != bufferView.extensions.end()) {
				auto& bufferIndex = meshoptExtension->second.Get("buffer");
				auto& buffer = _model.buffers[bufferIndex.GetNumberAsInt()];
				totalBuffer.data.insert(totalBuffer.data.end(), buffer.data.begin(), buffer.data.end());

				tinygltf::Value::Object newMeshoptExtension;
				newMeshoptExtension.insert(std::make_pair("buffer", 0));
				newMeshoptExtension.insert(std::make_pair("byteOffset", tinygltf::Value(byteOffset)));
				newMeshoptExtension.insert(std::make_pair("byteLength", meshoptExtension->second.Get("byteLength")));
				newMeshoptExtension.insert(std::make_pair("byteStride", meshoptExtension->second.Get("byteStride")));
				newMeshoptExtension.insert(std::make_pair("mode", meshoptExtension->second.Get("mode")));
				newMeshoptExtension.insert(std::make_pair("count", meshoptExtension->second.Get("count")));
				byteOffset += meshoptExtension->second.Get("byteLength").GetNumberAsInt();
				bufferView.extensions.clear();
				bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", newMeshoptExtension));
				//4-byte aligned 
				int needAdd = 4 - byteOffset % 4;
				if (needAdd % 4 != 0) {
					for (int i = 0; i < needAdd; ++i) {
						totalBuffer.data.push_back(0);
						byteOffset++;
					}
				}

				bufferView.buffer = 1;
				bufferView.byteOffset = fallbackByteOffset;
				fallbackByteOffset += bufferView.byteLength;
				//4-byte aligned 
				int fallbackNeedAdd = 4 - fallbackByteOffset % 4;
				if (fallbackNeedAdd != 0) {
					for (int i = 0; i < fallbackNeedAdd; ++i) {
						fallbackByteOffset++;
					}
				}
			}
			else {
				auto& buffer = _model.buffers[bufferView.buffer];
				totalBuffer.data.insert(totalBuffer.data.end(), buffer.data.begin(), buffer.data.end());
				bufferView.buffer = 0;
				bufferView.byteOffset = byteOffset;
				byteOffset += bufferView.byteLength;

				//4-byte aligned 
				int needAdd = 4 - byteOffset % 4;
				if (needAdd % 4 != 0) {
					for (int i = 0; i < needAdd; ++i) {
						totalBuffer.data.push_back(0);
						byteOffset++;
					}
				}
			}
		}
		_model.buffers.clear();
		_model.buffers.push_back(totalBuffer);
		if (fallbackByteOffset>0) {
			fallbackBuffer.data.resize(fallbackByteOffset);
			tinygltf::Value::Object bufferMeshoptExtension;
			bufferMeshoptExtension.insert(std::make_pair("fallback", true));
			fallbackBuffer.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(bufferMeshoptExtension)));
			_model.buffers.push_back(fallbackBuffer);
		}
	}
	void mergeMeshes() {
		std::map<int, std::vector<tinygltf::Primitive>> materialPrimitiveMap;
		for (auto& mesh : _model.meshes) {
			if (mesh.primitives.size() > 0) {
				auto& item = materialPrimitiveMap.find(mesh.primitives[0].material);
				if (item != materialPrimitiveMap.end()) {
					item->second.push_back(mesh.primitives[0]);
				}
				else {
					std::vector<tinygltf::Primitive> primitives;
					primitives.push_back(mesh.primitives[0]);
					materialPrimitiveMap.insert(std::make_pair(mesh.primitives[0].material, primitives));
				}
			}
		}
		
		std::vector<tinygltf::Accessor> accessors;
		std::vector<tinygltf::BufferView> bufferViews;
		std::vector<tinygltf::Buffer> buffers;
		std::vector<tinygltf::Primitive> primitives;
		for (auto& image : _model.images) {
			tinygltf::BufferView bufferView = _model.bufferViews[image.bufferView];
			tinygltf::Buffer buffer = _model.buffers[bufferView.buffer];

			bufferView.buffer = buffers.size();
			image.bufferView = bufferViews.size();
			bufferViews.push_back(bufferView);
			buffers.push_back(buffer);
		}

		for (const auto& pair : materialPrimitiveMap) {
			tinygltf::Accessor totalIndicesAccessor;
			totalIndicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
			tinygltf::BufferView totalIndicesBufferView;
			tinygltf::Buffer totalIndicesBuffer;

			tinygltf::BufferView totalNormalsBufferView;
			tinygltf::Buffer totalNormalsBuffer;
			tinygltf::Accessor totalNormalsAccessor;

			tinygltf::BufferView totalVerticesBufferView;
			tinygltf::Buffer totalVerticesBuffer;
			tinygltf::Accessor totalVerticesAccessor;

			tinygltf::BufferView totalTexcoordsBufferView;
			tinygltf::Buffer totalTexcoordsBuffer;
			tinygltf::Accessor totalTexcoordsAccessor;

			tinygltf::BufferView totalBatchIdsBufferView;
			tinygltf::Buffer totalBatchIdsBuffer;
			tinygltf::Accessor totalBatchIdsAccessor;

			tinygltf::BufferView totalColorsBufferView;
			tinygltf::Buffer totalColorsBuffer;
			tinygltf::Accessor totalColorsAccessor;


			tinygltf::Primitive totalPrimitive;
			totalPrimitive.mode = 4;//triangles
			totalPrimitive.material = pair.first;

			unsigned int count = 0;

			for (const auto& prim : pair.second) {
				tinygltf::Accessor& oldIndicesAccessor = _model.accessors[prim.indices];
				totalIndicesAccessor = oldIndicesAccessor;
				tinygltf::Accessor& oldVerticesAccessor = _model.accessors[prim.attributes.find("POSITION")->second];
				totalVerticesAccessor = oldVerticesAccessor;
				auto& normalAttr = prim.attributes.find("NORMAL");
				if (normalAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldNormalsAccessor = _model.accessors[normalAttr->second];
					totalNormalsAccessor = oldNormalsAccessor;
				}
				auto& texAttr = prim.attributes.find("TEXCOORD_0");
				if (texAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldTexcoordsAccessor = _model.accessors[texAttr->second];
					totalTexcoordsAccessor = oldTexcoordsAccessor;
				}
				auto& batchidAttr = prim.attributes.find("_BATCHID");
				if (batchidAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldBatchIdsAccessor = _model.accessors[batchidAttr->second];
					totalBatchIdsAccessor = oldBatchIdsAccessor;
				}
				auto& colorAttr = prim.attributes.find("COLOR_0");
				if (colorAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldColorsAccessor = _model.accessors[colorAttr->second];
					totalColorsAccessor = oldColorsAccessor;
				}

				const tinygltf::Accessor accessor = _model.accessors[prim.indices];
				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					totalIndicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
					break;
				}
				count += accessor.count;
				if (count > std::numeric_limits<unsigned short>::max()) {
					totalIndicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
					break;
				}
			}

			totalIndicesAccessor.count = 0;
			totalVerticesAccessor.count = 0;
			totalNormalsAccessor.count = 0;
			totalTexcoordsAccessor.count = 0;
			totalBatchIdsAccessor.count = 0;
			totalColorsAccessor.count = 0;

			auto reindexBufferAndBvAndAccessor = [&](tinygltf::Accessor accessor,tinygltf::BufferView& tBv,tinygltf::Buffer& tBuffer,tinygltf::Accessor& newAccessor,unsigned int sum=0,bool isIndices=false) {
				tinygltf::BufferView& bv = _model.bufferViews[accessor.bufferView];
				tinygltf::Buffer& buffer = _model.buffers[bv.buffer];

				tBv.byteStride = bv.byteStride;
				tBv.target = bv.target;
				if(accessor.componentType!=newAccessor.componentType){
					if (newAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT && accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
						unsigned short* oldIndicesChar = (unsigned short*)&buffer.data[0];
						std::vector<unsigned int> indices;
						for (size_t i = 0; i < accessor.count; ++i) {
							unsigned short ushortVal = *oldIndicesChar++;
							unsigned int uintVal = (unsigned int)(ushortVal + sum);
							indices.push_back(uintVal);
						}		
						unsigned char* indicesUChar = (unsigned char*)&indices[0];
						const unsigned int size = bv.byteLength * 2;
						for (unsigned int k = 0; k < size; ++k) {
							tBuffer.data.push_back(*indicesUChar++);
						}
						tBv.byteLength += bv.byteLength * 2;
					}

				}
				else {
					if (isIndices) {
						if (newAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
							unsigned int* oldIndicesChar = (unsigned int*)&buffer.data[0];
							std::vector<unsigned int> indices;
							for (unsigned int i = 0; i < accessor.count; ++i) {
								unsigned int oldUintVal = *oldIndicesChar++;
								unsigned int uintVal = oldUintVal + sum;
								indices.push_back(uintVal);
							}
							unsigned char* indicesUChar = (unsigned char*)&indices[0];
							const unsigned int size = bv.byteLength;
							for (unsigned int i = 0; i < size; ++i) {
								tBuffer.data.push_back(*indicesUChar++);
							}
						}
						else {
							unsigned short* oldIndicesChar = (unsigned short*)&buffer.data[0];
							std::vector<unsigned short> indices;
							for (size_t i = 0; i < accessor.count; ++i) {
								unsigned short oldUshortVal = *oldIndicesChar++;
								unsigned short ushortVal = oldUshortVal + sum;
								indices.push_back(ushortVal);
							}
							unsigned char* indicesUChar = (unsigned char*)&indices[0];
							const unsigned int size = bv.byteLength;
							for (size_t i = 0; i < size; ++i) {
								tBuffer.data.push_back(*indicesUChar++);
							}
						}
					}
					else {
						tBuffer.data.insert(tBuffer.data.end(), buffer.data.begin(), buffer.data.end());
					}
					tBv.byteLength += bv.byteLength;
				}
				newAccessor.count += accessor.count;

				if (accessor.minValues.size()) {
					if (newAccessor.minValues.size() == accessor.minValues.size()) {
						for (int k = 0; k < newAccessor.minValues.size(); ++k) {
							newAccessor.minValues[k] = osg::minimum(newAccessor.minValues[k], accessor.minValues[k]);
							newAccessor.maxValues[k] = osg::maximum(newAccessor.maxValues[k], accessor.maxValues[k]);
						}
					}
					else {
						newAccessor.minValues = accessor.minValues;
						newAccessor.maxValues = accessor.maxValues;
					}
				}
			};

			unsigned int sum = 0;
			for (const auto& prim : pair.second) {

				tinygltf::Accessor& oldVerticesAccessor = _model.accessors[prim.attributes.find("POSITION")->second];
				reindexBufferAndBvAndAccessor(oldVerticesAccessor, totalVerticesBufferView, totalVerticesBuffer, totalVerticesAccessor);

				tinygltf::Accessor& oldIndicesAccessor = _model.accessors[prim.indices];
				reindexBufferAndBvAndAccessor(oldIndicesAccessor, totalIndicesBufferView, totalIndicesBuffer, totalIndicesAccessor, sum, true);
				sum += oldVerticesAccessor.count;

				auto& normalAttr = prim.attributes.find("NORMAL");
				if (normalAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldNormalsAccessor = _model.accessors[normalAttr->second];
					reindexBufferAndBvAndAccessor(oldNormalsAccessor, totalNormalsBufferView, totalNormalsBuffer, totalNormalsAccessor);
				}
				auto& texAttr = prim.attributes.find("TEXCOORD_0");
				if (texAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldTexcoordsAccessor = _model.accessors[texAttr->second];
					reindexBufferAndBvAndAccessor(oldTexcoordsAccessor, totalTexcoordsBufferView, totalTexcoordsBuffer, totalTexcoordsAccessor);
				}
				auto& batchidAttr = prim.attributes.find("_BATCHID");
				if (batchidAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldBatchIdsAccessor = _model.accessors[batchidAttr->second];
					reindexBufferAndBvAndAccessor(oldBatchIdsAccessor, totalBatchIdsBufferView, totalBatchIdsBuffer, totalBatchIdsAccessor);
				}
				auto& colorAttr = prim.attributes.find("COLOR_0");
				if (colorAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldColorsAccessor = _model.accessors[colorAttr->second];
					reindexBufferAndBvAndAccessor(oldColorsAccessor, totalColorsBufferView, totalColorsBuffer, totalColorsAccessor);
				}


				//TODO:1、merge buffer and reindex buffer
				//TODO:2、merge bufferView and reindex bufferView
				//TODO:3、merge accessor and reindex accessor
			}

			totalIndicesBufferView.buffer = buffers.size();
			totalIndicesAccessor.bufferView = bufferViews.size();
			buffers.push_back(totalIndicesBuffer);
			bufferViews.push_back(totalIndicesBufferView);
			totalPrimitive.indices = accessors.size();
			accessors.push_back(totalIndicesAccessor);

			totalVerticesBufferView.buffer = buffers.size();
			totalVerticesAccessor.bufferView = bufferViews.size();
			buffers.push_back(totalVerticesBuffer);
			bufferViews.push_back(totalVerticesBufferView);
			totalPrimitive.attributes.insert(std::make_pair("POSITION", accessors.size()));
			accessors.push_back(totalVerticesAccessor);

			if (totalNormalsAccessor.count) {
				totalNormalsBufferView.buffer = buffers.size();
				totalNormalsAccessor.bufferView = bufferViews.size();
				buffers.push_back(totalNormalsBuffer);
				bufferViews.push_back(totalNormalsBufferView);
				totalPrimitive.attributes.insert(std::make_pair("NORMAL", accessors.size()));
				accessors.push_back(totalNormalsAccessor);
			}

			if (totalTexcoordsAccessor.count) {
				totalTexcoordsBufferView.buffer = buffers.size();
				totalTexcoordsAccessor.bufferView = bufferViews.size();
				buffers.push_back(totalTexcoordsBuffer);
				bufferViews.push_back(totalTexcoordsBufferView);
				totalPrimitive.attributes.insert(std::make_pair("TEXCOORD_0", accessors.size()));
				accessors.push_back(totalTexcoordsAccessor);
			}

			if (totalBatchIdsAccessor.count) {
				totalBatchIdsBufferView.buffer = buffers.size();
				totalBatchIdsAccessor.bufferView = bufferViews.size();
				buffers.push_back(totalBatchIdsBuffer);
				bufferViews.push_back(totalBatchIdsBufferView);
				totalPrimitive.attributes.insert(std::make_pair("_BATCHID", accessors.size()));
				accessors.push_back(totalBatchIdsAccessor);
			}

			if (totalColorsAccessor.count) {
				totalColorsBufferView.buffer = buffers.size();
				totalColorsAccessor.bufferView = bufferViews.size();
				buffers.push_back(totalColorsBuffer);
				bufferViews.push_back(totalColorsBufferView);
				totalPrimitive.attributes.insert(std::make_pair("COLOR_0", accessors.size()));
				accessors.push_back(totalColorsAccessor);
			}

			primitives.push_back(totalPrimitive);
		}


		_model.buffers.clear();
		_model.bufferViews.clear();
		_model.accessors.clear();
		_model.buffers = buffers;
		_model.bufferViews = bufferViews;
		_model.accessors = accessors;

		tinygltf::Mesh totalMesh;
		totalMesh.primitives = primitives;
		//for (auto& mesh : _model.meshes) {
		//	if (mesh.primitives.size() > 0) {
		//		totalMesh.primitives.push_back(mesh.primitives[0]);
		//	}
		//}


		_model.nodes.clear();
		tinygltf::Node node;
		node.mesh = 0;
		_model.nodes.push_back(node);
		_model.meshes.clear();
		_model.meshes.push_back(totalMesh);
		_model.scenes[0].nodes.clear();
		_model.scenes[0].nodes.push_back(0);



	}
public:
	GltfUtils(tinygltf::Model& model) :_model(model) {

	}
	int textureCompression(const TextureType& type, const osg::ref_ptr<osg::StateSet>& stateSet) {
		tinygltf::Material gltfMaterial;
		gltfMaterial.doubleSided = ((stateSet->getMode(GL_CULL_FACE) & osg::StateAttribute::ON) == 0);
		if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON) {
			gltfMaterial.alphaMode = "BLEND";
		}

		osg::Material* material = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
		GltfPbrMetallicRoughnessMaterial* pbrMRMaterial = dynamic_cast<GltfPbrMetallicRoughnessMaterial*>(material);
		if (pbrMRMaterial) {
			setGeneralGltfMaterialProperties(gltfMaterial, pbrMRMaterial, type);

			gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index = getOrCreateGltfTexture(pbrMRMaterial->metallicRoughnessTexture, type);
			gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = 0;
			gltfMaterial.pbrMetallicRoughness.baseColorTexture.index = getOrCreateGltfTexture(pbrMRMaterial->baseColorTexture, type);
			gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
			gltfMaterial.pbrMetallicRoughness.baseColorFactor = {
				pbrMRMaterial->baseColorFactor[0],
				pbrMRMaterial->baseColorFactor[1],
				pbrMRMaterial->baseColorFactor[2],
				pbrMRMaterial->baseColorFactor[3]
			};
			gltfMaterial.pbrMetallicRoughness.metallicFactor = pbrMRMaterial->metallicFactor;
			gltfMaterial.pbrMetallicRoughness.roughnessFactor = pbrMRMaterial->roughnessFactor;

#pragma region Extensions
			if (pbrMRMaterial->enable_KHR_materials_ior) {
				_model.extensionsRequired.push_back("KHR_materials_ior");
				_model.extensionsUsed.push_back("KHR_materials_ior");

				tinygltf::Value::Object obj;
				obj.insert(std::make_pair("ior", tinygltf::Value(pbrMRMaterial->ior)));
				gltfMaterial.extensions["KHR_materials_ior"] = tinygltf::Value(obj);
			}

			if (pbrMRMaterial->enable_KHR_materials_sheen) {
				_model.extensionsRequired.push_back("KHR_materials_sheen");
				_model.extensionsUsed.push_back("KHR_materials_sheen");

				tinygltf::Value::Object obj;

				tinygltf::Value::Array sheenColorFactor;
				sheenColorFactor.push_back(tinygltf::Value(pbrMRMaterial->sheenColorFactor[0]));
				sheenColorFactor.push_back(tinygltf::Value(pbrMRMaterial->sheenColorFactor[1]));
				sheenColorFactor.push_back(tinygltf::Value(pbrMRMaterial->sheenColorFactor[2]));
				obj.insert(std::make_pair("sheenColorFactor", tinygltf::Value(sheenColorFactor)));
				obj.insert(std::make_pair("sheenRoughnessFactor", tinygltf::Value(pbrMRMaterial->sheenRoughnessFactor)));

				tinygltf::Value::Object sheenColorTextureInfo;
				sheenColorTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(pbrMRMaterial->sheenColorTexture, type)));
				sheenColorTextureInfo.insert(std::make_pair("texCoord", 0));
				obj.insert(std::make_pair("sheenColorTexture", tinygltf::Value(sheenColorTextureInfo)));

				tinygltf::Value::Object sheenRoughnessTextureInfo;
				sheenRoughnessTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(pbrMRMaterial->sheenRoughnessTexture, type)));
				sheenRoughnessTextureInfo.insert(std::make_pair("texCoord", 0));
				obj.insert(std::make_pair("sheenRoughnessTexture", tinygltf::Value(sheenRoughnessTextureInfo)));

				gltfMaterial.extensions["KHR_materials_sheen"] = tinygltf::Value(obj);
			}

			if (pbrMRMaterial->enable_KHR_materials_volume) {
				_model.extensionsRequired.push_back("KHR_materials_volume");
				_model.extensionsUsed.push_back("KHR_materials_volume");

				tinygltf::Value::Object obj;

				tinygltf::Value::Array attenuationColor;
				attenuationColor.push_back(tinygltf::Value(pbrMRMaterial->attenuationColor[0]));
				attenuationColor.push_back(tinygltf::Value(pbrMRMaterial->attenuationColor[1]));
				attenuationColor.push_back(tinygltf::Value(pbrMRMaterial->attenuationColor[2]));
				obj.insert(std::make_pair("attenuationColor", tinygltf::Value(attenuationColor)));
				obj.insert(std::make_pair("sheenRoughnessFactor", tinygltf::Value(pbrMRMaterial->thicknessFactor)));
				obj.insert(std::make_pair("attenuationDistance", tinygltf::Value(pbrMRMaterial->attenuationDistance)));

				tinygltf::Value::Object thicknessTextureInfo;
				thicknessTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(pbrMRMaterial->thicknessTexture, type)));
				thicknessTextureInfo.insert(std::make_pair("texCoord", 0));
				obj.insert(std::make_pair("thicknessTexture", tinygltf::Value(thicknessTextureInfo)));


				gltfMaterial.extensions["KHR_materials_volume"] = tinygltf::Value(obj);
			}

			if (pbrMRMaterial->enable_KHR_materials_specular) {
				_model.extensionsRequired.push_back("KHR_materials_specular");
				_model.extensionsUsed.push_back("KHR_materials_specular");

				tinygltf::Value::Object obj;

				tinygltf::Value::Array specularColorFactor;
				specularColorFactor.push_back(tinygltf::Value(pbrMRMaterial->specularColorFactor[0]));
				specularColorFactor.push_back(tinygltf::Value(pbrMRMaterial->specularColorFactor[1]));
				specularColorFactor.push_back(tinygltf::Value(pbrMRMaterial->specularColorFactor[2]));
				obj.insert(std::make_pair("specularColorFactor", tinygltf::Value(specularColorFactor)));
				obj.insert(std::make_pair("specularFactor", tinygltf::Value(pbrMRMaterial->specularFactor)));

				tinygltf::Value::Object specularTextureInfo;
				specularTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(pbrMRMaterial->specularTexture, type)));
				specularTextureInfo.insert(std::make_pair("texCoord", 0));
				obj.insert(std::make_pair("specularTexture", tinygltf::Value(specularTextureInfo)));

				tinygltf::Value::Object specularColorTextureInfo;
				specularColorTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(pbrMRMaterial->specularColorTexture, type)));
				specularColorTextureInfo.insert(std::make_pair("texCoord", 0));
				obj.insert(std::make_pair("specularColorTexture", tinygltf::Value(specularColorTextureInfo)));

				gltfMaterial.extensions["KHR_materials_specular"] = tinygltf::Value(obj);
			}

			if (pbrMRMaterial->enable_KHR_materials_transmission) {
				_model.extensionsRequired.push_back("KHR_materials_transmission");
				_model.extensionsUsed.push_back("KHR_materials_transmission");

				tinygltf::Value::Object obj;

				obj.insert(std::make_pair("transmissionFactor", tinygltf::Value(pbrMRMaterial->transmissionFactor)));

				tinygltf::Value::Object transmissionTextureInfo;
				transmissionTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(pbrMRMaterial->transmissionTexture, type)));
				transmissionTextureInfo.insert(std::make_pair("texCoord", 0));
				obj.insert(std::make_pair("transmissionTexture", tinygltf::Value(transmissionTextureInfo)));


				gltfMaterial.extensions["KHR_materials_transmission"] = tinygltf::Value(obj);
			}
#pragma endregion
		}
		else {
			GltfPbrSpecularGlossinessMaterial* pbrSGMaterial = dynamic_cast<GltfPbrSpecularGlossinessMaterial*>(material);
			setGeneralGltfMaterialProperties(gltfMaterial, pbrSGMaterial, type);

			_model.extensionsRequired.push_back("KHR_materials_pbrSpecularGlossiness");
			_model.extensionsUsed.push_back("KHR_materials_pbrSpecularGlossiness");

			tinygltf::Value::Object obj;

			tinygltf::Value::Array specularFactor;
			specularFactor.push_back(tinygltf::Value(pbrSGMaterial->specularFactor[0]));
			specularFactor.push_back(tinygltf::Value(pbrSGMaterial->specularFactor[1]));
			specularFactor.push_back(tinygltf::Value(pbrSGMaterial->specularFactor[2]));
			obj.insert(std::make_pair("specularFactor", tinygltf::Value(specularFactor)));

			tinygltf::Value::Array diffuseFactor;
			diffuseFactor.push_back(tinygltf::Value(pbrSGMaterial->diffuseFactor[0]));
			diffuseFactor.push_back(tinygltf::Value(pbrSGMaterial->diffuseFactor[1]));
			diffuseFactor.push_back(tinygltf::Value(pbrSGMaterial->diffuseFactor[2]));
			diffuseFactor.push_back(tinygltf::Value(pbrSGMaterial->diffuseFactor[3]));
			obj.insert(std::make_pair("diffuseFactor", tinygltf::Value(diffuseFactor)));

			obj.insert(std::make_pair("glossinessFactor", tinygltf::Value(pbrSGMaterial->glossinessFactor)));

			tinygltf::Value::Object diffuseTextureInfo;
			diffuseTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(pbrSGMaterial->diffuseTexture, type)));
			diffuseTextureInfo.insert(std::make_pair("texCoord", 0));
			obj.insert(std::make_pair("diffuseTexture", tinygltf::Value(diffuseTextureInfo)));

			tinygltf::Value::Object specularGlossinessTextureInfo;
			specularGlossinessTextureInfo.insert(std::make_pair("index", getOrCreateGltfTexture(pbrSGMaterial->specularGlossinessTexture, type)));
			specularGlossinessTextureInfo.insert(std::make_pair("texCoord", 0));
			obj.insert(std::make_pair("specularGlossinessTexture", tinygltf::Value(specularGlossinessTextureInfo)));

			gltfMaterial.extensions["KHR_materials_pbrSpecularGlossiness"] = tinygltf::Value(obj);
		}
		json matJson;
		tinygltf::SerializeGltfMaterial(gltfMaterial, matJson);
		for (int i = 0; i < _model.materials.size(); ++i) {
			json existMatJson;
			tinygltf::SerializeGltfMaterial(_model.materials.at(i), existMatJson);
			if (matJson == existMatJson) {
				return i;
			}
		}
		const int materialIndex = _model.materials.size();
		_model.materials.push_back(gltfMaterial);
		return materialIndex;
	}
	int textureCompression(const TextureType& type, const osg::ref_ptr<osg::StateSet>& stateSet, const osg::ref_ptr<osg::Texture>& texture) {
		if (texture) {
			tinygltf::Material mat;
			mat.pbrMetallicRoughness.baseColorTexture.index = getOrCreateGltfTexture(texture, type);
			mat.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
			mat.pbrMetallicRoughness.baseColorFactor = { 1.0,1.0,1.0,1.0 };
			mat.pbrMetallicRoughness.metallicFactor = 0.0;
			mat.pbrMetallicRoughness.roughnessFactor = 1.0;
			mat.doubleSided = ((stateSet->getMode(GL_CULL_FACE) & osg::StateAttribute::ON) == 0);

			if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON) {
				mat.alphaMode = "BLEND";
			}
			json matJson;
			tinygltf::SerializeGltfMaterial(mat, matJson);
			for (int i = 0; i < _model.materials.size(); ++i) {
				json existMatJson;
				tinygltf::SerializeGltfMaterial(_model.materials.at(i), existMatJson);
				if (matJson == existMatJson) {
					return i;
				}
			}
			const int materialIndex = _model.materials.size();
			_model.materials.push_back(mat);
			return materialIndex;
		}
		return -1;
	}
	int textureCompression(const TextureType& type, const osg::ref_ptr<osg::StateSet>& stateSet, const osg::ref_ptr<osg::Material>& material) {
		if (material) {
			tinygltf::Material mat;
			mat.pbrMetallicRoughness.baseColorTexture.index = -1;
			mat.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
			const osg::Vec4 baseColor = material->getDiffuse(osg::Material::FRONT_AND_BACK);
			mat.pbrMetallicRoughness.baseColorFactor = { baseColor.x(),baseColor.y(),baseColor.z(),baseColor.w() };
			mat.pbrMetallicRoughness.metallicFactor = 0.0;
			mat.pbrMetallicRoughness.roughnessFactor = 1.0;
			mat.doubleSided = ((stateSet->getMode(GL_CULL_FACE) & osg::StateAttribute::ON) == 0);

			if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON) {
				mat.alphaMode = "BLEND";
			}
			json matJson;
			tinygltf::SerializeGltfMaterial(mat, matJson);
			for (int i = 0; i < _model.materials.size(); ++i) {
				json existMatJson;
				tinygltf::SerializeGltfMaterial(_model.materials.at(i), existMatJson);
				if (matJson == existMatJson) {
					return i;
				}
			}

			const int materialIndex = _model.materials.size();
			_model.materials.push_back(mat);
			return materialIndex;
		}
		return -1;
	}
	bool geometryCompresstion(const CompressionType& type,const int vertexComporessLevel) {
		VertexCompressionOptions vco;
		switch (vertexComporessLevel)
		{
		case 0:
			vco.PositionQuantizationBits = 16.0;
			vco.TexCoordQuantizationBits = 14.0;
			vco.NormalQuantizationBits = 12.0;
			vco.ColorQuantizationBits = 10.0;
			vco.GenericQuantizationBits = 18.0;
			break;
		case 2:
			vco.PositionQuantizationBits = 12.0;
			vco.TexCoordQuantizationBits = 10.0;
			vco.NormalQuantizationBits = 8.0;
			vco.ColorQuantizationBits = 8.0;
			vco.GenericQuantizationBits = 14.0;
			break;
		default:
			break;
		}
		mergeMeshes();
		//KHR_draco_mesh_compression
		if (type == CompressionType::DRACO) {
			geometryCompression("KHR_draco_mesh_compression", vco);
		}

		//EXT_meshopt_compression
		if (type == CompressionType::MESHOPT) {
			geometryCompression("EXT_meshopt_compression", vco);
		}
		//2
		mergeBuffers();
		return true;
	}

};

#endif // !OSGDB_GLTF_UTILS
