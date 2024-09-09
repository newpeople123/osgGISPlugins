#include "utils/GltfOptimizer.h"
#include <osgUtil/TransformAttributeFunctor>
#include <osgUtil/Tessellator>
#include <osgUtil/Statistics>
#include <osgUtil/MeshOptimizers>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include <iomanip>
#include <osgdb_gltf/material/GltfPbrMRMaterial.h>
#include <osgdb_gltf/material/GltfPbrSGMaterial.h>

#include <osgViewer/Viewer>
#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/SphericalManipulator>
#include <osgViewer/ViewerEventHandlers>
using namespace osgGISPlugins;

void GltfOptimizer::optimize(osg::Node* node, unsigned int options)
{
	osgUtil::StatsVisitor stats;

	if (osg::getNotifyLevel() >= osg::INFO)
	{
		node->accept(stats);
		stats.totalUpStats();
		OSG_NOTICE << std::endl << "Stats before:" << std::endl;
		stats.print(osg::notify(osg::NOTICE));
	}

	if (options & TEXTURE_ATLAS_BUILDER_BY_STB)
	{
		OSG_INFO << "Optimizer::optimize() doing TEXTURE_ATLAS_BUILDER by stb library" << std::endl;
		TextureAtlasBuilderVisitor tabv(_gltfTextureOptions, this);
		node->accept(tabv);
		tabv.packTextures();


		// now merge duplicate state, that may have been introduced by merge textures into texture atlas'
		bool combineDynamicState = false;
		bool combineStaticState = true;
		bool combineUnspecifiedState = true;

		osgUtil::Optimizer::StateVisitor osv(combineDynamicState, combineStaticState, combineUnspecifiedState, this);
		node->accept(osv);
		osv.optimize();
	}

	if (options & STATIC_OBJECT_DETECTION)
	{
		osgUtil::Optimizer::StaticObjectDetectionVisitor sodv;
		node->accept(sodv);
	}

	if (options & TESSELLATE_GEOMETRY)
	{
		OSG_INFO << "Optimizer::optimize() doing TESSELLATE_GEOMETRY" << std::endl;

		osgUtil::Optimizer::TessellateVisitor tsv;
		node->accept(tsv);
	}

	if (options & REMOVE_LOADED_PROXY_NODES)
	{
		OSG_INFO << "Optimizer::optimize() doing REMOVE_LOADED_PROXY_NODES" << std::endl;

		osgUtil::Optimizer::RemoveLoadedProxyNodesVisitor rlpnv(this);
		node->accept(rlpnv);
		rlpnv.removeRedundantNodes();

	}

	if (options & COMBINE_ADJACENT_LODS)
	{
		OSG_INFO << "Optimizer::optimize() doing COMBINE_ADJACENT_LODS" << std::endl;

		osgUtil::Optimizer::CombineLODsVisitor clv(this);
		node->accept(clv);
		clv.combineLODs();
	}

	if (options & OPTIMIZE_TEXTURE_SETTINGS)
	{
		OSG_INFO << "Optimizer::optimize() doing OPTIMIZE_TEXTURE_SETTINGS" << std::endl;

		osgUtil::Optimizer::TextureVisitor tv(true, true, // unref image
			false, false, // client storage
			false, 1.0, // anisotropic filtering
			this);
		node->accept(tv);
	}

	if (options & SHARE_DUPLICATE_STATE)
	{
		OSG_INFO << "Optimizer::optimize() doing SHARE_DUPLICATE_STATE" << std::endl;

		bool combineDynamicState = false;
		bool combineStaticState = true;
		bool combineUnspecifiedState = true;

		osgUtil::Optimizer::StateVisitor osv(combineDynamicState, combineStaticState, combineUnspecifiedState, this);
		node->accept(osv);
		osv.optimize();
	}

	if ((options & TEXTURE_ATLAS_BUILDER) && !(options & TEXTURE_ATLAS_BUILDER_BY_STB))
	{
		OSG_INFO << "Optimizer::optimize() doing TEXTURE_ATLAS_BUILDER" << std::endl;

		// traverse the scene collecting textures into texture atlas.
		osgUtil::Optimizer::TextureAtlasVisitor tav(this);
		node->accept(tav);
		tav.optimize();

		// now merge duplicate state, that may have been introduced by merge textures into texture atlas'
		bool combineDynamicState = false;
		bool combineStaticState = true;
		bool combineUnspecifiedState = true;

		osgUtil::Optimizer::StateVisitor osv(combineDynamicState, combineStaticState, combineUnspecifiedState, this);
		node->accept(osv);
		osv.optimize();
	}


	if (options & COPY_SHARED_NODES)
	{
		OSG_INFO << "Optimizer::optimize() doing COPY_SHARED_NODES" << std::endl;

		osgUtil::Optimizer::CopySharedSubgraphsVisitor cssv(this);
		node->accept(cssv);
		cssv.copySharedNodes();
	}

	if (options & FLATTEN_STATIC_TRANSFORMS)
	{
		OSG_INFO << "Optimizer::optimize() doing FLATTEN_STATIC_TRANSFORMS" << std::endl;

		int i = 0;
		bool result = false;
		do
		{
			OSG_DEBUG << "** RemoveStaticTransformsVisitor *** Pass " << i << std::endl;
			osgUtil::Optimizer::FlattenStaticTransformsVisitor fstv(this);
			node->accept(fstv);
			result = fstv.removeTransforms(node);
			++i;
		} while (result);

		// now combine any adjacent static transforms.
		osgUtil::Optimizer::CombineStaticTransformsVisitor cstv(this);
		node->accept(cstv);
		cstv.removeTransforms(node);
	}
	else if (options & FLATTEN_TRANSFORMS)
	{
		OSG_INFO << "Optimizer::optimize() doing FLATTEN_TRANSFORMS" << std::endl;
		FlattenTransformsVisitor ftv(this);
		node->accept(ftv);
	}

	if (options & FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS)
	{
		OSG_INFO << "Optimizer::optimize() doing FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS" << std::endl;

		// now combine any adjacent static transforms.
		osgUtil::Optimizer::FlattenStaticTransformsDuplicatingSharedSubgraphsVisitor fstdssv(this);
		node->accept(fstdssv);

	}

	if (options & REMOVE_REDUNDANT_NODES)
	{
		OSG_INFO << "Optimizer::optimize() doing REMOVE_REDUNDANT_NODES" << std::endl;

		osgUtil::Optimizer::RemoveEmptyNodesVisitor renv(this);
		node->accept(renv);
		renv.removeEmptyNodes();

		osgUtil::Optimizer::RemoveRedundantNodesVisitor rrnv(this);
		node->accept(rrnv);
		rrnv.removeRedundantNodes();

	}

	if (options & MERGE_GEODES)
	{
		OSG_INFO << "Optimizer::optimize() doing MERGE_GEODES" << std::endl;

		osg::Timer_t startTick = osg::Timer::instance()->tick();

		osgUtil::Optimizer::MergeGeodesVisitor visitor;
		node->accept(visitor);

		osg::Timer_t endTick = osg::Timer::instance()->tick();

		OSG_INFO << "MERGE_GEODES took " << osg::Timer::instance()->delta_s(startTick, endTick) << std::endl;
	}

	if (options & MAKE_FAST_GEOMETRY)
	{
		OSG_INFO << "Optimizer::optimize() doing MAKE_FAST_GEOMETRY" << std::endl;

		osgUtil::Optimizer::MakeFastGeometryVisitor mgv(this);
		node->accept(mgv);
	}

	if (options & MERGE_GEOMETRY)
	{
		OSG_INFO << "Optimizer::optimize() doing MERGE_GEOMETRY" << std::endl;

		osg::Timer_t startTick = osg::Timer::instance()->tick();

		osgUtil::Optimizer::MergeGeometryVisitor mgv(this);
		mgv.setTargetMaximumNumberOfVertices(10000);
		node->accept(mgv);

		osg::Timer_t endTick = osg::Timer::instance()->tick();

		OSG_INFO << "MERGE_GEOMETRY took " << osg::Timer::instance()->delta_s(startTick, endTick) << std::endl;
	}


	if (options & FLATTEN_BILLBOARDS)
	{
		osgUtil::Optimizer::FlattenBillboardVisitor fbv(this);
		node->accept(fbv);
		fbv.process();
	}

	if (options & SPATIALIZE_GROUPS)
	{
		OSG_INFO << "Optimizer::optimize() doing SPATIALIZE_GROUPS" << std::endl;

		osgUtil::Optimizer::SpatializeGroupsVisitor sv(this);
		node->accept(sv);
		sv.divide();
	}

	if (options & INDEX_MESH)
	{
		OSG_INFO << "Optimizer::optimize() doing INDEX_MESH" << std::endl;
		osgUtil::IndexMeshVisitor imv(this);
		node->accept(imv);
		imv.makeMesh();
	}
	else if (options & INDEX_MESH_BY_MESHOPTIMIZER)
	{
		OSG_INFO << "Optimizer::optimize() doing INDEX_MESH by meshoptimizer library" << std::endl;
		osgUtil::IndexMeshVisitor imv(this);
		node->accept(imv);
		imv.makeMesh();
		ReindexMeshVisitor rmv(this);
		node->accept(rmv);
	}

	if (options & VERTEX_POSTTRANSFORM)
	{
		OSG_INFO << "Optimizer::optimize() doing VERTEX_POSTTRANSFORM" << std::endl;
		osgUtil::VertexCacheVisitor vcv;
		node->accept(vcv);
		vcv.optimizeVertices();
	}

	if (options & VERTEX_PRETRANSFORM)
	{
		OSG_INFO << "Optimizer::optimize() doing VERTEX_PRETRANSFORM" << std::endl;
		osgUtil::VertexAccessOrderVisitor vaov;
		node->accept(vaov);
		vaov.optimizeOrder();
	}

	if (options & BUFFER_OBJECT_SETTINGS)
	{
		OSG_INFO << "Optimizer::optimize() doing BUFFER_OBJECT_SETTINGS" << std::endl;
		osgUtil::Optimizer::BufferObjectVisitor bov(true, true, true, true, true, false);
		node->accept(bov);
	}

	if (options & VERTEX_CACHE_BY_MESHOPTIMIZER)
	{
		OSG_INFO << "Optimizer::optimize() doing VERTEX_CACHE_BY_MESHOPTIMIZER by meshoptimizer library" << std::endl;
		VertexCacheVisitor vcv(this);
		node->accept(vcv);
	}

	if (options & OVER_DRAW_BY_MESHOPTIMIZER)
	{
		OSG_INFO << "Optimizer::optimize() doing OVER_DRAW_BY_MESHOPTIMIZER by meshoptimizer library" << std::endl;
		OverDrawVisitor odv(this);
		node->accept(odv);
	}

	if (options & VERTEX_FETCH_BY_MESHOPTIMIZER)
	{
		OSG_INFO << "Optimizer::optimize() doing VERTEX_FETCH_BY_MESHOPTIMIZER by meshoptimizer library" << std::endl;
		VertexFetchVisitor vfv(this);
		node->accept(vfv);
	}

	if (osg::getNotifyLevel() >= osg::INFO)
	{
		stats.reset();
		node->accept(stats);
		stats.totalUpStats();
		OSG_NOTICE << std::endl << "Stats after:" << std::endl;
		stats.print(osg::notify(osg::NOTICE));
	}
}
/** FlattenTransformsVisitor */
void GltfOptimizer::FlattenTransformsVisitor::apply(osg::Drawable& drawable)
{
	if (drawable.getDataVariance() == osg::Object::DataVariance::STATIC)
		return;
	const osg::MatrixList matrix_list = drawable.getWorldMatrices();
	osg::Matrixd mat;
	for (const osg::Matrixd& matrix : matrix_list)
	{
		mat = mat * matrix;
	}
	osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(drawable.asGeometry()->getVertexArray());
	if (mat != osg::Matrixd::identity())
	{
		for (auto& i : *positions)
		{
			i = i * mat;
		}
		drawable.asGeometry()->setVertexArray(positions);
		drawable.dirtyBound();
		drawable.computeBound();
	}
}

void GltfOptimizer::FlattenTransformsVisitor::apply(osg::Transform& transform)
{
	traverse(transform);
	if (transform.getDataVariance() == osg::Object::DataVariance::STATIC)
		return;
	osg::ref_ptr<osg::MatrixTransform> matrixTransform = transform.asMatrixTransform();
	if (matrixTransform.valid())
	{
		matrixTransform->setMatrix(osg::Matrixd::identity());
	}
	else
	{
		osg::ref_ptr<osg::PositionAttitudeTransform> positionAttitudeTransform = transform.asPositionAttitudeTransform();
		if (positionAttitudeTransform.valid())
		{
			positionAttitudeTransform->setPosition(osg::Vec3());
			positionAttitudeTransform->setAttitude(osg::Quat());
		}
	}
	transform.dirtyBound();
	transform.computeBound();
}

/** ReindexMeshVisitor */
void GltfOptimizer::ReindexMeshVisitor::apply(osg::Geometry& geometry)
{
	if (geometry.getDataVariance() == osg::Object::DataVariance::STATIC)
		return;
	const unsigned int psetCount = geometry.getNumPrimitiveSets();
	if (psetCount <= 0) return;
	for (size_t primIndex = 0; primIndex < psetCount; ++primIndex)
	{
		osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
		if (typeid(*pset.get()) == typeid(osg::DrawElementsUShort))
		{
			osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
			reindexMesh<osg::DrawElementsUShort, osg::UShortArray>(geometry, drawElementsUShort, primIndex);
		}
		else if (typeid(*pset.get()) == typeid(osg::DrawElementsUInt))
		{
			osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
			reindexMesh<osg::DrawElementsUInt, osg::UIntArray>(geometry, drawElementsUInt, primIndex);
		}
	}
}

/** VertexCacheVisitor */
void GltfOptimizer::VertexCacheVisitor::apply(osg::Geometry& geometry)
{
	if (geometry.getDataVariance() == osg::Object::DataVariance::STATIC)
		return;
	const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
	if (!positions) return;
	const size_t vertexCount = positions->size();
	const unsigned int psetCount = geometry.getNumPrimitiveSets();
	if (psetCount <= 0) return;

	for (size_t primIndex = 0; primIndex < psetCount; ++primIndex)
	{
		osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
		if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES)
		{
			const osg::PrimitiveSet::Type type = pset->getType();
			if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType)
			{
				osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
				processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
				if (indices->empty()) continue;
				if (_compressmore)
					//能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
					meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				else
					meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);

				osg::ref_ptr<osg::DrawElementsUByte> drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
				if (indices->empty()) continue;
				if (_compressmore)
					//能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
					meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				else
					meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);

				osg::ref_ptr<osg::DrawElementsUShort> drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
				if (indices->empty()) continue;
				if (_compressmore)
					//能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
					meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
				else
					meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);

				osg::ref_ptr<osg::DrawElementsUInt> drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
		}
	}
}

/** OverDrawVisitor */
void GltfOptimizer::OverDrawVisitor::apply(osg::Geometry& geometry)
{
	if (geometry.getDataVariance() == osg::Object::DataVariance::STATIC)
		return;
	const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
	if (!positions) return;
	const size_t vertexCount = positions->size();
	const unsigned int psetCount = geometry.getNumPrimitiveSets();
	if (psetCount <= 0) return;

	for (size_t primIndex = 0; primIndex < psetCount; ++primIndex)
	{
		osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
		if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES)
		{
			const osg::PrimitiveSet::Type type = pset->getType();
			if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType)
			{
				osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
				if (indices->empty()) continue;
				meshopt_optimizeOverdraw(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), &positions->at(0).x(), positions->size(), sizeof(osg::Vec3f), 1.05f);

				osg::ref_ptr<osg::DrawElementsUByte> drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
				if (indices->empty()) continue;
				meshopt_optimizeOverdraw(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), &positions->at(0).x(), positions->size(), sizeof(osg::Vec3f), 1.05f);

				osg::ref_ptr<osg::DrawElementsUShort> drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());

			}
			else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
				if (indices->empty()) continue;
				meshopt_optimizeOverdraw(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), &positions->at(0).x(), positions->size(), sizeof(osg::Vec3f), 1.05f);

				osg::ref_ptr<osg::DrawElementsUInt> drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
				drawElements->assign(indices->begin(), indices->end());

			}
		}
	}
}

/** VertexFetchVisitor */
void GltfOptimizer::VertexFetchVisitor::apply(osg::Geometry& geometry)
{
	if (geometry.getDataVariance() == osg::Object::DataVariance::STATIC)
		return;
	const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
	if (!positions) return;
	const size_t vertexCount = positions->size();
	const unsigned int psetCount = geometry.getNumPrimitiveSets();
	if (psetCount <= 0) return;
	for (size_t primIndex = 0; primIndex < psetCount; ++primIndex)
	{
		osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
		if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES)
		{
			const osg::PrimitiveSet::Type type = pset->getType();
			if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType)
			{
				osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
				if (indices->empty()) continue;

				osg::MixinVector<unsigned int> remap(vertexCount);
				size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap.asVector()[0], &indices->asVector()[0], indices->asVector().size(), vertexCount);
				assert(uniqueVertices <= vertexCount);
				osg::ref_ptr<osg::DrawElementsUByte> drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
				osg::ref_ptr<osg::UByteArray> optimizedIndices = GltfOptimizer::ReindexMeshVisitor::reindexMesh<osg::DrawElementsUByte, osg::UByteArray>(geometry, drawElements, primIndex, remap);
				if (!optimizedIndices) continue;
				drawElements->assign(optimizedIndices->begin(), optimizedIndices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
				if (indices->empty()) continue;

				osg::MixinVector<unsigned int> remap(vertexCount);
				size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap.asVector()[0], &indices->asVector()[0], indices->asVector().size(), vertexCount);

				assert(uniqueVertices <= vertexCount);
				osg::ref_ptr<osg::DrawElementsUShort> drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
				osg::ref_ptr<osg::UShortArray> optimizedIndices = GltfOptimizer::ReindexMeshVisitor::reindexMesh<osg::DrawElementsUShort, osg::UShortArray>(geometry, drawElements, primIndex, remap);
				if (!optimizedIndices) continue;
				drawElements->assign(optimizedIndices->begin(), optimizedIndices->end());
			}
			else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				GltfOptimizer::VertexCacheVisitor::processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
				if (indices->empty()) continue;

				osg::MixinVector<unsigned int> remap(vertexCount);
				size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap.asVector()[0], &indices->asVector()[0], indices->asVector().size(), vertexCount);
				assert(uniqueVertices <= vertexCount);
				osg::ref_ptr<osg::DrawElementsUInt> drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
				osg::ref_ptr<osg::UIntArray> optimizedIndices = GltfOptimizer::ReindexMeshVisitor::reindexMesh<osg::DrawElementsUInt, osg::UIntArray>(geometry, drawElements, primIndex, remap);
				if (!optimizedIndices) continue;
				drawElements->assign(optimizedIndices->begin(), optimizedIndices->end());
			}
		}
	}
}

/** TextureAtlasBuilderVisitor */
void GltfOptimizer::TextureAtlasBuilderVisitor::apply(osg::Drawable& drawable)
{
	const osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
	osg::ref_ptr<osg::StateSet> stateSet = drawable.getStateSet();
	if (stateSet.valid())
	{
		const osg::Object::DataVariance dv = stateSet->getDataVariance();
		if (dv == osg::Object::DataVariance::STATIC)
			return;
		const osg::ref_ptr<osg::Material> osgMaterial = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
		if (osgMaterial.valid())
		{
			const std::type_info& materialId = typeid(*osgMaterial.get());
			if (materialId == typeid(GltfMaterial) || materialId == typeid(GltfPbrMRMaterial) || materialId == typeid(GltfPbrSGMaterial))
			{
				osg::ref_ptr<GltfMaterial> gltfMaterial = dynamic_cast<GltfMaterial*>(osgMaterial.get());
				optimizeOsgMaterial(gltfMaterial, geom);
			}
			else
			{
				optimizeOsgTexture(stateSet, geom);
			}
		}
		else
		{
			optimizeOsgTexture(stateSet, geom);
		}
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::packTextures()
{
	packOsgTextures();
	packOsgMaterials();
}

void GltfOptimizer::TextureAtlasBuilderVisitor::optimizeOsgTexture(const osg::ref_ptr<osg::StateSet>& stateSet, const osg::ref_ptr<osg::Geometry>& geom)
{
	osg::ref_ptr<osg::Texture2D> texture = dynamic_cast<osg::Texture2D*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
	osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
	if (texCoords.valid() && texture.valid())
	{
		osg::ref_ptr<osg::Image> image = texture->getImage(0);
		if (image.valid())
		{
			resizeImageToPowerOfTwo(image);
			if (_options.packTexture)
			{
				bool bBuildTexturePacker = true;
				for (auto texCoord : *texCoords.get())
				{
					if (std::fabs(texCoord.x()) - 1.0 > 0.001 || std::fabs(texCoord.y()) - 1.0 > 0.001)
					{
						bBuildTexturePacker = false;
						break;
					}
				}
				if (bBuildTexturePacker)
				{
					if (image->s() < _options.maxWidth || image->t() < _options.maxHeight)
					{
						bool bAdd = true;
						for (auto img : _images)
						{
							if (img->getFileName() == image->getFileName())
							{
								_geometryImgMap[geom] = img;
								bAdd = false;
							}
						}
						if (bAdd)
						{
							_images.push_back(image);
							_geometryImgMap[geom] = image;
						}
					}
					else
					{
						std::string name;
						image->getUserValue(BASECOLOR_TEXTURE_FILENAME, name);
						if (name.empty())
						{
							exportImage(image);
						}
					}
				}
				else
				{
					std::string name;
					image->getUserValue(BASECOLOR_TEXTURE_FILENAME, name);
					if (name.empty())
					{
						exportImage(image);
					}
				}
			}
			else
			{
				std::string name;
				image->getUserValue(BASECOLOR_TEXTURE_FILENAME, name);
				if (name.empty())
				{
					exportImage(image);
				}
			}
		}
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::optimizeOsgTextureSize(osg::ref_ptr<osg::Texture2D> texture)
{
	if (texture.valid())
	{
		if (texture->getNumImages())
		{
			osg::ref_ptr<osg::Image> image = texture->getImage(0);
			if (image.valid())
			{
				resizeImageToPowerOfTwo(image);
			}
		}
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::exportOsgTexture(osg::ref_ptr<osg::Texture2D> texture)
{
	if (texture.valid())
	{
		if (texture->getNumImages())
		{
			osg::ref_ptr<osg::Image> image = texture->getImage(0);
			if (image.valid())
			{
				std::string name;
				image->getUserValue(BASECOLOR_TEXTURE_FILENAME, name);
				if (name.empty())
				{
					exportImage(image);
				}
			}
		}
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::optimizeOsgMaterial(const osg::ref_ptr<GltfMaterial>& gltfMaterial, const osg::ref_ptr<osg::Geometry>& geom)
{
	if (gltfMaterial.valid())
	{
		osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
		if (texCoords.valid())
		{
			optimizeOsgTextureSize(gltfMaterial->normalTexture);
			optimizeOsgTextureSize(gltfMaterial->occlusionTexture);
			optimizeOsgTextureSize(gltfMaterial->emissiveTexture);
			osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
			if (gltfMRMaterial.valid())
			{
				optimizeOsgTextureSize(gltfMRMaterial->metallicRoughnessTexture);
				optimizeOsgTextureSize(gltfMRMaterial->baseColorTexture);
			}
			for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
			{
				GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
				if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness))
				{
					KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
					optimizeOsgTextureSize(pbrSpecularGlossiness_extension->osgDiffuseTexture);
					optimizeOsgTextureSize(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture);
				}
			}

			if (_options.packTexture)
			{
				bool bBuildTexturePacker = true;
				for (auto texCoord : *texCoords.get())
				{
					if (std::fabs(texCoord.x()) - 1.0 > 0.001 || std::fabs(texCoord.y()) - 1.0 > 0.001)
					{
						bBuildTexturePacker = false;
						break;
					}
				}
				if (bBuildTexturePacker)
				{
					bool bAdd = true;
					for (auto* item : _gltfMaterials)
					{
						if (item == gltfMaterial.get())
						{
							_geometryMatMap[geom] = item;
							bAdd = false;
						}
					}
					if (bAdd)
					{
						_gltfMaterials.push_back(gltfMaterial.get());
						_geometryMatMap[geom] = gltfMaterial.get();
					}
				}
				else
				{
					exportOsgTexture(gltfMaterial->normalTexture);
					exportOsgTexture(gltfMaterial->occlusionTexture);
					exportOsgTexture(gltfMaterial->emissiveTexture);
					osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
					if (gltfMRMaterial.valid())
					{
						exportOsgTexture(gltfMRMaterial->metallicRoughnessTexture);
						exportOsgTexture(gltfMRMaterial->baseColorTexture);
					}
					for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
					{
						GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
						if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness))
						{
							KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
							exportOsgTexture(pbrSpecularGlossiness_extension->osgDiffuseTexture);
							exportOsgTexture(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture);
						}
					}
				}
			}
			else
			{
				exportOsgTexture(gltfMaterial->normalTexture);
				exportOsgTexture(gltfMaterial->occlusionTexture);
				exportOsgTexture(gltfMaterial->emissiveTexture);
				osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial.get());
				if (gltfMRMaterial.valid())
				{
					exportOsgTexture(gltfMRMaterial->metallicRoughnessTexture);
					exportOsgTexture(gltfMRMaterial->baseColorTexture);
				}
				for (size_t i = 0; i < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
				{
					GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(i);
					if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness))
					{
						KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
						exportOsgTexture(pbrSpecularGlossiness_extension->osgDiffuseTexture);
						exportOsgTexture(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture);
					}
				}
			}
		}
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::packOsgTextures()
{
	if (_images.empty()) return;
	while (_images.size())
	{
		TexturePacker packer(_options.maxWidth, _options.maxHeight);
		std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
		osg::ref_ptr<osg::Image> packedImage = packImges(packer, _images, deleteImgs);

		if (packedImage.valid() && deleteImgs.size())
		{
			const int width = packedImage->s(), height = packedImage->t();
			bool bResizeImage = resizeImageToPowerOfTwo(packedImage);
			exportImage(packedImage);
			for (auto& entry : _geometryImgMap)
			{
				osg::Geometry* geometry = entry.first;
				osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
				osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_STATESETS);

				osg::ref_ptr<osg::Texture2D> packedTexture = dynamic_cast<osg::Texture2D*>(osg::clone(stateSetCopy->getTextureAttribute(0, osg::StateAttribute::TEXTURE), osg::CopyOp::DEEP_COPY_STATEATTRIBUTES));

				packedTexture->setImage(packedImage);
				osg::Image* image = entry.second;
				int w, h;
				double x, y;
				const size_t id = packer.getId(image);
				if (packer.getPackingData(id, x, y, w, h))
				{
					packedTexture->setUserValue(TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME, true);
					double offsetX = x / width;
					packedTexture->setUserValue(TEX_TRANSFORM_BASECOLOR_OFFSET_X, osg::clampTo(offsetX, 0.0, 1.0));
					double offsetY = y / height;
					packedTexture->setUserValue(TEX_TRANSFORM_BASECOLOR_OFFSET_Y, osg::clampTo(offsetY, 0.0, 1.0));
					double scaleX = static_cast<double>(w) / width;
					packedTexture->setUserValue(TEX_TRANSFORM_BASECOLOR_SCALE_X, osg::clampTo(scaleX, 0.0, 1.0));
					double scaleY = static_cast<double>(h) / height;
					packedTexture->setUserValue(TEX_TRANSFORM_BASECOLOR_SCALE_Y, osg::clampTo(scaleY, 0.0, 1.0));
					packedTexture->setUserValue(TEX_TRANSFORM_BASECOLOR_TEXCOORD, 0);
					stateSetCopy->setTextureAttribute(0, packedTexture.get());
					geometry->setStateSet(stateSetCopy.get());
				}
			}
			removePackedImages(_images, deleteImgs);
		}
	}
}

osg::ref_ptr<osg::Image> GltfOptimizer::TextureAtlasBuilderVisitor::packImges(TexturePacker& packer, std::vector<osg::ref_ptr<osg::Image>>& imgs, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs)
{
	int area = _options.maxWidth * _options.maxHeight;
	std::sort(imgs.begin(), imgs.end(), GltfOptimizer::TextureAtlasBuilderVisitor::compareImageHeight);
	std::vector<size_t> indexPacks;
	osg::ref_ptr<osg::Image> packedImage;

	for (size_t i = 0; i < imgs.size(); ++i)
	{
		osg::ref_ptr<osg::Image> img = imgs.at(i);
		if (area >= img->s() * img->t())
		{
			if (deleteImgs.size())
			{
				if (deleteImgs.at(0)->getInternalTextureFormat() == img->getInternalTextureFormat() &&
					deleteImgs.at(0)->getPixelFormat() == img->getPixelFormat())
				{
					indexPacks.push_back(packer.addElement(img));
					area -= img->s() * img->t();
					deleteImgs.push_back(img);
				}
			}
			else
			{
				indexPacks.push_back(packer.addElement(img));
				area -= img->s() * img->t();
				deleteImgs.push_back(img);
			}
			if (i == (imgs.size() - 1))
			{
				packImages(packedImage, indexPacks, deleteImgs, packer);
			}
		}
		else
		{
			packImages(packedImage, indexPacks, deleteImgs, packer);
			break;
		}
	}

	return packedImage;
}

void GltfOptimizer::TextureAtlasBuilderVisitor::updateGltfMaterialUserValue(
	osg::Geometry* geometry,
	osg::ref_ptr<osg::StateSet>& stateSetCopy,
	osg::ref_ptr<GltfMaterial> packedGltfMaterial,
	TexturePacker& packer,
	const int width,
	const int height,
	const osg::ref_ptr<osg::Texture2D>& oldTexture,
	const GltfTextureType type)
{

	std::string textureNameExtension, textureOffsetXExtension, textureOffsetYExtension, textureScaleXExtension, textureScaleYExtension, textureTecCoord;
	switch (type)
	{
	case GltfTextureType::NORMAL:
		textureNameExtension = TEX_TRANSFORM_NORMAL_TEXTURE_NAME;
		textureOffsetXExtension = TEX_TRANSFORM_NORMAL_OFFSET_X;
		textureOffsetYExtension = TEX_TRANSFORM_NORMAL_OFFSET_Y;
		textureScaleXExtension = TEX_TRANSFORM_NORMAL_SCALE_X;
		textureScaleYExtension = TEX_TRANSFORM_NORMAL_SCALE_Y;
		textureTecCoord = TEX_TRANSFORM_NORMAL_TEXCOORD;
		break;
	case GltfTextureType::OCCLUSION:
		textureNameExtension = TEX_TRANSFORM_OCCLUSION_TEXTURE_NAME;
		textureOffsetXExtension = TEX_TRANSFORM_OCCLUSION_OFFSET_X;
		textureOffsetYExtension = TEX_TRANSFORM_OCCLUSION_OFFSET_Y;
		textureScaleXExtension = TEX_TRANSFORM_OCCLUSION_SCALE_X;
		textureScaleYExtension = TEX_TRANSFORM_OCCLUSION_SCALE_Y;
		textureTecCoord = TEX_TRANSFORM_OCCLUSION_TEXCOORD;
		break;
	case GltfTextureType::EMISSIVE:
		textureNameExtension = TEX_TRANSFORM_EMISSIVE_TEXTURE_NAME;
		textureOffsetXExtension = TEX_TRANSFORM_EMISSIVE_OFFSET_X;
		textureOffsetYExtension = TEX_TRANSFORM_EMISSIVE_OFFSET_Y;
		textureScaleXExtension = TEX_TRANSFORM_EMISSIVE_SCALE_X;
		textureScaleYExtension = TEX_TRANSFORM_EMISSIVE_SCALE_Y;
		textureTecCoord = TEX_TRANSFORM_EMISSIVE_TEXCOORD;
		break;
	case GltfTextureType::METALLICROUGHNESS:
		textureNameExtension = TEX_TRANSFORM_MR_TEXTURE_NAME;
		textureOffsetXExtension = TEX_TRANSFORM_MR_OFFSET_X;
		textureOffsetYExtension = TEX_TRANSFORM_MR_OFFSET_Y;
		textureScaleXExtension = TEX_TRANSFORM_MR_SCALE_X;
		textureScaleYExtension = TEX_TRANSFORM_MR_SCALE_Y;
		textureTecCoord = TEX_TRANSFORM_MR_TEXCOORD;
		break;
	case GltfTextureType::BASECOLOR:
		textureNameExtension = TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME;
		textureOffsetXExtension = TEX_TRANSFORM_BASECOLOR_OFFSET_X;
		textureOffsetYExtension = TEX_TRANSFORM_BASECOLOR_OFFSET_Y;
		textureScaleXExtension = TEX_TRANSFORM_BASECOLOR_SCALE_X;
		textureScaleYExtension = TEX_TRANSFORM_BASECOLOR_SCALE_Y;
		textureTecCoord = TEX_TRANSFORM_BASECOLOR_TEXCOORD;
		break;
	case GltfTextureType::DIFFUSE:
		textureNameExtension = TEX_TRANSFORM_DIFFUSE_TEXTURE_NAME;
		textureOffsetXExtension = TEX_TRANSFORM_DIFFUSE_OFFSET_X;
		textureOffsetYExtension = TEX_TRANSFORM_DIFFUSE_OFFSET_Y;
		textureScaleXExtension = TEX_TRANSFORM_DIFFUSE_SCALE_X;
		textureScaleYExtension = TEX_TRANSFORM_DIFFUSE_SCALE_Y;
		textureTecCoord = TEX_TRANSFORM_DIFFUSE_TEXCOORD;
		break;
	case GltfTextureType::SPECULARGLOSSINESS:
		textureNameExtension = TEX_TRANSFORM_SG_TEXTURE_NAME;
		textureOffsetXExtension = TEX_TRANSFORM_SG_OFFSET_X;
		textureOffsetYExtension = TEX_TRANSFORM_SG_OFFSET_Y;
		textureScaleXExtension = TEX_TRANSFORM_SG_SCALE_X;
		textureScaleYExtension = TEX_TRANSFORM_SG_SCALE_Y;
		textureTecCoord = TEX_TRANSFORM_SG_TEXCOORD;
		break;
	default:
		break;
	}

	int w, h;
	double x, y;
	const size_t id = packer.getId(oldTexture->getImage(0));
	if (packer.getPackingData(id, x, y, w, h))
	{
		packedGltfMaterial->setUserValue(textureNameExtension, true);
		double offsetX = x / width;
		packedGltfMaterial->setUserValue(textureOffsetXExtension, osg::clampTo(offsetX, 0.0, 1.0));
		double offsetY = y / height;
		packedGltfMaterial->setUserValue(textureOffsetYExtension, osg::clampTo(offsetY, 0.0, 1.0));
		double scaleX = static_cast<double>(w) / width;
		packedGltfMaterial->setUserValue(textureScaleXExtension, osg::clampTo(scaleX, 0.0, 1.0));
		double scaleY = static_cast<double>(h) / height;
		packedGltfMaterial->setUserValue(textureScaleYExtension, osg::clampTo(scaleY, 0.0, 1.0));
		packedGltfMaterial->setUserValue(textureTecCoord, 0);
		stateSetCopy->setAttribute(packedGltfMaterial.get(), osg::StateAttribute::MATERIAL);

		osg::ref_ptr<GltfPbrMRMaterial> packedGltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(packedGltfMaterial.get());
		geometry->setStateSet(stateSetCopy.get());
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::processGltfGeneralImages(std::vector<osg::ref_ptr<osg::Image>>& imgs, const GltfTextureType type)
{
	removeRepeatImages(imgs);

	while (imgs.size())
	{
		TexturePacker packer(_options.maxTextureAtlasWidth, _options.maxTextureAtlasHeight);
		std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
		osg::ref_ptr<osg::Image> packedImage = packImges(packer, imgs, deleteImgs);

		if (packedImage.valid() && deleteImgs.size())
		{
			const int width = packedImage->s(), height = packedImage->t();
			bool bResizeImage = resizeImageToPowerOfTwo(packedImage);
			exportImage(packedImage);
			for (auto& entry : _geometryMatMap)
			{
				osg::Geometry* geometry = entry.first;
				osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
				osg::ref_ptr<GltfMaterial> olgMaterial = dynamic_cast<GltfMaterial*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
				GltfMaterial* gltfMaterial = entry.second;
				if (olgMaterial.get() != gltfMaterial)
					continue;

				osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_STATESETS);
				osg::ref_ptr<GltfMaterial> packedGltfMaterial = dynamic_cast<GltfMaterial*>(osg::clone(stateSetCopy->getAttribute(osg::StateAttribute::MATERIAL), osg::CopyOp::DEEP_COPY_STATEATTRIBUTES));

				osg::ref_ptr<osg::Texture2D> packedTexture, oldTexture;
				if (type == GltfTextureType::NORMAL)
				{
					packedTexture = osg::clone(packedGltfMaterial->normalTexture.get(), osg::CopyOp::DEEP_COPY_TEXTURES);
					packedGltfMaterial->normalTexture = packedTexture;
				}
				else if (type == GltfTextureType::OCCLUSION)
				{
					packedTexture = osg::clone(packedGltfMaterial->occlusionTexture.get(), osg::CopyOp::DEEP_COPY_TEXTURES);
					packedGltfMaterial->occlusionTexture = packedTexture;
				}
				else if (type == GltfTextureType::EMISSIVE)
				{
					packedTexture = osg::clone(packedGltfMaterial->emissiveTexture.get(), osg::CopyOp::DEEP_COPY_TEXTURES);
					packedGltfMaterial->emissiveTexture = packedTexture;
				}

				if (packedTexture.valid() && packedTexture->getNumImages())
				{

					packedTexture->setImage(packedImage);
					if (type == GltfTextureType::NORMAL)
					{
						oldTexture = gltfMaterial->normalTexture;
					}
					else if (type == GltfTextureType::OCCLUSION)
					{
						oldTexture = gltfMaterial->occlusionTexture;
					}
					else if (type == GltfTextureType::EMISSIVE)
					{
						oldTexture = gltfMaterial->emissiveTexture;
					}
					if (oldTexture.valid() && oldTexture->getNumImages())
					{
						updateGltfMaterialUserValue(geometry, stateSetCopy, packedGltfMaterial, packer, width, height, oldTexture, type);
					}

				}
			}
			removePackedImages(imgs, deleteImgs);
		}
	}
}

void osgGISPlugins::GltfOptimizer::TextureAtlasBuilderVisitor::removeRepeatImages(std::vector<osg::ref_ptr<osg::Image>>& imgs)
{
	std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
	for (osg::ref_ptr<osg::Image> img : imgs)
	{
		std::string ext = _options.ext;
		std::string filename = computeImageHash(img);
		const GLenum pixelFormat = img->getPixelFormat();
		if (_options.ext == ".jpg" && pixelFormat != GL_ALPHA && pixelFormat != GL_RGB)
		{
			ext = ".png";
		}
		const std::string fullPath = _options.cachePath + "/" + filename + ext;
		if (osgDB::fileExists(fullPath))
		{
			deleteImgs.push_back(img);
		}
	}
	removePackedImages(imgs, deleteImgs);
}

void GltfOptimizer::TextureAtlasBuilderVisitor::addImageFromTexture(const osg::ref_ptr<osg::Texture2D>& texture, std::vector<osg::ref_ptr<osg::Image>>& imgs)
{
	if (texture.valid())
	{
		if (texture->getNumImages())
		{
			bool bAdd = true;
			osg::ref_ptr<osg::Image> image = texture->getImage(0);
			for (osg::ref_ptr<osg::Image> img : imgs)
			{
				if (img == image)
					bAdd = false;
			}
			if (bAdd)
				imgs.push_back(image);
		}
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::removePackedImages(std::vector<osg::ref_ptr<osg::Image>>& imgs, const std::vector<osg::ref_ptr<osg::Image>>& deleteImgs)
{
	for (osg::ref_ptr<osg::Image> img : deleteImgs)
	{
		auto it = std::remove(imgs.begin(), imgs.end(), img.get());
		imgs.erase(it, imgs.end());
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::processGltfPbrMRImages(std::vector<osg::ref_ptr<osg::Image>>& imageList, const GltfTextureType type)
{
	removeRepeatImages(imageList);

	while (imageList.size())
	{
		TexturePacker packer(_options.maxTextureAtlasWidth, _options.maxTextureAtlasHeight);
		std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
		osg::ref_ptr<osg::Image> packedImage = packImges(packer, imageList, deleteImgs);

		if (packedImage.valid() && deleteImgs.size())
		{
			const int width = packedImage->s(), height = packedImage->t();
			bool bResizeImage = resizeImageToPowerOfTwo(packedImage);
			exportImage(packedImage);

			for (auto it = _geometryMatMap.begin(); it != _geometryMatMap.end(); ++it)
			{
				osg::Geometry* geometry = it->first;
				osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
				if (!stateSet.valid())
					continue;
				osg::ref_ptr<GltfPbrMRMaterial> olgMaterial = dynamic_cast<GltfPbrMRMaterial*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
				GltfMaterial* gltfMaterial = it->second;
				if (olgMaterial.get() != gltfMaterial)
					continue;

				GltfPbrMRMaterial* gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(gltfMaterial);
				const osg::Texture2D* stateSetTexture = ((type == GltfTextureType::METALLICROUGHNESS) ? gltfMRMaterial->metallicRoughnessTexture : gltfMRMaterial->baseColorTexture).get();
				if (stateSetTexture == nullptr)
					continue;

				osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_STATESETS);
				const osg::StateAttribute* stateSetCopyMaterial = stateSetCopy->getAttribute(osg::StateAttribute::MATERIAL);
				if (stateSetCopyMaterial == nullptr)
					continue;
				osg::ref_ptr<GltfPbrMRMaterial> packedGltfMaterial = dynamic_cast<GltfPbrMRMaterial*>(osg::clone(stateSetCopyMaterial, osg::CopyOp::DEEP_COPY_STATEATTRIBUTES));

				const osg::Texture2D* stateSetCopyTexture = ((type == GltfTextureType::METALLICROUGHNESS) ? packedGltfMaterial->metallicRoughnessTexture : packedGltfMaterial->baseColorTexture).get();
				osg::ref_ptr<osg::Texture2D> texture = osg::clone(stateSetCopyTexture, osg::CopyOp::DEEP_COPY_TEXTURES);
				if (type == GltfTextureType::METALLICROUGHNESS)
					packedGltfMaterial->metallicRoughnessTexture = texture;
				else
					packedGltfMaterial->baseColorTexture = texture;

				if (texture.valid() && texture->getNumImages())
				{
					texture->setImage(packedImage);

					osg::ref_ptr<osg::Texture2D> originalTexture = (type == GltfTextureType::METALLICROUGHNESS) ? gltfMRMaterial->metallicRoughnessTexture : gltfMRMaterial->baseColorTexture;

					if (originalTexture.valid() && originalTexture->getNumImages())
					{
						updateGltfMaterialUserValue(geometry, stateSetCopy, packedGltfMaterial, packer, width, height, originalTexture, type);
					}

				}
			}
			removePackedImages(imageList, deleteImgs);
		}
	}


}

void GltfOptimizer::TextureAtlasBuilderVisitor::processGltfPbrSGImages(std::vector<osg::ref_ptr<osg::Image>>& images, const GltfTextureType type)
{
	removeRepeatImages(images);
	while (images.size())
	{
		TexturePacker packer(_options.maxTextureAtlasWidth, _options.maxTextureAtlasHeight);
		std::vector<osg::ref_ptr<osg::Image>> deleteImgs;
		osg::ref_ptr<osg::Image> packedImage = packImges(packer, images, deleteImgs);

		if (packedImage.valid() && deleteImgs.size())
		{
			const int width = packedImage->s(), height = packedImage->t();
			bool bResizeImage = resizeImageToPowerOfTwo(packedImage);
			exportImage(packedImage);
			for (auto& entry : _geometryMatMap)
			{
				osg::Geometry* geometry = entry.first;
				osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
				osg::ref_ptr<GltfMaterial> olgMaterial = dynamic_cast<GltfMaterial*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
				GltfMaterial* gltfMaterial = entry.second;
				if (olgMaterial.get() != gltfMaterial)
					continue;

				osg::ref_ptr<osg::StateSet> stateSetCopy = osg::clone(stateSet.get(), osg::CopyOp::DEEP_COPY_STATESETS);
				osg::ref_ptr<GltfPbrMRMaterial> packedGltfMaterial = dynamic_cast<GltfPbrMRMaterial*>(osg::clone(stateSetCopy->getAttribute(osg::StateAttribute::MATERIAL), osg::CopyOp::DEEP_COPY_STATEATTRIBUTES));

				for (size_t i = 0; i < packedGltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
				{
					GltfExtension* packedExtension = packedGltfMaterial->materialExtensionsByCesiumSupport.at(i);
					for (size_t j = 0; j < gltfMaterial->materialExtensionsByCesiumSupport.size(); ++j)
					{
						GltfExtension* extension = gltfMaterial->materialExtensionsByCesiumSupport.at(j);
						if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness) && typeid(*packedExtension) == typeid(KHR_materials_pbrSpecularGlossiness))
						{

							KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
							KHR_materials_pbrSpecularGlossiness* packed_pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(packedExtension);

							osg::ref_ptr<osg::Texture2D> osgTexture = (type == GltfTextureType::DIFFUSE) ? pbrSpecularGlossiness_extension->osgDiffuseTexture : pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture;
							osg::ref_ptr<osg::Texture2D> packedOsgTexture = osg::clone(((type == GltfTextureType::DIFFUSE) ? packed_pbrSpecularGlossiness_extension->osgDiffuseTexture : packed_pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture).get(), osg::CopyOp::DEEP_COPY_TEXTURES);
							if (type == GltfTextureType::DIFFUSE)
								packed_pbrSpecularGlossiness_extension->osgDiffuseTexture = packedOsgTexture;
							else
								packed_pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture = packedOsgTexture;


							if (osgTexture.valid() && packedOsgTexture.valid())
							{
								if (osgTexture->getNumImages() && packedOsgTexture->getNumImages())
								{
									packedOsgTexture->setImage(packedImage);

									updateGltfMaterialUserValue(geometry, stateSetCopy, packedGltfMaterial, packer, width, height, osgTexture, type);
								}
							}
						}
					}
				}

			}
			removePackedImages(images, deleteImgs);
		}
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::packOsgMaterials()
{
	if (_gltfMaterials.empty()) return;
	std::vector<osg::ref_ptr<osg::Image>> normalImgs, occlusionImgs, emissiveImgs, mrImgs, baseColorImgs, diffuseImgs, sgImgs;
	for (auto* material : _gltfMaterials)
	{
		addImageFromTexture(material->normalTexture, normalImgs);
		addImageFromTexture(material->occlusionTexture, occlusionImgs);
		addImageFromTexture(material->emissiveTexture, emissiveImgs);

		osg::ref_ptr<GltfPbrMRMaterial> gltfMRMaterial = dynamic_cast<GltfPbrMRMaterial*>(material);
		if (gltfMRMaterial.valid())
		{
			addImageFromTexture(gltfMRMaterial->metallicRoughnessTexture, mrImgs);
			addImageFromTexture(gltfMRMaterial->baseColorTexture, baseColorImgs);
		}
		for (size_t i = 0; i < material->materialExtensionsByCesiumSupport.size(); ++i)
		{
			GltfExtension* extension = material->materialExtensionsByCesiumSupport.at(i);
			if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness))
			{
				KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness*>(extension);
				addImageFromTexture(pbrSpecularGlossiness_extension->osgDiffuseTexture, diffuseImgs);
				addImageFromTexture(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture, sgImgs);
			}
		}
	}

	processGltfGeneralImages(normalImgs, GltfTextureType::NORMAL);
	processGltfGeneralImages(occlusionImgs, GltfTextureType::OCCLUSION);
	processGltfGeneralImages(emissiveImgs, GltfTextureType::EMISSIVE);

	processGltfPbrMRImages(mrImgs, GltfTextureType::METALLICROUGHNESS);
	processGltfPbrMRImages(baseColorImgs, GltfTextureType::BASECOLOR);

	processGltfPbrSGImages(diffuseImgs, GltfTextureType::DIFFUSE);

	processGltfPbrSGImages(diffuseImgs, GltfTextureType::SPECULARGLOSSINESS);

	normalImgs.clear();
	occlusionImgs.clear();
	emissiveImgs.clear();
	mrImgs.clear();
	baseColorImgs.clear();
	diffuseImgs.clear();
	sgImgs.clear();
}

void GltfOptimizer::TextureAtlasBuilderVisitor::packImages(osg::ref_ptr<osg::Image>& img, std::vector<size_t>& indexes, std::vector<osg::ref_ptr<osg::Image>>& deleteImgs, TexturePacker& packer)
{
	size_t numImages;
	img = packer.pack(numImages, true);
	if (!img.valid() || numImages != indexes.size())
	{
		packer.removeElement(indexes.back());
		indexes.pop_back();
		deleteImgs.pop_back();
		packImages(img, indexes, deleteImgs, packer);
	}
}

void GltfOptimizer::TextureAtlasBuilderVisitor::exportImage(const osg::ref_ptr<osg::Image>& img)
{
	std::string ext = _options.ext;
	std::string filename = computeImageHash(img);
	const GLenum pixelFormat = img->getPixelFormat();
	if (_options.ext == ".jpg" && pixelFormat != GL_ALPHA && pixelFormat != GL_RGB)
	{
		ext = ".png";
	}

	osg::ref_ptr<osgDB::Options> options = new osgDB::Options;
	if (ext == ".jpg" || ext == ".jpeg")
		options->setOptionString("JPEG_QUALITY  75");
	else if (ext == ".png")
		options->setOptionString("PNG_COMPRESSION 5");


	std::string fullPath = _options.cachePath + "/" + filename + ext;
	bool isFileExists = osgDB::fileExists(fullPath);
	if (!isFileExists)
	{
		osg::ref_ptr< osg::Image > flipped = new osg::Image(*img);
		if (!(osgDB::writeImageFile(*flipped.get(), fullPath)))
		{
			fullPath = _options.cachePath + "/" + filename + ".png";
			options->setOptionString("PNG_COMPRESSION 5");
			std::ifstream fileExistedPng(fullPath);
			if ((!fileExistedPng.good()) || (fileExistedPng.peek() == std::ifstream::traits_type::eof()))
			{
				if (!(osgDB::writeImageFile(*flipped.get(), fullPath)))
				{
					osg::notify(osg::FATAL) << '\n';
				}
			}
			fileExistedPng.close();
		}
		flipped = nullptr;
	}
	img->setUserValue(BASECOLOR_TEXTURE_FILENAME, fullPath);
}

bool GltfOptimizer::TextureAtlasBuilderVisitor::resizeImageToPowerOfTwo(const osg::ref_ptr<osg::Image>& img)
{
	int originalWidth = img->s();
	int originalHeight = img->t();
	int newWidth = findNearestPowerOfTwo(originalWidth);
	int newHeight = findNearestPowerOfTwo(originalHeight);

	newWidth = newWidth > _options.maxWidth ? _options.maxWidth : newWidth;
	newHeight = newHeight > _options.maxHeight ? _options.maxHeight : newHeight;
	img->scaleImage(newWidth, newHeight, img->r());
	return true;
}

int GltfOptimizer::TextureAtlasBuilderVisitor::findNearestPowerOfTwo(int value)
{
	int powerOfTwo = 1;
	while (powerOfTwo * 2 <= value)
	{
		powerOfTwo *= 2;
	}
	return powerOfTwo;
}

std::string GltfOptimizer::TextureAtlasBuilderVisitor::computeImageHash(const osg::ref_ptr<osg::Image>& image)
{
	size_t hash = 0;
	auto hash_combine = [&hash](size_t value)
		{
			hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		};

	hash_combine(std::hash<int>()(image->s()));
	hash_combine(std::hash<int>()(image->t()));
	hash_combine(std::hash<int>()(image->r()));
	hash_combine(std::hash<int>()(image->getPixelFormat()));
	hash_combine(std::hash<int>()(image->getDataType()));

	const unsigned char* data = image->data();
	size_t dataSize = image->getImageSizeInBytes();

	for (size_t i = 0; i < dataSize; ++i)
	{
		hash_combine(std::hash<unsigned char>()(data[i]));
	}
	std::ostringstream oss;
	oss << std::hex << std::setw(16) << std::setfill('0') << hash;
	oss.clear();
	return oss.str();
}

bool GltfOptimizer::TextureAtlasBuilderVisitor::compareImageHeight(osg::ref_ptr<osg::Image> img1, osg::ref_ptr<osg::Image> img2)
{
	if (img1->t() == img2->t())
	{
		return img1->s() > img2->s();
	}
	return img1->t() > img2->t();
}
