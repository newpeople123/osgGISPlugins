#include "3dtiles/optimizer/MeshOptimizer.h"
#include <meshoptimizer.h>
MeshOptimizer::MeshOptimizer(MeshSimplifierBase* meshSimplifier, float simplifyRatio)
	: _meshSimplifier(meshSimplifier), _simplifyRatio(simplifyRatio), osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

void MeshOptimizer::apply(osg::Geometry& geometry)
{
	//顺序很重要
	//1
	_meshSimplifier->reindexMesh(&geometry);
	//2
	if (_simplifyRatio < 1.0)
		_meshSimplifier->simplifyMesh(&geometry, _simplifyRatio);
	//3
	vertexCacheOptimize(geometry);
	//4
	overdrawOptimize(geometry);
	//5
	vertexFetchOptimize(geometry);
	//6
	_meshSimplifier->mergePrimitives(&geometry);

}

void MeshOptimizer::vertexCacheOptimize(osg::Geometry& geometry)
{
	const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
	if (!positions) return;
	const size_t vertexCount = positions->size();
	const unsigned int psetCount = geometry.getNumPrimitiveSets();
	if (psetCount <= 0) return;

	for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
		osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
		const osg::PrimitiveSet::Type type = pset->getType();
		if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
			osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
			processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
			if (indices->empty()) continue;
			meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
			auto* drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
			std::copy(indices->begin(), indices->end(), drawElements->begin());
		}
		else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {
			osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
			processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
			if (indices->empty()) continue;
			meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
			auto* drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
			std::copy(indices->begin(), indices->end(), drawElements->begin());
		}
		else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
			osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
			processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
			if (indices->empty()) continue;
			meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
			auto* drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
			std::copy(indices->begin(), indices->end(), drawElements->begin());
		}
	}
}

void MeshOptimizer::overdrawOptimize(osg::Geometry& geometry)
{
}

void MeshOptimizer::vertexFetchOptimize(osg::Geometry& geometry)
{
}

template <typename DrawElementsType, typename IndexArrayType>
void MeshOptimizer::processDrawElements(osg::PrimitiveSet* pset, IndexArrayType& indices)
{
	auto* drawElements = dynamic_cast<DrawElementsType*>(pset);
	if (drawElements) {
		indices.insert(indices.end(), drawElements->begin(), drawElements->end());
	}
}
