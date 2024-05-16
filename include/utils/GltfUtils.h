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
#include <unordered_set>
#include <osg/Material>
#include <utils/GltfPbrMetallicRoughnessMaterial.h>
#include <utils/GltfPbrSpecularGlossinessMaterial.h>
#include <draco/compression/decode.h>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include <osgDB/ReadFile>
#include <osgDB/ConvertUTF>
#include <meshoptimizer.h>
#include <osg/MatrixTransform>
#include <osgUtil/Optimizer>

class GeometryNodeVisitor :public osg::NodeVisitor {
	bool _recomputeBoundingBox;
public:
	GeometryNodeVisitor(const bool recomputeBoundingBox = true) : NodeVisitor(TRAVERSE_ALL_CHILDREN),
	                                                              _recomputeBoundingBox(recomputeBoundingBox)
	{
	}

	void apply(osg::Drawable& drawable) override {
		if(_recomputeBoundingBox)
			drawable.dirtyBound();
		const osg::ref_ptr<osg::Vec4Array> colors = dynamic_cast<osg::Vec4Array*>(drawable.asGeometry()->getColorArray());
		const osg::MatrixList matrix_list = drawable.getWorldMatrices();
		osg::Matrixd mat;
		for (const osg::Matrixd& matrix : matrix_list) {
			mat = mat * matrix;
		}
		const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(drawable.asGeometry()->getVertexArray());
		if (mat != osg::Matrixd::identity()) {
			for (auto& i : *positions)
			{
				i = i * mat;
			}
		}
	}
	void apply(osg::Group& group) override
	{
		traverse(group);
	}

};

class TransformNodeVisitor :public osg::NodeVisitor {
public:
	TransformNodeVisitor() : NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

	void apply(osg::Group& group) override
	{
		traverse(group);
	}

	void apply(osg::Transform& mtransform) override
	{
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
		std::string result = buf.str();
		return result;
	}

	template<typename T>
	Stringify& operator << (const T& val) { buf << val; return (*this); }

	Stringify& operator << (const Stringify& val) { buf << static_cast<std::string>(val); return (*this); }

protected:
	std::stringstream buf;
};

inline unsigned hashString(const std::string& input)
{
	constexpr unsigned int m = 0x5bd1e995;
	unsigned int len;
	len = input.length();
	const char* data = input.c_str();
	unsigned int h = m ^ len; // using "m" as the seed.

	while (len >= 4)
	{
		constexpr int r = 24;
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
	tinygltf::Model& _model;
	std::vector<osg::ref_ptr<osg::Texture>> _textures;
public:
	struct VertexCompressionOptions {
		int PositionQuantizationBits = 14;
		int TexCoordQuantizationBits = 12;
		int NormalQuantizationBits = 10;
		int ColorQuantizationBits = 8;
		int GenericQuantizationBits = 16;
		int EncodeSpeed = 0;
		int DecodeSpeed = 10;
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
		const auto numComponents = CalculateNumComponents(accessor);
		const auto stride = sizeof(T) * numComponents;

		const auto buffer1 = std::make_unique<draco::DataBuffer>();
		draco::GeometryAttribute atrribute;
		atrribute.Init(GetTypeFromAttributeName(attributeName), &*buffer1, numComponents, GetDataType(accessor),
		               accessor.normalized, stride, 0);
		dracoMesh.set_num_points(static_cast<unsigned int>(accessor.count));
		const int attId = dracoMesh.AddAttribute(atrribute, true, dracoMesh.num_points());
		const auto attrActual = dracoMesh.attribute(attId);

		const auto& bufferView = _model.bufferViews[accessor.bufferView];
		const auto& buffer = _model.buffers[bufferView.buffer];
		std::vector<T> values;

		const void* data_ptr = buffer.data.data() + bufferView.byteOffset;


		switch (accessor.componentType) {
		case TINYGLTF_COMPONENT_TYPE_BYTE:
			values.assign(static_cast<const int8_t*>(data_ptr),
				static_cast<const int8_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			values.assign(static_cast<const uint8_t*>(data_ptr),
				static_cast<const uint8_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:
			values.assign(static_cast<const int16_t*>(data_ptr),
				static_cast<const int16_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			values.assign(static_cast<const uint16_t*>(data_ptr),
				static_cast<const uint16_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			values.assign(static_cast<const uint32_t*>(data_ptr),
				static_cast<const uint32_t*>(data_ptr) + accessor.count * numComponents);
			break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			values.assign(static_cast<const float*>(data_ptr),
				static_cast<const float*>(data_ptr) + accessor.count * numComponents);
			break;
		default:
			std::cerr << "Unknown component type." << '\n';
			break;
		}
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
			std::cerr << "Inconsistent points count." << '\n';
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

		for (auto& j : mesh.primitives)
		{
			const auto& primitive = j;
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
					assert(indices.size() % 3 == 0);
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
					assert(indices.size() % 3 == 0);
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
					assert(indices.size() % 3 == 0);
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
					assert(indices.size() % 3 == 0);
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
					assert(indices.size() % 3 == 0);
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
					assert(indices.size() % 3 == 0);
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
				default:std::cerr << "Unknown component type." << '\n'; break;
				}

				bufferViewsToRemove.emplace(accessor.bufferView);
				attributeAccessor.bufferView = -1;
				attributeAccessor.byteOffset = 0;
				_model.accessors[attribute.second] = attributeAccessor;
				dracoExtensionAttrObj.insert(std::make_pair(attribute.first,
				                                            tinygltf::Value(
					                                            static_cast<int>(dracoMesh.attribute(attId)->
						                                            unique_id()))));

			}

			if (!primitive.targets.empty()) {
				encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
			}

			draco::EncoderBuffer encoderBuffer;
			const draco::Status status = encoder.EncodeMeshToBuffer(dracoMesh, &encoderBuffer);
			if (!status.ok()) {
				std::cerr << "Failed to encode the mesh: " << status.error_msg() << '\n';
			}

			draco::Decoder decoder;
			draco::DecoderBuffer decoderBuffer;
			decoderBuffer.Init(encoderBuffer.data(), encoderBuffer.size());
			auto decodeResult = decoder.DecodeMeshFromBuffer(&decoderBuffer);
			if (!decodeResult.ok()) {
				std::cerr << "Draco failed to decode mesh" << '\n';
				return;
			}
			
			if (primitive.indices != -1) {
				assert(_model.accessors.size() > primitive.indices);
				tinygltf::Accessor encodedIndexAccessor(_model.accessors[primitive.indices]);
				encodedIndexAccessor.count = encoder.num_encoded_faces() * 3;
				_model.accessors[primitive.indices] = encodedIndexAccessor;
			}
			for (const auto& dracoAttribute : dracoExtensionAttrObj) {
				auto accessorId = primitive.attributes.at(dracoAttribute.first);
				assert(_model.accessors.size() > accessorId);

				tinygltf::Accessor encodedAccessor(_model.accessors[accessorId]);
				encodedAccessor.count = encoder.num_encoded_points();
				_model.accessors[accessorId] = encodedAccessor;
			}
			dracoExtensionObj.insert(std::make_pair("attributes", dracoExtensionAttrObj));

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
			dracoExtensionObj.insert(std::make_pair("bufferView", tinygltf::Value(id)));

			tinygltf::Primitive resultPrimitive(primitive);
			resultPrimitive.extensions.insert(std::make_pair("KHR_draco_mesh_compression", tinygltf::Value(dracoExtensionObj)));
			j = resultPrimitive;



		}

	}
	void CompressMeshByMeshopt(const tinygltf::Mesh& mesh, const VertexCompressionOptions& vco) const
	{
		// 1、Indexing
		// 2、Vertex cache optimization
		// 3、Overdraw optimization
		// 4、Vertex fetch optimization
		// 5、Vertex quantization
		// 6、Vertex/index buffer compression

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
				auto getEncodeVertexBuffer = [&accessor](const tinygltf::Accessor& attributeAccessor, const tinygltf::Model& model, int num = 3)->std::vector<unsigned char> {
					const tinygltf::BufferView bufferView = model.bufferViews[attributeAccessor.bufferView];
					const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
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
					case TINYGLTF_COMPONENT_TYPE_BYTE: vbuf.resize(meshopt_encodeVertexBuffer(vbuf.data(), vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(int8_t) * num)); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:vbuf.resize(meshopt_encodeVertexBuffer(vbuf.data(), vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(uint8_t) * num)); break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:vbuf.resize(meshopt_encodeVertexBuffer(vbuf.data(), vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(int16_t) * num)); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:vbuf.resize(meshopt_encodeVertexBuffer(vbuf.data(), vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(uint16_t) * num)); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:vbuf.resize(meshopt_encodeVertexBuffer(vbuf.data(), vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(uint32_t) * num)); break;
					default:vbuf.resize(meshopt_encodeVertexBuffer(vbuf.data(), vbuf.size(), buffer.data.data() + bufferView.byteOffset, attributeAccessor.count, sizeof(float) * num)); break;
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
					meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value(static_cast<int>(vbuf.size()))));
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(int8_t)) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint8_t)) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(int16_t)) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint16_t)) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint32_t)) * 3)));
						break;
					default:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(float)) * 3)));
						break;
					}
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
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(int8_t)) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint8_t)) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(int16_t)) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint16_t)) * 3)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint32_t)) * 3)));
						break;
					default:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(float)) * 3)));
						break;
					}
					meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value(static_cast<int>(attributeAccessor.count))));
					meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value(static_cast<std::string>("ATTRIBUTES"))));
					bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
				}
				else if (attribute.first == "TEXCOORD_0") {
					std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model, 2);
					buffer.data.resize(vbuf.size());
					for (unsigned int i = 0; i < vbuf.size(); ++i)
						buffer.data[i] = vbuf[i];
					meshoptExtensionObj = tinygltf::Value::Object();
					meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
					meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value(static_cast<int>(vbuf.size()))));
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(int8_t)) * 2)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint8_t)) * 2)));
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(int16_t)) * 2)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint16_t)) * 2)));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint32_t)) * 2)));
						break;
					default:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(float)) * 2)));
						break;
					}
					meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value(static_cast<int>(attributeAccessor.count))));
					meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value(static_cast<std::string>("ATTRIBUTES"))));
					bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(meshoptExtensionObj)));
				}
				else if (attribute.first == "_BATCHID") {
					std::vector<unsigned char> vbuf = getEncodeVertexBuffer(attributeAccessor, _model, 1);
					buffer.data.resize(vbuf.size());
					for (unsigned int i = 0; i < vbuf.size(); ++i)
						buffer.data[i] = vbuf[i];
					meshoptExtensionObj = tinygltf::Value::Object();
					meshoptExtensionObj.insert(std::make_pair("buffer", tinygltf::Value((int)bufferView.buffer)));
					meshoptExtensionObj.insert(std::make_pair("byteLength", tinygltf::Value(static_cast<int>(vbuf.size()))));
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(int8_t)))));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint8_t)))));
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(int16_t)))));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint16_t)))));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(uint32_t)))));
						break;
					default:
						meshoptExtensionObj.insert(std::make_pair("byteStride", tinygltf::Value(static_cast<int>(sizeof(float)))));
						break;
					}
					meshoptExtensionObj.insert(std::make_pair("count", tinygltf::Value(static_cast<int>(attributeAccessor.count))));
					meshoptExtensionObj.insert(std::make_pair("mode", tinygltf::Value(static_cast<std::string>("ATTRIBUTES"))));
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
		encoder.SetSpeedOptions(options.EncodeSpeed, options.DecodeSpeed);
		encoder.SetTrackEncodedProperties(true);
	}
	bool geometryCompression(const std::string& extensionName, const VertexCompressionOptions& vco) {
		_model.extensionsRequired.push_back(extensionName);
		_model.extensionsUsed.push_back(extensionName);

		if (extensionName == "KHR_draco_mesh_compression") {
			for (auto& mesh : _model.meshes) {
				std::unordered_set<int> bufferViewsToRemove;
				try
				{

					//std::cout << "start draco:" << std::endl;
					CompressMeshByDraco(mesh, bufferViewsToRemove, vco);
					//std::cout << "end draco" << std::endl;
				}
				catch (const std::exception& e)
				{
					std::cerr << "Exception:" << e.what() << std::endl;
				}
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

	int getOrCreateGltfTexture(osg::ref_ptr<osg::Texture> osgTexture, const TextureType& type) {
		if (!osgTexture.valid()) {
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
		_textures.emplace_back(osgTexture);
		std::string filename;
		std::string mimeType;
		std::string ext = "png";
		std::string ktxVersion = "2.0";
		switch (type)
		{
		case KTX:
			ext = "ktx";
			ktxVersion = "1.0";
			mimeType = "image/ktx";
			break;
		case KTX2:
			ext = "ktx";
			mimeType = "image/ktx2";
			break;
		case PNG:
			ext = "png";
			mimeType = "image/png";
			break;
		case JPG:
			ext = "jpg";
			mimeType = "image/jpeg";
			break;
		case WEBP:
			ext = "webp";
			mimeType = "image/webp";
			break;
		default:
			break;
		}
		// Flip the image before writing
		{
			osg::ref_ptr< osg::Image > flipped = new osg::Image(*osgImage);
			//need to forbid filpVertical when use texture atlas 
			//if (type == TextureType::KTX2 || type == TextureType::KTX) {
			//}
			textureOptimizeSize(flipped);

			// If the image has a filename try to hash it so we only write out one copy of it.  
			if (!osgImage->getFileName().empty())
			{

				std::string data(reinterpret_cast<char const*>(flipped->data()));
				filename = Stringify() << std::hex << hashString(data);
				//std::hash<unsigned char> hasher;
				//std::size_t hashValue = hasher(*flipped->data());
				//filename = std::to_string(hashValue);
				filename += "-w" + std::to_string(flipped->s()) + "-h" + std::to_string(flipped->t());
				const GLenum pixelFormat = flipped->getPixelFormat();
				if (ext == "jpg" && pixelFormat != GL_ALPHA && pixelFormat != GL_RGB) {
					filename += ".png";
				}
				else {
					filename += "." + ext;
				}
				bool isFileExists = osgDB::fileExists("./" + filename);
				if (!isFileExists)
				{
					if (flipped->getOrigin() == osg::Image::BOTTOM_LEFT)
						flipped->flipVertical();
					if (flipped->getOrigin() == osg::Image::BOTTOM_LEFT)
					{
						flipped->setOrigin(osg::Image::TOP_LEFT);
					}
					if (type == KTX) {
						osg::ref_ptr<osgDB::Options> option = new osgDB::Options;
						option->setOptionString("Version=" + ktxVersion);
						if (!(writeImageFile(*flipped, filename, option.get()))) {
							notify(osg::FATAL) << '\n';
						}
					}
					else {
						if (!(osgDB::writeImageFile(*flipped, filename))) {
							mimeType = "image/png";
							filename = Stringify() << std::hex << hashString(data);
							filename += "-w" + std::to_string(flipped->s()) + "-h" + std::to_string(flipped->t());
							filename += ".png";
							std::ifstream fileExistsPng(filename);
							if ((!fileExistsPng.good()) || (fileExistsPng.peek() == std::ifstream::traits_type::eof()))
							{
								if (!(osgDB::writeImageFile(*flipped, filename))) {
									notify(osg::FATAL) << '\n';
								}
							}
							fileExistsPng.close();
						}
					}
				}
			}
			else
			{
				// Otherwise just find a filename that doesn't exist
				int fileNameInc = 0;
				do
				{
					std::stringstream ss;
					ss << fileNameInc << "." << ext;
					filename = ss.str();
					fileNameInc++;
				} while (osgDB::fileExists("./" + filename));
				if (type == KTX) {
					osg::ref_ptr<osgDB::Options> option = new osgDB::Options;
					option->setOptionString("Version=" + ktxVersion);
					if (!(writeImageFile(*flipped, filename, option.get()))) {
						notify(osg::FATAL) << '\n';
					}
				}
				else {
					if (!(osgDB::writeImageFile(*flipped, filename))) {
						notify(osg::FATAL) << '\n';
					}
				}
			}
		}

		tinygltf::Image image;
		//image.name = osgDB::convertStringFromCurrentCodePageToUTF8(osgDB::getSimpleFileName(osgImage->getFileName()));if defined image by buffer,name must be not defined;

		tinygltf::BufferView gltfBufferView;
		int bufferViewIndex = -1;
		bufferViewIndex = static_cast<int>(_model.bufferViews.size());
		gltfBufferView.buffer = _model.buffers.size();
		gltfBufferView.byteOffset = 0;
		{
			std::ifstream file("./" + filename, std::ios::binary);
			std::vector<unsigned char> imageData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			gltfBufferView.byteLength = static_cast<int>(imageData.size());
			tinygltf::Buffer gltfBuffer;
			gltfBuffer.data = imageData;
			_model.buffers.push_back(gltfBuffer);
			file.close();
		}
		gltfBufferView.target = TINYGLTF_TEXTURE_TARGET_TEXTURE2D;
		gltfBufferView.name = filename;
		_model.bufferViews.push_back(gltfBufferView);

		image.mimeType = mimeType;
		image.bufferView = bufferViewIndex; //_model.bufferViews.size() the only bufferView in this model
		_model.images.push_back(image);
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
		if (type == KTX2)
		{
			_model.extensionsRequired.emplace_back("KHR_texture_basisu");
			_model.extensionsUsed.emplace_back("KHR_texture_basisu");
			texture.source = -1;
			tinygltf::Value::Object ktxExtensionObj;
			ktxExtensionObj.insert(std::make_pair("source", index));
			texture.extensions.insert(std::make_pair("KHR_texture_basisu", tinygltf::Value(ktxExtensionObj)));
		}
		else if (type == WEBP) {
			_model.extensionsRequired.emplace_back("EXT_texture_webp");
			_model.extensionsUsed.emplace_back("EXT_texture_webp");
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
		const osg::ref_ptr<osg::Texture2D> normalTexture = material->normalTexture;
		if (normalTexture) {
			gltfMaterial.normalTexture.index = getOrCreateGltfTexture(normalTexture, type);
			gltfMaterial.normalTexture.texCoord = 0;
		}
		const osg::ref_ptr<osg::Texture2D> occlusionTexture = material->occlusionTexture;
		if (occlusionTexture) {
			gltfMaterial.occlusionTexture.index = getOrCreateGltfTexture(occlusionTexture, type);
			gltfMaterial.occlusionTexture.texCoord = 0;
		}
		const osg::ref_ptr<osg::Texture2D> emissiveTexture = material->emissiveTexture;
		if (emissiveTexture) {
			gltfMaterial.emissiveTexture.index = getOrCreateGltfTexture(emissiveTexture, type);
			gltfMaterial.emissiveTexture.texCoord = 0;
		}
		gltfMaterial.emissiveFactor.push_back(material->emissiveFactor[0]);
		gltfMaterial.emissiveFactor.push_back(material->emissiveFactor[1]);
		gltfMaterial.emissiveFactor.push_back(material->emissiveFactor[2]);

		if (material->enable_KHR_materials_clearcoat) {
			_model.extensionsRequired.emplace_back("KHR_materials_clearcoat");
			_model.extensionsUsed.emplace_back("KHR_materials_clearcoat");

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
			_model.extensionsRequired.emplace_back("KHR_materials_anisotropy");
			_model.extensionsUsed.emplace_back("KHR_materials_anisotropy");

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
			_model.extensionsRequired.emplace_back("KHR_materials_emissive_strength");
			_model.extensionsUsed.emplace_back("KHR_materials_emissive_strength");

			tinygltf::Value::Object obj;
			obj.insert(std::make_pair("emissiveStrength", tinygltf::Value(material->emissiveStrength)));
			gltfMaterial.extensions["KHR_materials_emissive_strength"] = tinygltf::Value(obj);
		}
	}

	//convert image size to the power of 2
	void textureOptimizeSize(const osg::ref_ptr<osg::Image>& img) {
		auto findNearestPowerOfTwo = [](int n) {
			int powerOfTwo = 1;
			while (powerOfTwo * 2 <= n) {
				powerOfTwo *= 2;
			}
			return powerOfTwo;
			};

		const int oldS = img->s();
		const int oldR = img->t();
		const int newS = findNearestPowerOfTwo(oldS);
		const int newR = findNearestPowerOfTwo(oldR);
		img->scaleImage(newS, newR, 1);
	}
	void mergeBuffers() const
	{
		tinygltf::Buffer totalBuffer;
		tinygltf::Buffer fallbackBuffer;
		fallbackBuffer.name = "fallback";
		int byteOffset = 0, fallbackByteOffset = 0;
		for (auto& bufferView : _model.bufferViews) {
			const auto& meshoptExtension = bufferView.extensions.find("EXT_meshopt_compression");
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
				const int needAdd = 4 - byteOffset % 4;
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
				const int fallbackNeedAdd = 4 - fallbackByteOffset % 4;
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
				const int needAdd = 4 - byteOffset % 4;
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
	void mergeMeshes() const
	{
		std::map<int, std::vector<tinygltf::Primitive>> materialPrimitiveMap;
		for (auto& mesh : _model.meshes) {
			if (!mesh.primitives.empty()) {
				const auto& item = materialPrimitiveMap.find(mesh.primitives[0].material);
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
				const auto& normalAttr = prim.attributes.find("NORMAL");
				if (normalAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldNormalsAccessor = _model.accessors[normalAttr->second];
					totalNormalsAccessor = oldNormalsAccessor;
				}
				const auto& texAttr = prim.attributes.find("TEXCOORD_0");
				if (texAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldTexcoordsAccessor = _model.accessors[texAttr->second];
					totalTexcoordsAccessor = oldTexcoordsAccessor;
				}
				const auto& batchidAttr = prim.attributes.find("_BATCHID");
				if (batchidAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldBatchIdsAccessor = _model.accessors[batchidAttr->second];
					totalBatchIdsAccessor = oldBatchIdsAccessor;
				}
				const auto& colorAttr = prim.attributes.find("COLOR_0");
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

			auto reindexBufferAndBvAndAccessor = [&](const tinygltf::Accessor& accessor,tinygltf::BufferView& tBv,tinygltf::Buffer& tBuffer,tinygltf::Accessor& newAccessor,unsigned int sum=0,bool isIndices=false) {
				tinygltf::BufferView& bv = _model.bufferViews[accessor.bufferView];
				tinygltf::Buffer& buffer = _model.buffers[bv.buffer];

				tBv.byteStride = bv.byteStride;
				tBv.target = bv.target;
				if(accessor.componentType!=newAccessor.componentType){
					if (newAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT && accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
						unsigned short* oldIndicesChar = (unsigned short*)buffer.data.data();
						std::vector<unsigned int> indices;
						for (size_t i = 0; i < accessor.count; ++i) {
							unsigned short ushortVal = *oldIndicesChar++;
							unsigned int uintVal = (unsigned int)(ushortVal + sum);
							indices.push_back(uintVal);
						}		
						unsigned char* indicesUChar = (unsigned char*)indices.data();
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
							unsigned int* oldIndicesChar = (unsigned int*)buffer.data.data();
							std::vector<unsigned int> indices;
							for (unsigned int i = 0; i < accessor.count; ++i) {
								unsigned int oldUintVal = *oldIndicesChar++;
								unsigned int uintVal = oldUintVal + sum;
								indices.push_back(uintVal);
							}
							unsigned char* indicesUChar = (unsigned char*)indices.data();
							const unsigned int size = bv.byteLength;
							for (unsigned int i = 0; i < size; ++i) {
								tBuffer.data.push_back(*indicesUChar++);
							}
						}
						else {
							unsigned short* oldIndicesChar = (unsigned short*)buffer.data.data();
							std::vector<unsigned short> indices;
							for (size_t i = 0; i < accessor.count; ++i) {
								unsigned short oldUshortVal = *oldIndicesChar++;
								unsigned short ushortVal = oldUshortVal + sum;
								indices.push_back(ushortVal);
							}
							unsigned char* indicesUChar = (unsigned char*)indices.data();
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

				if (!accessor.minValues.empty()) {
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

				const auto& normalAttr = prim.attributes.find("NORMAL");
				if (normalAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldNormalsAccessor = _model.accessors[normalAttr->second];
					reindexBufferAndBvAndAccessor(oldNormalsAccessor, totalNormalsBufferView, totalNormalsBuffer, totalNormalsAccessor);
				}
				const auto& texAttr = prim.attributes.find("TEXCOORD_0");
				if (texAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldTexcoordsAccessor = _model.accessors[texAttr->second];
					reindexBufferAndBvAndAccessor(oldTexcoordsAccessor, totalTexcoordsBufferView, totalTexcoordsBuffer, totalTexcoordsAccessor);
				}
				const auto& batchidAttr = prim.attributes.find("_BATCHID");
				if (batchidAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldBatchIdsAccessor = _model.accessors[batchidAttr->second];
					reindexBufferAndBvAndAccessor(oldBatchIdsAccessor, totalBatchIdsBufferView, totalBatchIdsBuffer, totalBatchIdsAccessor);
				}
				const auto& colorAttr = prim.attributes.find("COLOR_0");
				if (colorAttr != prim.attributes.end()) {
					tinygltf::Accessor& oldColorsAccessor = _model.accessors[colorAttr->second];
					reindexBufferAndBvAndAccessor(oldColorsAccessor, totalColorsBufferView, totalColorsBuffer, totalColorsAccessor);
				}
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
	void mergePrimitives(const osg::ref_ptr<osg::Geometry>& geom) {
		osgUtil::Optimizer optimizer;
		optimizer.optimize(geom, osgUtil::Optimizer::INDEX_MESH);
		osg::ref_ptr<osg::PrimitiveSet> mergePrimitiveset = nullptr;
		const unsigned int numPrimitiveSets = geom->getNumPrimitiveSets();
		for (unsigned i = 0; i < numPrimitiveSets; ++i) {
			osg::PrimitiveSet* pset = geom->getPrimitiveSet(i);;
			const osg::PrimitiveSet::Type type = pset->getType();
			switch (type)
			{
			case osg::PrimitiveSet::PrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArraysPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArrayLengthsPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUBytePrimitiveType:
				if (mergePrimitiveset == nullptr) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					const auto primitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(pset);
					const osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					if (mergeType == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
						const auto mergePrimitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(mergePrimitiveset.get());
						mergePrimitiveUByte->insert(mergePrimitiveUByte->end(), primitiveUByte->begin(), primitiveUByte->end());
					}
					else {
						const auto mergePrimitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(mergePrimitiveset.get());
						if (mergeType == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {

							for (unsigned int k = 0; k < primitiveUByte->getNumIndices(); ++k) {
								unsigned short index = primitiveUByte->at(k);
								mergePrimitiveUShort->push_back(index);
							}
						}
						else if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
							const auto mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
							for (unsigned int k = 0; k < primitiveUByte->getNumIndices(); ++k) {
								unsigned int index = primitiveUByte->at(k);
								mergePrimitiveUInt->push_back(index);
							}
						}
					}
				}

				break;
			case osg::PrimitiveSet::DrawElementsUShortPrimitiveType:
				if (mergePrimitiveset == nullptr) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					osg::ref_ptr<osg::DrawElementsUShort> primitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(pset);
					const osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					const osg::ref_ptr<osg::DrawElementsUShort> mergePrimitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(mergePrimitiveset.get());
					if (mergeType == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {
						mergePrimitiveUShort->insert(mergePrimitiveUShort->end(), primitiveUShort->begin(), primitiveUShort->end());
						primitiveUShort.release();
					}
					else {
						if (mergeType == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
							const auto mergePrimitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(mergePrimitiveset.get());
							const auto newMergePrimitvieUShort = new osg::DrawElementsUShort;
							for (unsigned int k = 0; k < mergePrimitiveUByte->getNumIndices(); ++k) {
								unsigned short index = mergePrimitiveUByte->at(k);
								newMergePrimitvieUShort->push_back(index);
							}
							newMergePrimitvieUShort->insert(newMergePrimitvieUShort->end(), primitiveUShort->begin(), primitiveUShort->end());
							primitiveUShort.release();
							mergePrimitiveset.release();
							mergePrimitiveset = newMergePrimitvieUShort;
						}
						else if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
							const auto mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
							for (unsigned int k = 0; k < primitiveUShort->getNumIndices(); ++k) {
								unsigned int index = primitiveUShort->at(k);
								mergePrimitiveUInt->push_back(index);
							}
							primitiveUShort.release();
						}
					}

				}
				break;
			case osg::PrimitiveSet::DrawElementsUIntPrimitiveType:
				if (mergePrimitiveset == nullptr) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					osg::ref_ptr<osg::DrawElementsUInt> primitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(pset);
					const osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
						const auto mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
						mergePrimitiveUInt->insert(mergePrimitiveUInt->end(), primitiveUInt->begin(), primitiveUInt->end());
						primitiveUInt.release();
					}
					else {
						const auto mergePrimitive = dynamic_cast<osg::DrawElements*>(mergePrimitiveset.get());
						const auto newMergePrimitvieUInt = new osg::DrawElementsUInt;
						for (unsigned int k = 0; k < mergePrimitive->getNumIndices(); ++k) {
							unsigned int index = mergePrimitive->getElement(k);
							newMergePrimitvieUInt->push_back(index);
						}
						newMergePrimitvieUInt->insert(newMergePrimitvieUInt->end(), primitiveUInt->begin(), primitiveUInt->end());
						primitiveUInt.release();
						mergePrimitiveset.release();
						mergePrimitiveset = newMergePrimitvieUInt;
					}
				}
				break;
			case osg::PrimitiveSet::MultiDrawArraysPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArraysIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUByteIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUShortIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUIntIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawArraysIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUByteIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUShortIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUIntIndirectPrimitiveType:
				break;
			default:
				break;
			}
		}
		if (mergePrimitiveset != nullptr) {
			for (unsigned i = 0; i < numPrimitiveSets; ++i) {
				geom->removePrimitiveSet(0);
			}
			geom->addPrimitiveSet(mergePrimitiveset);
		}
	}

	void reindexMesh(const osg::ref_ptr<osg::Geometry>& geom) {
		//reindexmesh
		const unsigned int num = geom->getNumPrimitiveSets();
		if (num > 0) {
			osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
			osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
			osg::ref_ptr<osg::Vec2Array> texCoords = nullptr;
			if(geom->getNumTexCoordArrays())
				texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
			osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geom->getVertexAttribArray(0));
			for (unsigned int kk = 0; kk < num; ++kk) {
				osg::ref_ptr<osg::DrawElements> drawElements = dynamic_cast<osg::DrawElements*>(geom->getPrimitiveSet(kk));
				if (drawElements.valid() && positions.valid()) {
					const unsigned int numIndices = drawElements->getNumIndices();
					std::vector<meshopt_Stream> streams;
					struct Attr
					{
						float f[4];
					};
					const size_t count = positions->size();
					std::vector<Attr> vertexData, normalData, texCoordData, batchIdData;
					for (size_t i = 0; i < count; ++i)
					{
						const osg::Vec3& vertex = positions->at(i);
						Attr v;
						v.f[0] = vertex.x();
						v.f[1] = vertex.y();
						v.f[2] = vertex.z();
						v.f[3] = 0.0;

						vertexData.push_back(v);

						if (normals.valid()) {
							const osg::Vec3& normal = normals->at(i);
							Attr n;
							n.f[0] = normal.x();
							n.f[1] = normal.y();
							n.f[2] = normal.z();
							n.f[3] = 0.0;
							normalData.push_back(n);
						}
						if (texCoords.valid()) {
							const osg::Vec2& texCoord = texCoords->at(i);
							Attr t;
							t.f[0] = texCoord.x();
							t.f[1] = texCoord.y();
							t.f[2] = 0.0;
							t.f[3] = 0.0;
							texCoordData.push_back(t);
						}
						if(batchIds.valid())
						{
							const float batchId = batchIds->at(i);
							Attr b;
							b.f[0] = batchId;
							batchIdData.push_back(b);
						}
					}
					meshopt_Stream vertexStream = { vertexData.data(), sizeof(Attr), sizeof(Attr) };
					streams.push_back(vertexStream);
					if (normals.valid()) {
						meshopt_Stream normalStream = { normalData.data(), sizeof(Attr), sizeof(Attr) };
						streams.push_back(normalStream);
					}
					if (texCoords.valid()) {
						meshopt_Stream texCoordStream = { texCoordData.data(), sizeof(Attr), sizeof(Attr) };
						streams.push_back(texCoordStream);
					}
					if(positions->size()!=normals->size()||positions->size()!=texCoords->size())
					{
						continue;
					}
					osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(geom->getPrimitiveSet(kk));
					if (drawElementsUShort.valid()) {
						osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
						for (unsigned int i = 0; i < numIndices; ++i)
						{
							indices->push_back(drawElementsUShort->at(i));
						}
						std::vector<unsigned int> remap(positions->size());
						size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(remap.data(), &(*indices)[0], indices->size(), positions->size(),
							streams.data(), streams.size());

						//size_t uniqueVertexCount = meshopt_generateVertexRemap(&remap[0], &(*indices)[0], indices->size(), &(*positions)[0].x(), positions->size(), sizeof(osg::Vec3));
						osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(uniqueVertexCount);
						osg::ref_ptr<osg::FloatArray> optimizedBatchIds = new osg::FloatArray(uniqueVertexCount);
						osg::ref_ptr<osg::UShortArray> optimizedIndices = new osg::UShortArray(indices->size());
						meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), remap.data());
						meshopt_remapVertexBuffer(vertexData.data(), vertexData.data(), positions->size(), sizeof(Attr),
							remap.data());
						vertexData.resize(uniqueVertexCount);
						if (normals.valid()) {
							meshopt_remapVertexBuffer(normalData.data(), normalData.data(), normals->size(), sizeof(Attr),
								remap.data());
							normalData.resize(uniqueVertexCount);
						}
						if (texCoords.valid()) {
							meshopt_remapVertexBuffer(texCoordData.data(), texCoordData.data(), texCoords->size(), sizeof(Attr),
								remap.data());
							texCoordData.resize(uniqueVertexCount);
						}
						if (batchIds.valid())
						{
							meshopt_remapVertexBuffer(batchIdData.data(), batchIdData.data(), batchIds->size(), sizeof(Attr),
								remap.data());
							batchIdData.resize(uniqueVertexCount);
						}

						for (size_t i = 0; i < uniqueVertexCount; ++i) {
							optimizedVertices->at(i) = osg::Vec3(vertexData[i].f[0], vertexData[i].f[1], vertexData[i].f[2]);
							if (normals.valid())
							{
								osg::Vec3 n(normalData[i].f[0], normalData[i].f[1], normalData[i].f[2]);
								n.normalize();
								optimizedNormals->at(i) = n;
							}
							if (texCoords.valid())
								optimizedTexCoords->at(i) = osg::Vec2(texCoordData[i].f[0], texCoordData[i].f[1]);
							if(batchIds.valid())
							{
								optimizedBatchIds->at(i) = batchIdData[i].f[0];
							}
						}
						geom->setVertexArray(optimizedVertices);
						if (normals.valid())
							geom->setNormalArray(optimizedNormals);
						if (texCoords.valid())
							geom->setTexCoordArray(0, optimizedTexCoords);
						if (batchIds.valid())
							geom->setVertexAttribArray(0, optimizedBatchIds);
#pragma region filterTriangles
						size_t newNumIndices = 0;

						for (size_t i = 0; i < numIndices; i += 3) {
							unsigned short a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);

							if (a != b && a != c && b != c)
							{
								optimizedIndices->at(newNumIndices) = a;
								optimizedIndices->at(newNumIndices + 1) = b;
								optimizedIndices->at(newNumIndices + 2) = c;
								newNumIndices += 3;
							}
						}
						optimizedIndices->resize(newNumIndices);
#pragma endregion

						geom->setPrimitiveSet(kk, new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));

					}
					else {
						osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(geom->getPrimitiveSet(kk));
						osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
						for (unsigned int i = 0; i < numIndices; ++i)
						{
							indices->push_back(drawElementsUInt->at(i));
						}
						std::vector<unsigned int> remap(positions->size());
						size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(remap.data(), &(*indices)[0], indices->size(), positions->size(),
							streams.data(), streams.size());

						//size_t uniqueVertexCount = meshopt_generateVertexRemap(&remap[0], &(*indices)[0], indices->size(), &(*positions)[0].x(), positions->size(), sizeof(osg::Vec3));
						osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(uniqueVertexCount);
						osg::ref_ptr<osg::FloatArray> optimizedBatchIds = new osg::FloatArray(uniqueVertexCount);
						osg::ref_ptr<osg::UIntArray> optimizedIndices = new osg::UIntArray(indices->size());
						meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), remap.data());
						meshopt_remapVertexBuffer(vertexData.data(), vertexData.data(), positions->size(), sizeof(Attr),
							remap.data());
						vertexData.resize(uniqueVertexCount);
						if (normals.valid()) {
							meshopt_remapVertexBuffer(normalData.data(), normalData.data(), normals->size(), sizeof(Attr),
								remap.data());
							normalData.resize(uniqueVertexCount);
						}
						if (texCoords.valid()) {
							meshopt_remapVertexBuffer(texCoordData.data(), texCoordData.data(), texCoords->size(), sizeof(Attr),
								remap.data());
							texCoordData.resize(uniqueVertexCount);
						}
						if (batchIds.valid())
						{
							meshopt_remapVertexBuffer(batchIdData.data(), batchIdData.data(), batchIds->size(), sizeof(Attr),
								remap.data());
							batchIdData.resize(uniqueVertexCount);
						}
						for (size_t i = 0; i < uniqueVertexCount; ++i) {
							optimizedVertices->at(i) = osg::Vec3(vertexData[i].f[0], vertexData[i].f[1], vertexData[i].f[2]);
							if (normals.valid()) {
								osg::Vec3 n(normalData[i].f[0], normalData[i].f[1], normalData[i].f[2]);
								n.normalize();
								optimizedNormals->at(i) = osg::Vec3(normalData[i].f[0], normalData[i].f[1], normalData[i].f[2]);
							}
							if (texCoords.valid())
								optimizedTexCoords->at(i) = osg::Vec2(texCoordData[i].f[0], texCoordData[i].f[1]);
							if (batchIds.valid())
							{
								optimizedBatchIds->at(i) = batchIdData[i].f[0];
							}
						}
						geom->setVertexArray(optimizedVertices);
						if (normals.valid())
							geom->setNormalArray(optimizedNormals);
						if (texCoords.valid())
							geom->setTexCoordArray(0, optimizedTexCoords);
						if (batchIds.valid())
							geom->setVertexAttribArray(0, optimizedBatchIds);
#pragma region filterTriangles
						size_t newNumIndices = 0;
						for (size_t i = 0; i < numIndices; i += 3) {
							unsigned int a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);

							if (a != b && a != c && b != c)
							{
								optimizedIndices->at(newNumIndices) = a;
								optimizedIndices->at(newNumIndices + 1) = b;
								optimizedIndices->at(newNumIndices + 2) = c;
								newNumIndices += 3;
							}
						}
						optimizedIndices->resize(newNumIndices);
#pragma endregion
						geom->setPrimitiveSet(kk, new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));
					}

				}
			}
		}
	}

	void simplifyMesh(const osg::ref_ptr<osg::Geometry>& geom,const double ratio,const bool aggressive,const bool lockBorders)
	{
		const float target_error = 1e-2f;
		const float target_error_aggressive = 1e-1f;
		const unsigned int options = lockBorders ? meshopt_SimplifyLockBorder : 0;
		osgUtil::Optimizer optimizer;
		optimizer.optimize(geom, osgUtil::Optimizer::INDEX_MESH);
		const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
		const unsigned int num = geom->getNumPrimitiveSets();
		if (num > 0) {
			for (unsigned int kk = 0; kk < geom->getNumPrimitiveSets(); ++kk) {
				osg::ref_ptr<osg::DrawElements> drawElements = dynamic_cast<osg::DrawElements*>(geom->getPrimitiveSet(kk));
				if (drawElements.valid() && positions.valid()) {
					const unsigned int numIndices = drawElements->getNumIndices();
					const unsigned int count = positions->size();
					std::vector<float> vertices(count * 3);
					for (size_t i = 0; i < count; ++i)
					{
						const osg::Vec3& vertex = positions->at(i);
						vertices[i * 3] = vertex.x();
						vertices[i * 3 + 1] = vertex.y();
						vertices[i * 3 + 2] = vertex.z();

					}

					osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(geom->getPrimitiveSet(kk));
					if (drawElementsUShort.valid()) {
						osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
						for (unsigned int i = 0; i < numIndices; ++i)
						{
							indices->push_back(drawElementsUShort->at(i));
						}
						osg::ref_ptr<osg::UShortArray> destination = new osg::UShortArray;
						destination->resize(numIndices);
						const unsigned int targetIndexCount = numIndices * ratio;
						size_t newLength = meshopt_simplify(&(*destination)[0], &(*indices)[0], static_cast<size_t>(numIndices), vertices.data(), static_cast<size_t>(count), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error, options);
						if (aggressive && newLength > targetIndexCount)
							newLength = meshopt_simplifySloppy(&(*destination)[0], &(*indices)[0], static_cast<size_t>(numIndices), vertices.data(), static_cast<size_t>(count), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error_aggressive);

						if (newLength > 0) {
							osg::ref_ptr<osg::UShortArray> newIndices = new osg::UShortArray;

							for (size_t i = 0; i < static_cast<size_t>(newLength); ++i)
							{
								newIndices->push_back(destination->at(i));
							}
							geom->setPrimitiveSet(kk, new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, newIndices->size(), &(*newIndices)[0]));
						}
						else
						{
							geom->removePrimitiveSet(kk);
							kk--;
						}
					}
					else {
						const osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(geom->getPrimitiveSet(kk));
						osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
						for (unsigned int i = 0; i < numIndices; ++i)
						{
							indices->push_back(drawElementsUInt->at(i));
						}
						osg::ref_ptr<osg::UIntArray> destination = new osg::UIntArray;
						destination->resize(numIndices);
						const unsigned int targetIndexCount = numIndices * ratio;
						size_t newLength = meshopt_simplify(&(*destination)[0], &(*indices)[0], static_cast<size_t>(numIndices), vertices.data(), static_cast<size_t>(count), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error, options);
						if (aggressive && newLength > targetIndexCount)
							newLength = meshopt_simplifySloppy(&(*destination)[0], &(*indices)[0], static_cast<size_t>(numIndices), vertices.data(), static_cast<size_t>(count), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error_aggressive);

						osg::ref_ptr<osg::UIntArray> newIndices = new osg::UIntArray;
						if (newLength > 0) {
							for (size_t i = 0; i < static_cast<size_t>(newLength); ++i)
							{
								newIndices->push_back(destination->at(i));
							}
							geom->setPrimitiveSet(kk, new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, newIndices->size(), &(*newIndices)[0]));
						}
						else
						{
							geom->removePrimitiveSet(kk);
							kk--;
						}
					}

				}
			}
		}
	}

	GltfUtils(tinygltf::Model& model) :_model(model) {

	}
	int textureCompression(const TextureType& type, const osg::ref_ptr<osg::StateSet>& stateSet) {
		tinygltf::Material gltfMaterial;
		gltfMaterial.doubleSided = ((stateSet->getMode(GL_CULL_FACE) & osg::StateAttribute::ON) == 0);
		if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON) {
			gltfMaterial.alphaMode = "BLEND";
		}

		osg::Material* material = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
		auto pbrMRMaterial = dynamic_cast<GltfPbrMetallicRoughnessMaterial*>(material);
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
				_model.extensionsRequired.emplace_back("KHR_materials_ior");
				_model.extensionsUsed.emplace_back("KHR_materials_ior");

				tinygltf::Value::Object obj;
				obj.insert(std::make_pair("ior", tinygltf::Value(pbrMRMaterial->ior)));
				gltfMaterial.extensions["KHR_materials_ior"] = tinygltf::Value(obj);
			}

			if (pbrMRMaterial->enable_KHR_materials_sheen) {
				_model.extensionsRequired.emplace_back("KHR_materials_sheen");
				_model.extensionsUsed.emplace_back("KHR_materials_sheen");

				tinygltf::Value::Object obj;

				tinygltf::Value::Array sheenColorFactor;
				sheenColorFactor.emplace_back(pbrMRMaterial->sheenColorFactor[0]);
				sheenColorFactor.emplace_back(pbrMRMaterial->sheenColorFactor[1]);
				sheenColorFactor.emplace_back(pbrMRMaterial->sheenColorFactor[2]);
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
				_model.extensionsRequired.emplace_back("KHR_materials_volume");
				_model.extensionsUsed.emplace_back("KHR_materials_volume");

				tinygltf::Value::Object obj;

				tinygltf::Value::Array attenuationColor;
				attenuationColor.emplace_back(pbrMRMaterial->attenuationColor[0]);
				attenuationColor.emplace_back(pbrMRMaterial->attenuationColor[1]);
				attenuationColor.emplace_back(pbrMRMaterial->attenuationColor[2]);
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
				_model.extensionsRequired.emplace_back("KHR_materials_specular");
				_model.extensionsUsed.emplace_back("KHR_materials_specular");

				tinygltf::Value::Object obj;

				tinygltf::Value::Array specularColorFactor;
				specularColorFactor.emplace_back(pbrMRMaterial->specularColorFactor[0]);
				specularColorFactor.emplace_back(pbrMRMaterial->specularColorFactor[1]);
				specularColorFactor.emplace_back(pbrMRMaterial->specularColorFactor[2]);
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
				_model.extensionsRequired.emplace_back("KHR_materials_transmission");
				_model.extensionsUsed.emplace_back("KHR_materials_transmission");

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

			_model.extensionsRequired.emplace_back("KHR_materials_pbrSpecularGlossiness");
			_model.extensionsUsed.emplace_back("KHR_materials_pbrSpecularGlossiness");

			tinygltf::Value::Object obj;

			tinygltf::Value::Array specularFactor;
			specularFactor.emplace_back(pbrSGMaterial->specularFactor[0]);
			specularFactor.emplace_back(pbrSGMaterial->specularFactor[1]);
			specularFactor.emplace_back(pbrSGMaterial->specularFactor[2]);
			obj.insert(std::make_pair("specularFactor", tinygltf::Value(specularFactor)));

			tinygltf::Value::Array diffuseFactor;
			diffuseFactor.emplace_back(pbrSGMaterial->diffuseFactor[0]);
			diffuseFactor.emplace_back(pbrSGMaterial->diffuseFactor[1]);
			diffuseFactor.emplace_back(pbrSGMaterial->diffuseFactor[2]);
			diffuseFactor.emplace_back(pbrSGMaterial->diffuseFactor[3]);
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
		SerializeGltfMaterial(gltfMaterial, matJson);
		for (int i = 0; i < _model.materials.size(); ++i) {
			json existMatJson;
			SerializeGltfMaterial(_model.materials.at(i), existMatJson);
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
			SerializeGltfMaterial(mat, matJson);
			for (int i = 0; i < _model.materials.size(); ++i) {
				json existMatJson;
				SerializeGltfMaterial(_model.materials.at(i), existMatJson);
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
	int textureCompression(const TextureType& type, const osg::ref_ptr<osg::StateSet>& stateSet, const osg::ref_ptr<osg::Material>& material) const
	{
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
			SerializeGltfMaterial(mat, matJson);
			for (int i = 0; i < _model.materials.size(); ++i) {
				json existMatJson;
				SerializeGltfMaterial(_model.materials.at(i), existMatJson);
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
		if (type == NONE) {
		mergeMeshes();
		}
		//KHR_draco_mesh_compression
		if (type == DRACO) {
			geometryCompression("KHR_draco_mesh_compression", vco);
		}

		//EXT_meshopt_compression
		if (type == MESHOPT) {
			geometryCompression("EXT_meshopt_compression", vco);
		} 
		//2
		mergeBuffers();
		return true;
	}

};

#endif // !OSGDB_GLTF_UTILS
