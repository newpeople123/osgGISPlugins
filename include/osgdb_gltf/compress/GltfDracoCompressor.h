#ifndef OSG_GIS_PLUGINS_GLTF_DRACO_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_DRACO_COMPRESSOR_H 1
#include "osgdb_gltf/compress/GltfCompressor.h"
#include <unordered_set>
#include <draco/compression/encode.h>
class GltfDracoCompressor :public GltfCompressor
{
public:
	struct DracoCompressionOptions:CompressionOptions {
		//Default value is medium level
		int GenericQuantizationBits = 16;
		int EncodeSpeed = 0;
		int DecodeSpeed = 10;
		DracoCompressionOptions() {
			//Default value is medium level
			PositionQuantizationBits = 14;
			TexCoordQuantizationBits = 12;
			NormalQuantizationBits = 10;
			ColorQuantizationBits = 8;
		}
	};
private:
	DracoCompressionOptions _compressionOptions;

	draco::GeometryAttribute::Type getTypeFromAttributeName(const std::string& name);

	draco::DataType getDataType(const int componentType);

	template<typename T>
	int initPointAttribute(draco::Mesh& dracoMesh, const std::string& attributeName, const tinygltf::Accessor& accessor);
	/// <summary>
	/// 目前只处理gl_triangles的情况
	/// </summary>
	/// <param name="mesh"></param>
	void compressMesh(tinygltf::Mesh& mesh);

	void setDracoEncoderOptions(draco::Encoder& encoder);

	template<typename T>
	void initDracoMeshFaces(const tinygltf::Accessor indicesAccessor, draco::Mesh& dracoMesh);

	void removeBufferViews(const std::unordered_set<int>& bufferViewsToRemove);
public:

	GltfDracoCompressor(tinygltf::Model& model,const DracoCompressionOptions compressionOptions) :GltfCompressor(model),_compressionOptions(compressionOptions)
	{
		model.extensionsRequired.push_back(extension.name);
		model.extensionsUsed.push_back(extension.name);

		for (auto& mesh : _model.meshes) {
			compressMesh(mesh);
		}
	}

	KHR_draco_mesh_compression extension;
};

template<typename T>
inline int GltfDracoCompressor::initPointAttribute(draco::Mesh& dracoMesh, const std::string& attributeName, const tinygltf::Accessor& accessor)
{
	std::vector<T> bufferData = getBufferData<T>(accessor);

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
inline void GltfDracoCompressor::initDracoMeshFaces(const tinygltf::Accessor indicesAccessor, draco::Mesh& dracoMesh)
{
	std::vector<T> indices = getBufferData<T>(indicesAccessor);
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
#endif // !OSG_GIS_PLUGINS_GLTF_DRACO_COMPRESSOR_H
