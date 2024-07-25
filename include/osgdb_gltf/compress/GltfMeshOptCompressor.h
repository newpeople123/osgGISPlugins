#ifndef OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H 1
#include "osgdb_gltf/compress/GltfCompressor.h"

class GltfMeshOptCompressor :public GltfCompressor {
private:
	int _positionBit = 14;//[1,16]
	int _normalBit = 8;//[1,16]
	int _texBit = 12;//[1,16]
	int _colorBit = 8;//[1,16]
	bool _compressmore = true;
	bool _posFloat = false;

	std::vector<int> _materialIndexes;
	//bool _posNormalized = false;

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

	std::tuple<double, double, double, double> getTexcoordBounds(
		const tinygltf::Primitive& primitive,
		const tinygltf::Accessor& accessor);

	std::tuple<double, double, double, double> getPositionBounds();
public:
	//启用该扩展需要展开变换矩阵
    KHR_mesh_quantization meshQuanExtension;
    EXT_meshopt_compression meshOptExtension;
    GltfMeshOptCompressor(tinygltf::Model& model) :GltfCompressor(model)
    {
		_positionBit = osg::clampTo(_positionBit, 1, 16);
		_normalBit = osg::clampTo(_normalBit, 1, 16);
		_texBit = osg::clampTo(_texBit, 1, 16);
		_colorBit = osg::clampTo(_colorBit, 1, 16);

        model.extensionsRequired.push_back(meshQuanExtension.name);
        //model.extensionsRequired.push_back(meshOptExtension.name);
        model.extensionsUsed.push_back(meshQuanExtension.name);
        //model.extensionsUsed.push_back(meshOptExtension.name);

		_materialIndexes.clear();

		
		std::tuple<double, double, double, double> result = getPositionBounds();
		const double minVX = std::get<0>(result);
		const double minVY = std::get<1>(result);
		const double minVZ = std::get<2>(result);
		const double scaleV = std::get<3>(result);
        for (auto& mesh : _model.meshes) {
			quantizeMesh(mesh, minVX, minVY, minVZ, scaleV);
        }

		for (auto& mesh : _model.meshes)
		{
			for (const tinygltf::Primitive& primitive : mesh.primitives)
			{
				if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
					_model.accessors[primitive.attributes.find("TEXCOORD_0")->second].maxValues.clear();
					_model.accessors[primitive.attributes.find("TEXCOORD_0")->second].minValues.clear();
				}

			}
		}
		
    }
};
#endif // !OSG_GIS_PLUGINS_GLTF_MESHOPT_COMPRESSOR_H
