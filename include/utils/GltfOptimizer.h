#ifndef OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H
#define OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H
#include <osgUtil/Optimizer>
#include <meshoptimizer.h>
#include <osgdb_gltf/material/GltfMaterial.h>
#include <unordered_map>
#include <tbb/parallel_for.h>
#include <tbb/concurrent_unordered_map.h>
#include "TexturePacker.h"
#include "Utils.h"
#include <shared_mutex>
namespace osgGISPlugins
{
	class GltfOptimizer :public osgUtil::Optimizer
	{
	public:
		enum class GltfTextureType
		{
			NORMAL,
			OCCLUSION,
			EMISSIVE,
			METALLICROUGHNESS,
			BASECOLOR,
			DIFFUSE,
			SPECULARGLOSSINESS
		};

		struct GltfTextureOptimizationOptions
		{
			int maxTextureWidth = 512;
			int maxTextureHeight = 512;
			int maxTextureAtlasWidth = 2048;
			int maxTextureAtlasHeight = 2048;

			std::string ext = ".png";
			std::string cachePath = "./";
			bool packTexture = true;
			GltfTextureOptimizationOptions& operator=(const GltfTextureOptimizationOptions& other)
			{
				if (this != &other)
				{ // 避免自赋值
					maxTextureWidth = other.maxTextureWidth;
					maxTextureHeight = other.maxTextureHeight;
					maxTextureAtlasWidth = other.maxTextureAtlasWidth;
					maxTextureAtlasHeight = other.maxTextureAtlasHeight;
					ext = other.ext;
					cachePath = other.cachePath;
					packTexture = other.packTexture;
				}
				return *this;
			}
		};


		enum OptimizationOptions
		{
			FLATTEN_STATIC_TRANSFORMS = (1 << 0),
			REMOVE_REDUNDANT_NODES = (1 << 1),
			REMOVE_LOADED_PROXY_NODES = (1 << 2),
			COMBINE_ADJACENT_LODS = (1 << 3),
			SHARE_DUPLICATE_STATE = (1 << 4),
			MERGE_GEOMETRY = (1 << 5),
			CHECK_GEOMETRY = (1 << 6), // deprecated, currently no-op
			MAKE_FAST_GEOMETRY = (1 << 7),
			SPATIALIZE_GROUPS = (1 << 8),
			COPY_SHARED_NODES = (1 << 9),
			TRISTRIP_GEOMETRY = (1 << 10),
			TESSELLATE_GEOMETRY = (1 << 11),
			OPTIMIZE_TEXTURE_SETTINGS = (1 << 12),
			MERGE_GEODES = (1 << 13),
			FLATTEN_BILLBOARDS = (1 << 14),
			TEXTURE_ATLAS_BUILDER = (1 << 15),
			STATIC_OBJECT_DETECTION = (1 << 16),
			FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS = (1 << 17),
			INDEX_MESH = (1 << 18),
			VERTEX_POSTTRANSFORM = (1 << 19),
			VERTEX_PRETRANSFORM = (1 << 20),
			BUFFER_OBJECT_SETTINGS = (1 << 21),
			INDEX_MESH_BY_MESHOPTIMIZER = (1 << 22),// 简化前调用
			VERTEX_CACHE_BY_MESHOPTIMIZER = (1 << 23),
			OVER_DRAW_BY_MESHOPTIMIZER = (1 << 24),
			VERTEX_FETCH_BY_MESHOPTIMIZER = (1 << 25),
			TEXTURE_ATLAS_BUILDER_BY_STB = (1 << 26),
			FLATTEN_TRANSFORMS = (1 << 27),
			MERGE_TRANSFORMS = (1 << 28),
			GENERATE_NORMAL_TEXTURE = (1 << 29),


			REDUCE_DRAWCALL_OPTIMIZATIONS =
			TEXTURE_ATLAS_BUILDER_BY_STB |
			VERTEX_CACHE_BY_MESHOPTIMIZER |
			OVER_DRAW_BY_MESHOPTIMIZER |
			VERTEX_FETCH_BY_MESHOPTIMIZER,

			EXPORT_GLTF_OPTIMIZATIONS =
			INDEX_MESH |
			REDUCE_DRAWCALL_OPTIMIZATIONS,

			DEFAULT_OPTIMIZATIONS = FLATTEN_STATIC_TRANSFORMS |
			REMOVE_REDUNDANT_NODES |
			REMOVE_LOADED_PROXY_NODES |
			COMBINE_ADJACENT_LODS |
			SHARE_DUPLICATE_STATE |
			MERGE_GEOMETRY |
			MAKE_FAST_GEOMETRY |
			CHECK_GEOMETRY |
			OPTIMIZE_TEXTURE_SETTINGS |
			STATIC_OBJECT_DETECTION,

			ALL_OPTIMIZATIONS = FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS |
			REMOVE_REDUNDANT_NODES |
			REMOVE_LOADED_PROXY_NODES |
			COMBINE_ADJACENT_LODS |
			SHARE_DUPLICATE_STATE |
			MERGE_GEODES |
			MERGE_GEOMETRY |
			MAKE_FAST_GEOMETRY |
			CHECK_GEOMETRY |
			SPATIALIZE_GROUPS |
			COPY_SHARED_NODES |
			TRISTRIP_GEOMETRY |
			OPTIMIZE_TEXTURE_SETTINGS |
			TEXTURE_ATLAS_BUILDER |
			STATIC_OBJECT_DETECTION |
			BUFFER_OBJECT_SETTINGS
		};

		void optimize(osg::Node* node, unsigned int options) override;

		GltfTextureOptimizationOptions getGltfTextureOptimizationOptions() const { return _gltfTextureOptions; }

		void setGltfTextureOptimizationOptions(const GltfTextureOptimizationOptions& val) {
			_gltfTextureOptions = val;
		}

		class FlattenTransformsVisitor :public osgUtil::BaseOptimizerVisitor
		{
		public:
			FlattenTransformsVisitor(osgUtil::Optimizer* optimizer = 0) :
				BaseOptimizerVisitor(optimizer, FLATTEN_TRANSFORMS)
			{
			}

			void apply(osg::Drawable& drawable) override;

			void apply(osg::Transform& transform) override;
		private:
			osg::Matrixd _currentMatrix;
		};


		class ReindexMeshVisitor :public osgUtil::BaseOptimizerVisitor
		{
		private:
			template<typename DrawElementsType, typename IndexArrayType>
			static void reindexMesh(osg::Geometry& geometry, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex);
		public:
			template<typename DrawElementsType, typename IndexArrayType>
			static osg::ref_ptr<IndexArrayType> reindexMesh(osg::Geometry& geometry, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex, osg::MixinVector<unsigned int>& remap, const bool bGenerateIndex = true, const size_t uniqueVertexNum = 0);

			ReindexMeshVisitor(osgUtil::Optimizer* optimizer = 0) :
				BaseOptimizerVisitor(optimizer, INDEX_MESH_BY_MESHOPTIMIZER)
			{
			}

			void apply(osg::Geometry& geometry) override;

		};

		class VertexCacheVisitor :public osgUtil::BaseOptimizerVisitor
		{
		private:
			bool _compressmore = false;
		public:
			bool getCompressmore() const { return _compressmore; }

			void setCompressmore(const bool val) { _compressmore = val; }

			VertexCacheVisitor(osgUtil::Optimizer* optimizer = 0) :
				BaseOptimizerVisitor(optimizer, VERTEX_CACHE_BY_MESHOPTIMIZER)
			{
			}

			void apply(osg::Geometry& geometry) override;

			template <typename DrawElementsType, typename IndexArrayType>
			static void processDrawElements(osg::PrimitiveSet* pset, IndexArrayType& indices);
		};

		class OverDrawVisitor :public osgUtil::BaseOptimizerVisitor
		{
		public:
			OverDrawVisitor(osgUtil::Optimizer* optimizer = 0) :
				BaseOptimizerVisitor(optimizer, OVER_DRAW_BY_MESHOPTIMIZER)
			{
			}

			void apply(osg::Geometry& geometry) override;
		};

		class VertexFetchVisitor :public osgUtil::BaseOptimizerVisitor
		{
		public:
			VertexFetchVisitor(osgUtil::Optimizer* optimizer = 0) :
				BaseOptimizerVisitor(optimizer, VERTEX_FETCH_BY_MESHOPTIMIZER)
			{
			}

			void apply(osg::Geometry& geometry) override;
		};

		class TextureAtlasBuilderVisitor :public osgUtil::BaseOptimizerVisitor
		{
		private:
			static std::unordered_map<std::string, std::shared_ptr<std::mutex>> imageFileLocks;
			static std::shared_mutex imageFileLocksMapMutex;  // 控制访问map的锁

			static std::shared_ptr<std::mutex> getImageLock(const std::string& filename);

			void optimizeOsgTexture(const osg::ref_ptr<osg::StateSet>& stateSet, const osg::ref_ptr<osg::Geometry>& geom);

			void optimizeOsgTextureSize(osg::ref_ptr<osg::Texture2D> texture);

			void exportOsgTextureIfNeeded(osg::ref_ptr<osg::Texture2D> texture, const GltfTextureType type = GltfTextureType::BASECOLOR);

			void optimizeOsgMaterial(const osg::ref_ptr<GltfMaterial>& gltfMaterial, const osg::ref_ptr<osg::Geometry>& geom);

			static bool gltfMaterialHasTexture(const osg::ref_ptr<GltfMaterial>& gltfMaterial);

			void packOsgTextures();

			void packOsgMaterials();

			std::string exportImage(const osg::ref_ptr<osg::Image>& img, const GltfTextureType type);

			static void packImages(osg::ref_ptr<osg::Image>& img, std::vector<size_t>& indexes, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs, TexturePacker& packer);

			osg::ref_ptr<osg::Image> packImges(TexturePacker& packer, std::vector<osg::ref_ptr<osg::Image>>& imgs, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs);

			static void removePackedImages(std::vector<osg::ref_ptr<osg::Image>>& imgs, const std::vector<osg::ref_ptr<osg::Image>>& deleteImgs);

			void updateGltfMaterialUserValue(osg::ref_ptr<GltfMaterial> gltfMaterial,
				osg::ref_ptr<osg::Texture2D> texture,
				const double offsetX,
				const double offsetY,
				const double scaleX,
				const double scaleY,
				const std::string fullPath,
				const GltfTextureType type);

			static void resizeImageToPowerOfTwo(const osg::ref_ptr<osg::Image>& img, const int maxWidth, const int maxHeight);

			static bool compareImageHeight(osg::ref_ptr<osg::Image> img1, osg::ref_ptr<osg::Image> img2);

			void addImageFromTexture(const osg::ref_ptr<osg::Texture2D>& texture, std::vector<osg::ref_ptr<osg::Image>>& imgs);

			void processGltfPbrMRImages(std::vector<osg::ref_ptr<osg::Image>>& imageList, const GltfTextureType type);

			void processGltfPbrSGImages(std::vector<osg::ref_ptr<osg::Image>>& imageList, const GltfTextureType type);

			void processGltfGeneralImages(std::vector<osg::ref_ptr<osg::Image>>& imageList, const GltfTextureType type);

			void processTextureImages(
				std::vector<osg::ref_ptr<osg::Image>>& images,
				std::unordered_map<osg::Geometry*, osg::ref_ptr<GltfMaterial>> geometryGltfMaterialMap,
				const GltfTextureType type,
				const std::function<osg::ref_ptr<osg::Texture2D>(GltfMaterial*)>& getTextureFunc);

			static void removeRepeatImages(std::vector<osg::ref_ptr<osg::Image>>& imgs);

			GltfTextureOptimizationOptions _options;

			//osgMaterial
			std::vector<osg::ref_ptr<GltfMaterial>> _gltfMaterials;
			std::unordered_map<osg::Geometry*, osg::ref_ptr<GltfMaterial>> _geometryGltfMaterialMap;

			//osgTexture
			std::vector<osg::ref_ptr<osg::Image>> _images;
			std::map<osg::Geometry*, osg::ref_ptr<osg::Image>> _geometryImgMap;
		public:
			TextureAtlasBuilderVisitor(const GltfTextureOptimizationOptions options, osgUtil::Optimizer* optimizer = 0) :BaseOptimizerVisitor(optimizer, VERTEX_FETCH_BY_MESHOPTIMIZER), _options(options)
			{
			}

			void apply(osg::Drawable& drawable) override;

			void packTextures();

			~TextureAtlasBuilderVisitor()
			{
				_gltfMaterials.clear();
				_geometryGltfMaterialMap.clear();

				_images.clear();
				_geometryImgMap.clear();
			}
		};

		class MergeTransformVisitor :public osgUtil::BaseOptimizerVisitor
		{
		private:
			std::unordered_map<osg::Matrixd, std::vector<osg::ref_ptr<osg::Node>>, Utils::MatrixHash, Utils::MatrixEqual> _matrixNodesMap;
			osg::Matrixd _currentMatrix;
		public:
			MergeTransformVisitor(osgUtil::Optimizer* optimizer = 0) :
				BaseOptimizerVisitor(optimizer, MERGE_TRANSFORMS) {
			}

			void apply(osg::MatrixTransform& xtransform) override;

			void apply(osg::Geode& geode) override;

			osg::Node* getNode();
		};

		class GenerateNormalTextureVisitor :public osgUtil::BaseOptimizerVisitor
		{
		private:
			tbb::concurrent_unordered_map<std::string, osg::ref_ptr<osg::Image>> _imageCache;
			tbb::concurrent_unordered_map<std::string, osg::ref_ptr<osg::Image>> _imageCache2;


			osg::ref_ptr<osg::Image> getOrGenerateNormalMap(osg::Image* image);

			osg::ref_ptr<osg::Image> getOrGenerateMetallicRoughnessMap(osg::Image* image);

			void processGeometry(osg::Geometry* geometry);
		public:
			GenerateNormalTextureVisitor(osgUtil::Optimizer* optimizer = 0) :
				BaseOptimizerVisitor(optimizer, GENERATE_NORMAL_TEXTURE)
			{
			}

			void apply(osg::Geode& geode) override;
		};

		class GenerateTangentVisitor :public osgUtil::BaseOptimizerVisitor
		{
		private:
			void generateTangent(osg::Geometry& geometry);
		public:
			GenerateTangentVisitor(osgUtil::Optimizer* optimizer = 0) :
				BaseOptimizerVisitor(optimizer, GENERATE_NORMAL_TEXTURE)
			{
			}

			void apply(osg::Geometry& geometry) override;
		};

	private:
		GltfTextureOptimizationOptions _gltfTextureOptions;
	};

	template<typename DrawElementsType, typename IndexArrayType>
	inline void GltfOptimizer::ReindexMeshVisitor::reindexMesh(osg::Geometry& geometry, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex)
	{
		osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geometry.getVertexAttribArray(0));
		osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
		if (!positions.valid())
		{
			return;
		}
		osg::MixinVector<unsigned int> remap(positions->size());
		osg::ref_ptr<IndexArrayType> optimizedIndices = reindexMesh<DrawElementsType, IndexArrayType>(geometry, drawElements, psetIndex, remap);
		if (!optimizedIndices)
		{
			return;
		}
#pragma region filterTriangles
		const unsigned int indiceCount = drawElements->getNumIndices();
		size_t newIndiceCount = 0;
		for (size_t i = 0; i < indiceCount; i += 3)
		{
			const size_t a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);
			if (a != b && a != c && b != c)
			{
				optimizedIndices->at(newIndiceCount + 0) = a;
				optimizedIndices->at(newIndiceCount + 1) = b;
				optimizedIndices->at(newIndiceCount + 2) = c;
				newIndiceCount += 3;
			}
		}
		optimizedIndices->resize(newIndiceCount);
#pragma endregion

		geometry.setPrimitiveSet(psetIndex, new DrawElementsType(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));
	}

	template<typename DrawElementsType, typename IndexArrayType>
	inline osg::ref_ptr<IndexArrayType> GltfOptimizer::ReindexMeshVisitor::reindexMesh(osg::Geometry& geometry, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex, osg::MixinVector<unsigned int>& remap, const bool bGenerateIndex, const size_t uniqueVertexNum)
	{
		osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geometry.getVertexAttribArray(0));
		osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
		osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geometry.getNormalArray());
		osg::ref_ptr<osg::Vec2Array> texCoords = nullptr;
		if (geometry.getNumTexCoordArrays())
		{
			texCoords = dynamic_cast<osg::Vec2Array*>(geometry.getTexCoordArray(0));
		}

		if (!positions.valid())
		{
			return NULL;
		}

		if (normals.valid())
			if (positions->size() != normals->size())
			{
				return NULL;
			}
		if (texCoords.valid())
			if (positions->size() != texCoords->size())
			{
				return NULL;
			}

		osg::ref_ptr<IndexArrayType> indices = new IndexArrayType;
		const unsigned int indiceCount = drawElements->getNumIndices();
		osg::MixinVector<meshopt_Stream> streams;
		struct Attr
		{
			float f[4];
		};
		const size_t count = positions->size();
		osg::MixinVector<Attr> vertexData, normalData, texCoordData, batchIdData;

		for (size_t i = 0; i < count; ++i)
		{
			const osg::Vec3& vertex = positions->at(i);
			Attr v = { vertex.x(), vertex.y(), vertex.z(), 0.0f };
			vertexData.push_back(v);

			if (normals.valid())
			{
				const osg::Vec3& normal = normals->at(i);
				Attr n = { normal.x(), normal.y(), normal.z(), 0.0f };
				normalData.push_back(n);
			}
			if (texCoords.valid())
			{
				const osg::Vec2& texCoord = texCoords->at(i);
				Attr t = { texCoord.x(), texCoord.y(), 0.0f, 0.0f };
				texCoordData.push_back(t);
			}
			if (batchIds.valid())
			{
				Attr b = { batchIds->at(i), 0.0f, 0.0f, 0.0f };
				batchIdData.push_back(b);
			}
		}
		meshopt_Stream vertexStream = { vertexData.asVector().data(), sizeof(Attr), sizeof(Attr) };
		streams.push_back(vertexStream);
		if (normals.valid())
		{
			meshopt_Stream normalStream = { normalData.asVector().data(), sizeof(Attr), sizeof(Attr) };
			streams.push_back(normalStream);
		}
		if (texCoords.valid())
		{
			meshopt_Stream texCoordStream = { texCoordData.asVector().data(), sizeof(Attr), sizeof(Attr) };
			streams.push_back(texCoordStream);
		}

		for (size_t i = 0; i < indiceCount; ++i)
		{
			indices->push_back(drawElements->at(i));
		}

		size_t uniqueVertexCount = uniqueVertexNum;
		if (bGenerateIndex)
		{
			uniqueVertexCount = meshopt_generateVertexRemapMulti(&remap.asVector()[0], &(*indices)[0], indices->size(), count, &streams.asVector()[0], streams.size());
			assert(uniqueVertexCount <= count);
		}

		osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(count);
		osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(count);
		osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(count);
		osg::ref_ptr<osg::FloatArray> optimizedBatchIds = new osg::FloatArray(count);
		osg::ref_ptr<IndexArrayType> optimizedIndices = new IndexArrayType(indices->size());

		meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), &remap.asVector()[0]);
		meshopt_remapVertexBuffer(&vertexData.asVector()[0], &vertexData.asVector()[0], count, sizeof(Attr), &remap.asVector()[0]);
		vertexData.resize(uniqueVertexCount);

		if (normals.valid())
		{
			meshopt_remapVertexBuffer(&normalData.asVector()[0], &normalData.asVector()[0], count, sizeof(Attr), &remap.asVector()[0]);
			normalData.resize(uniqueVertexCount);
		}
		if (texCoords.valid())
		{
			meshopt_remapVertexBuffer(&texCoordData.asVector()[0], &texCoordData.asVector()[0], count, sizeof(Attr), &remap.asVector()[0]);
			texCoordData.resize(uniqueVertexCount);
		}
		if (batchIds.valid())
		{
			meshopt_remapVertexBuffer(&batchIdData.asVector()[0], &batchIdData.asVector()[0], count, sizeof(Attr), &remap.asVector()[0]);
			batchIdData.resize(uniqueVertexCount);
		}
		for (size_t i = 0; i < uniqueVertexCount; ++i)
		{
			optimizedVertices->at(i) = osg::Vec3(vertexData[i].f[0], vertexData[i].f[1], vertexData[i].f[2]);
			if (normals.valid())
			{
				optimizedNormals->at(i) = osg::Vec3(normalData[i].f[0], normalData[i].f[1], normalData[i].f[2]);
			}
			if (texCoords.valid())
				optimizedTexCoords->at(i) = osg::Vec2(texCoordData[i].f[0], texCoordData[i].f[1]);
			if (batchIds.valid())
			{
				optimizedBatchIds->at(i) = batchIdData[i].f[0];
			}
		}

		positions->assign(optimizedVertices->begin(), optimizedVertices->end());
		if (normals.valid())
			normals->assign(optimizedNormals->begin(), optimizedNormals->end());
		if (texCoords.valid())
			texCoords->assign(optimizedTexCoords->begin(), optimizedTexCoords->end());
		if (batchIds.valid())
			batchIds->assign(optimizedBatchIds->begin(), optimizedBatchIds->end());

		return optimizedIndices.release();
	}

	template<typename DrawElementsType, typename IndexArrayType>
	inline void GltfOptimizer::VertexCacheVisitor::processDrawElements(osg::PrimitiveSet* pset, IndexArrayType& indices)
	{
		DrawElementsType* drawElements = dynamic_cast<DrawElementsType*>(pset);
		if (drawElements)
		{
			indices.insert(indices.end(), drawElements->begin(), drawElements->end());
		}
	}
}
#endif // !OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H
