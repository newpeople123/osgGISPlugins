#include "3dtiles/optimizer/MeshOptimizer.h"
#include <osg/PrimitiveSet>
#include <meshoptimizer.h>
#include <osgUtil/Optimizer>
#include <iostream>
void MeshOptimizer::reindexMesh(osg::ref_ptr<osg::Geometry> geom)
{
	if (geom.valid()) {
		const unsigned int psetCount = geom->getNumPrimitiveSets();
		if (psetCount <= 0) return;
		for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
			osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(primIndex);
			if (typeid(*pset.get()) == typeid(osg::DrawElementsUShort)) {
				osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
				reindexPrimitiveSet<osg::DrawElementsUShort, osg::UShortArray>(geom, drawElementsUShort, primIndex);
			}
			else if (typeid(*pset.get()) == typeid(osg::DrawElementsUInt)) {
				osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
				reindexPrimitiveSet<osg::DrawElementsUInt, osg::UIntArray>(geom, drawElementsUInt, primIndex);
			}
		}
	}
}

template<typename DrawElementsType, typename IndexArrayType>
void MeshOptimizer::reindexPrimitiveSet(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex) {
	osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geom->getVertexAttribArray(0));
	osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
	osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
	osg::ref_ptr<osg::Vec2Array> texCoords = nullptr;
	if (geom->getNumTexCoordArrays())
		texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));

	if (!positions || (!normals && geom->getNormalArray()) || (!texCoords && geom->getNumTexCoordArrays())) {
		return;
	}

	if (positions->size() != normals->size() || positions->size() != texCoords->size()) {
		return;
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

		if (normals.valid()) {
			const osg::Vec3& normal = normals->at(i);
			Attr n = { normal.x(), normal.y(), normal.z(), 0.0f };
			normalData.push_back(n);
		}
		if (texCoords.valid()) {
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
	if (normals.valid()) {
		meshopt_Stream normalStream = { normalData.asVector().data(), sizeof(Attr), sizeof(Attr) };
		streams.push_back(normalStream);
	}
	if (texCoords.valid()) {
		meshopt_Stream texCoordStream = { texCoordData.asVector().data(), sizeof(Attr), sizeof(Attr) };
		streams.push_back(texCoordStream);
	}

	for (size_t i = 0; i < indiceCount; ++i)
	{
		indices->push_back(drawElements->at(i));
	}

	osg::MixinVector<unsigned int> remap(positions->size());
	size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(&remap.asVector()[0], &(*indices)[0], indices->size(), count,
		&streams.asVector()[0], streams.size());
	if (uniqueVertexCount > count) return;

	osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(uniqueVertexCount);
	osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(uniqueVertexCount);
	osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(uniqueVertexCount);
	osg::ref_ptr<osg::FloatArray> optimizedBatchIds = new osg::FloatArray(uniqueVertexCount);
	osg::ref_ptr<IndexArrayType> optimizedIndices = new IndexArrayType(indices->size());

	meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), &remap.asVector()[0]);
	meshopt_remapVertexBuffer(&vertexData.asVector()[0], &vertexData.asVector()[0], count, sizeof(Attr), &remap.asVector()[0]);
	vertexData.resize(uniqueVertexCount);

	if (normals.valid()) {
		meshopt_remapVertexBuffer(&normalData.asVector()[0], &normalData.asVector()[0], count, sizeof(Attr), &remap.asVector()[0]);
		normalData.resize(uniqueVertexCount);
	}
	if (texCoords.valid()) {
		meshopt_remapVertexBuffer(&texCoordData.asVector()[0], &texCoordData.asVector()[0], count, sizeof(Attr), &remap.asVector()[0]);
		texCoordData.resize(uniqueVertexCount);
	}
	if (batchIds.valid())
	{
		meshopt_remapVertexBuffer(&batchIdData.asVector()[0], &batchIdData.asVector()[0], count, sizeof(Attr), &remap.asVector()[0]);
		batchIdData.resize(uniqueVertexCount);
	}

	for (size_t i = 0; i < uniqueVertexCount; ++i) {
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

	geom->setVertexArray(optimizedVertices);
	if (normals.valid())
		geom->setNormalArray(optimizedNormals, osg::Array::BIND_PER_VERTEX);
	if (texCoords.valid())
		geom->setTexCoordArray(0, optimizedTexCoords);
	if (batchIds.valid())
		geom->setVertexAttribArray(0, optimizedBatchIds);

#pragma region filterTriangles
	size_t newIndiceCount = 0;
	for (size_t i = 0; i < indiceCount; i += 3) {
		const auto a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);
		if (a != b && a != c && b != c)
		{
			optimizedIndices->at(newIndiceCount) = a;
			optimizedIndices->at(newIndiceCount + 1) = b;
			optimizedIndices->at(newIndiceCount + 2) = c;
			newIndiceCount += 3;
		}
	}
	optimizedIndices->resize(newIndiceCount);
#pragma endregion

	geom->setPrimitiveSet(psetIndex, new DrawElementsType(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));

}

void MeshOptimizer::simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio)
{
	if (!geom.valid()) return;
	const unsigned int psetCount = geom->getNumPrimitiveSets();
	for (unsigned int primIndex = 0; primIndex < psetCount; ++primIndex) {
		osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(primIndex);
		if (typeid(*pset.get()) == typeid(osg::DrawElementsUShort)) {
			osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
			simplifyPrimitiveSet<osg::DrawElementsUShort, osg::UShortArray>(geom, drawElementsUShort, simplifyRatio, primIndex);
		}else if (typeid(*pset.get()) == typeid(osg::DrawElementsUInt)) {
			osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
			simplifyPrimitiveSet<osg::DrawElementsUInt, osg::UIntArray>(geom, drawElementsUInt, simplifyRatio, primIndex);
		}
	}


}

template<typename DrawElementsType, typename IndexArrayType>
void MeshOptimizer::simplifyPrimitiveSet(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const float simplifyRatio, unsigned int& psetIndex)
{
	const bool aggressive = false;
	const bool lockBorders = false;
	const float target_error = 1e-2f;
	const float target_error_aggressive = 1e-1f;
	const unsigned int options = lockBorders ? meshopt_SimplifyLockBorder : 0;

	const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
	if (!positions.valid()) return;

	const unsigned int indiceCount = drawElements->getNumIndices();
	const unsigned int positionCount = positions->size();
	std::vector<float> vertices(positionCount * 3);
	for (size_t i = 0; i < positionCount; ++i) {
		const osg::Vec3& vertex = positions->at(i);
		vertices[i * 3] = vertex.x();
		vertices[i * 3 + 1] = vertex.y();
		vertices[i * 3 + 2] = vertex.z();
	}

	osg::ref_ptr<IndexArrayType> indices = new IndexArrayType;
	for (unsigned int i = 0; i < indiceCount; ++i) {
		indices->push_back(drawElements->at(i));
	}

	osg::ref_ptr<IndexArrayType> destination = new IndexArrayType;
	destination->resize(indiceCount);
	const unsigned int targetIndexCount = indiceCount * simplifyRatio;
	size_t newLength = meshopt_simplify(&(*destination)[0], &(*indices)[0], static_cast<size_t>(indiceCount), vertices.data(), static_cast<size_t>(positionCount), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error, options);
	if (aggressive && newLength > targetIndexCount) {
		newLength = meshopt_simplifySloppy(&(*destination)[0], &(*indices)[0], static_cast<size_t>(indiceCount), vertices.data(), static_cast<size_t>(positionCount), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error_aggressive);
	}

	if (newLength > 0) {
		osg::ref_ptr<IndexArrayType> newIndices = new IndexArrayType;
		for (size_t i = 0; i < static_cast<size_t>(newLength); ++i) {
			newIndices->push_back(destination->at(i));
		}
		geom->setPrimitiveSet(psetIndex, new DrawElementsType(osg::PrimitiveSet::TRIANGLES, newIndices->size(), &(*newIndices)[0]));
	}
	else {
		geom->removePrimitiveSet(psetIndex);
		psetIndex--;
	}
}
