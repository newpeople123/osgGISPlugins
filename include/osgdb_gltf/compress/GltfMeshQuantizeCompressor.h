#ifndef OSG_GIS_PLUGINS_GLTF_MESH_QUANTIZE_COMPRESSOR_H
#define OSG_GIS_PLUGINS_GLTF_MESH_QUANTIZE_COMPRESSOR_H 1
#include "osgdb_gltf/compress/GltfCompressor.h"
namespace osgGISPlugins {
	class GltfMeshQuantizeCompressor :public GltfCompressor
	{
	public:
		struct MeshQuantizeCompressionOptions :CompressionOptions {
			bool Compressmore = true;
			bool PositionFloat = false;
			bool PositionNormalized = false;//对应-vpi
			MeshQuantizeCompressionOptions() {
				PositionQuantizationBits = 14;//[1,16]
				TexCoordQuantizationBits = 12;//[1,16]
				NormalQuantizationBits = 8;//[1,16]
				ColorQuantizationBits = 8;//[1,16]
			}
		};
	private:

		std::vector<int> _materialIndexes;
		MeshQuantizeCompressionOptions _compressionOptions;

		static void encodeQuat(osg::Vec4s v, osg::Vec4 a, int bits);

		static void encodeOct(int& fu, int& fv, float nx, float ny, float nz, int bits);

		static void encodeExpShared(osg::Vec4ui v, osg::Vec4 a, int bits);

		static unsigned int encodeExpOne(float v, int bits);

		static osg::ref_ptr<osg::Vec3uiArray> encodeExpParallel(const std::vector<float>& vertex, int bits);

		static int quantizeColor(float v, int bytebits, int bits);

		void quantizeMesh(tinygltf::Mesh& mesh, double minVX, double minVY, double minVZ, double scaleV);

		void recomputeTextureTransform(tinygltf::ExtensionMap& extensionMap, const tinygltf::Accessor& accessor, double minTx, double minTy, double scaleTx, double scaleTy) const;

		void processMaterial(const tinygltf::Primitive& primitive, const tinygltf::Accessor& accessor, double minTx, double minTy, double scaleTx, double scaleTy);

		std::tuple<double, double, double, double> getTexcoordBounds(
			const tinygltf::Primitive& primitive,
			const tinygltf::Accessor& accessor) const;

		std::tuple<double, double, double, double> getPositionBounds() const;

	public:
		//启用该扩展需要展开变换矩阵，如果不展开的话，压缩率很低，所以这里要求必须展开
		KHR_mesh_quantization meshQuanExtension;
		GltfMeshQuantizeCompressor(tinygltf::Model& model, const MeshQuantizeCompressionOptions& compressionOptions) :GltfCompressor(model, "KHR_mesh_quantization"), _compressionOptions(compressionOptions) {
			if (_compressionOptions.PositionQuantizationBits > 16)
			{
				_compressionOptions.PositionFloat = true;
			}
			_compressionOptions.PositionQuantizationBits = osg::clampTo(_compressionOptions.PositionQuantizationBits, 1, 16);
			_compressionOptions.NormalQuantizationBits = osg::clampTo(_compressionOptions.NormalQuantizationBits, 1, 16);
			_compressionOptions.TexCoordQuantizationBits = osg::clampTo(_compressionOptions.TexCoordQuantizationBits, 1, 16);
			_compressionOptions.ColorQuantizationBits = osg::clampTo(_compressionOptions.ColorQuantizationBits, 1, 16);
		}

		void apply() override;

		static bool valid(const tinygltf::Model& model);
	};
}
#endif // !OSG_GIS_PLUGINS_GLTF_MESH_QUANTIZE_COMPRESSOR_H
