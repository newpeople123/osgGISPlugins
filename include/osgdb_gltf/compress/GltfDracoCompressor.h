#ifndef OSG_GIS_PLUGINS_GLTF_DRACO_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_DRACO_COMPRESSOR_H 1
#include "osgdb_gltf/compress/GltfCompressor.h"
#include <unordered_set>
#include <draco/compression/encode.h>
class GltfDracoCompressor :public GltfCompressor
{
public:
	struct DracoCompressionOptions {
		//Default value is medium level
		int PositionQuantizationBits = 14;
		int TexCoordQuantizationBits = 12;
		int NormalQuantizationBits = 10;
		int ColorQuantizationBits = 8;
		int GenericQuantizationBits = 16;
		int EncodeSpeed = 0;
		int DecodeSpeed = 10;
	};
private:
	draco::GeometryAttribute::Type getTypeFromAttributeName(const std::string& name);

	draco::DataType getDataType(const int componentType);

	template<typename T>
	int initPointAttribute(draco::Mesh& dracoMesh, const std::string& attributeName, const std::vector<T>& bufferData, const tinygltf::Accessor& accessor);

	template<typename T>
	int initPointAttributeFromGltf(draco::Mesh& dracoMesh, const std::string& attributeName, const tinygltf::Accessor& accessor);

	void compressMesh(tinygltf::Mesh& mesh, DracoCompressionOptions dco);

	void setDracoEncoderOptions(draco::Encoder& encoder, const DracoCompressionOptions& options);

	template<typename T>
	void initDracoMeshFaces(T& indices, draco::Mesh& dracoMesh);

	template<typename T>
	void initDracoMeshFromGltf(draco::Mesh& dracoMesh, const tinygltf::Accessor indicesAccessor, const tinygltf::BufferView indicesBv, tinygltf::Buffer indicesBuffer);

	void removeBufferViews(const std::unordered_set<int>& bufferViewsToRemove);
public:

	GltfDracoCompressor(tinygltf::Model& model) :GltfCompressor(model)
	{
		model.extensionsRequired.push_back(extension.name);
		model.extensionsUsed.push_back(extension.name);

		for (auto& mesh : _model.meshes) {
			compressMesh(mesh, DracoCompressionOptions());
		}
	}

	KHR_draco_mesh_compression extension;
};

template<typename T>
inline int GltfDracoCompressor::initPointAttribute(draco::Mesh& dracoMesh, const std::string& attributeName, const std::vector<T>& bufferData, const tinygltf::Accessor& accessor)
{
	const auto numComponents = calculateNumComponents(accessor.type);
	const auto stride = sizeof(T) * numComponents;
	const auto dracoBbuffer = std::make_unique<draco::DataBuffer>();
	draco::GeometryAttribute atrribute;
	atrribute.Init(getTypeFromAttributeName(attributeName), &*dracoBbuffer, numComponents, getDataType(accessor.componentType),
		accessor.normalized, stride, 0);
	dracoMesh.set_num_points(static_cast<unsigned int>(accessor.count));
	const int attId = dracoMesh.AddAttribute(atrribute, true, dracoMesh.num_points());
	const auto attrActual = dracoMesh.attribute(attId);
	for (draco::PointIndex i(0); i < static_cast<uint32_t>(accessor.count); ++i)
	{
		attrActual->SetAttributeValue(attrActual->mapped_index(i), &bufferData[i.value() * numComponents]);
	}
	if (dracoMesh.num_points() == 0)
	{
		dracoMesh.set_num_points(static_cast<unsigned int>(accessor.count));
	}
	else if (dracoMesh.num_points() != accessor.count)
	{
		std::cerr << "draco:Inconsistent points count." << '\n';
	}

	return attId;
}

template<typename T>
inline int GltfDracoCompressor::initPointAttributeFromGltf(draco::Mesh& dracoMesh, const std::string& attributeName, const tinygltf::Accessor& accessor)
{
	std::vector<T> bufferData = getBufferData<T>(accessor);
	return initPointAttribute<T>(dracoMesh, attributeName, bufferData, accessor);
}

template<typename T>
inline void GltfDracoCompressor::initDracoMeshFaces(T& indices, draco::Mesh& dracoMesh)
{
	assert(indices.size() % 3 == 0);
	size_t numFaces = indices.size() / 3;
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

template<typename T>
inline void GltfDracoCompressor::initDracoMeshFromGltf(draco::Mesh& dracoMesh, const tinygltf::Accessor indicesAccessor, const tinygltf::BufferView indicesBv, tinygltf::Buffer indicesBuffer)
{
	auto indicesPtr = reinterpret_cast<const T*>(indicesBuffer.data.data() + indicesBv.byteOffset);
	std::vector<T> indices(indicesPtr, indicesPtr + indicesAccessor.count);
	initDracoMeshFaces(indices, dracoMesh);
}
#endif // !OSG_GIS_PLUGINS_GLTF_DRACO_COMPRESSOR_H
