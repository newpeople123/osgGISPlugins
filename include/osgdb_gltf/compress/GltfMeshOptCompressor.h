#ifndef OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H 1
#include "osgdb_gltf/compress/GltfCompressor.h"

class GltfMeshOptCompressor :public GltfCompressor {
private:
	int _positionBit = 14;
	int _normalBit = 8;
	int _texBit = 12;
	int _colorBit = 12;
	bool _compressmore = true;

	static void encodeQuat(osg::Vec4s v, osg::Vec4 a, int bits);

	static void encodeOct(int& fu, int& fv, float nx, float ny, float nz, int bits);

	static void encodeExpShared(osg::Vec4ui v, osg::Vec4 a, int bits);

	static unsigned int encodeExpOne(float v, int bits);

	static osg::ref_ptr<osg::Vec3uiArray> encodeExpParallel(std::vector<float> vertex, int bits);

	static int quantizeColor(float v, int bytebits, int bits);

	void quantizeMesh(tinygltf::Mesh& mesh, const double minVX, const double minVY, const double minVZ, const double scaleV);

	void restoreBuffer(tinygltf::Buffer& buffer, tinygltf::BufferView& bufferView, osg::ref_ptr<osg::Array> newBufferData);

	void recomputeTextureTransform(tinygltf::ExtensionMap& extensionMap,tinygltf::Accessor& accessor, const double minTx, const double minTy, const double scaleTx, const double scaleTy);

	void processMaterial(const tinygltf::Primitive primitive, tinygltf::Accessor& accessor, const double minTx, const double minTy, const double scaleTx, const double scaleTy);

	std::tuple<double, double, double, double> calculateTexcoordScales(
		const tinygltf::Model& model,
		const tinygltf::Primitive& primitive,
		const tinygltf::Accessor& accessor);
public:
	//启用该扩展需要展开变换矩阵
    KHR_mesh_quantization meshQuanExtension;
    EXT_meshopt_compression meshOptExtension;
    GltfMeshOptCompressor(tinygltf::Model& model) :GltfCompressor(model)
    {
        model.extensionsRequired.push_back(meshQuanExtension.name);
        //model.extensionsRequired.push_back(meshOptExtension.name);
        model.extensionsUsed.push_back(meshQuanExtension.name);
        //model.extensionsUsed.push_back(meshOptExtension.name);
		
		
		double minVX = FLT_MAX, minVY = FLT_MAX, minVZ = FLT_MAX, scaleV = -FLT_MAX, maxVX = -FLT_MAX, maxVY = -FLT_MAX, maxVZ = -FLT_MAX;
		double minTX = FLT_MAX, minTY = FLT_MAX, scaleTx = -FLT_MAX, scaleTy = -FLT_MAX, maxTX = -FLT_MAX, maxTY = -FLT_MAX;
		for (auto& mesh : _model.meshes)
		{
			for (const tinygltf::Primitive& primitive : mesh.primitives)
			{
				for (const auto& pair : primitive.attributes)
				{
					tinygltf::Accessor& accessor = _model.accessors[pair.second];
					if (accessor.type == TINYGLTF_TYPE_VEC3 && pair.first == "POSITION")
					{
						minVX = std::min(double(accessor.minValues[0]), minVX);
						minVY = std::min(double(accessor.minValues[1]), minVY);
						minVZ = std::min(double(accessor.minValues[2]), minVZ);

						maxVX = std::max(double(accessor.maxValues[0]), maxVX);
						maxVY = std::max(double(accessor.maxValues[1]), maxVY);
						maxVZ = std::max(double(accessor.maxValues[2]), maxVZ);
					}
					else if (accessor.type == TINYGLTF_TYPE_VEC2 && pair.first == "TEXCOORD_0")
					{
						minTX = std::min(double(accessor.minValues[0]), minTX);
						minTY = std::min(double(accessor.minValues[1]), minTY);

						maxTX = std::max(double(accessor.maxValues[0]), maxTX);
						maxTY = std::max(double(accessor.maxValues[1]), maxTY);
					}

				}

			}
		}
		scaleV = std::max(std::max(maxVX - minVX, maxVY - minVY), maxVZ - minVZ);
		scaleTx = maxTX - minTX;
		scaleTy = maxTY - minTY;
        for (auto& mesh : _model.meshes) {
			quantizeMesh(mesh, minVX, minVY, minVZ, scaleV);
        }

		for (auto& mesh : _model.meshes)
		{
			for (const tinygltf::Primitive& primitive : mesh.primitives)
			{
				_model.accessors[primitive.attributes.find("TEXCOORD_0")->second].maxValues.clear();
				_model.accessors[primitive.attributes.find("TEXCOORD_0")->second].minValues.clear();

			}
		}
		
    }
};
#endif // !OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H
