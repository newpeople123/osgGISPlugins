#include "3dtiles/optimizer/mesh/MeshOptimizer.h"
#include <osg/PrimitiveSet>
#include <meshoptimizer.h>
#include <osgUtil/Optimizer>
void MeshOptimizer::reindexMesh(osg::ref_ptr<osg::Geometry> geom)
{
	if (geom.valid()) {
		const unsigned int psetCount = geom->getNumPrimitiveSets();
		if (psetCount > 0) {

			osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
			osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
			osg::ref_ptr<osg::Vec2Array> texCoords = nullptr;
			if (geom->getNumTexCoordArrays())
				texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
			osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geom->getVertexAttribArray(0));
			for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
				osg::ref_ptr<osg::DrawElements> drawElements = dynamic_cast<osg::DrawElements*>(geom->getPrimitiveSet(primIndex));
				if (drawElements.valid() && positions.valid()) {
					const unsigned int indiceCount = drawElements->getNumIndices();
					osg::MixinVector<meshopt_Stream> streams;
					struct Attr
					{
						float f[4];
					};
					const size_t positionCount = positions->size();
					osg::MixinVector<Attr> vertexData, normalData, texCoordData, batchIdData;
					for (size_t i = 0; i < positionCount; ++i)
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
						if (batchIds.valid())
						{
							const float batchId = batchIds->at(i);
							Attr b;
							b.f[0] = batchId;
							batchIdData.push_back(b);
						}
					}
					meshopt_Stream vertexStream = { vertexData.asVector().data(), sizeof(Attr), sizeof(Attr)};
					streams.push_back(vertexStream);
					if (normals.valid()) {
						meshopt_Stream normalStream = { normalData.asVector().data(), sizeof(Attr), sizeof(Attr) };
						streams.push_back(normalStream);
						if (positions->size() != normals->size())
						{
							continue;
						}
					}
					if (texCoords.valid()) {
						meshopt_Stream texCoordStream = { texCoordData.asVector().data(), sizeof(Attr), sizeof(Attr) };
						streams.push_back(texCoordStream);
						if (positions->size() != texCoords->size())
						{
							continue;
						}
					}

					osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(geom->getPrimitiveSet(primIndex));
					if (drawElementsUShort.valid()) {
						osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
						for (size_t i = 0; i < indiceCount; ++i)
						{
							indices->push_back(drawElementsUShort->at(i));
						}
						osg::MixinVector<unsigned int> remap(positions->size());
						size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(remap.asVector().data(), &(*indices)[0], indices->size(), positions->size(),
							streams.asVector().data(), streams.size());
						osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(uniqueVertexCount);
						osg::ref_ptr<osg::FloatArray> optimizedBatchIds = new osg::FloatArray(uniqueVertexCount);
						osg::ref_ptr<osg::UShortArray> optimizedIndices = new osg::UShortArray(indices->size());
						meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), remap.asVector().data());
						meshopt_remapVertexBuffer(vertexData.asVector().data(), vertexData.asVector().data(), positions->size(), sizeof(Attr),
							remap.asVector().data());
						vertexData.resize(uniqueVertexCount);
						if (normals.valid()) {
							meshopt_remapVertexBuffer(normalData.asVector().data(), normalData.asVector().data(), normals->size(), sizeof(Attr),
								remap.asVector().data());
							normalData.resize(uniqueVertexCount);
						}
						if (texCoords.valid()) {
							meshopt_remapVertexBuffer(texCoordData.asVector().data(), texCoordData.asVector().data(), texCoords->size(), sizeof(Attr),
								remap.asVector().data());
							texCoordData.resize(uniqueVertexCount);
						}
						if (batchIds.valid())
						{
							meshopt_remapVertexBuffer(batchIdData.asVector().data(), batchIdData.asVector().data(), batchIds->size(), sizeof(Attr),
								remap.asVector().data());
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
						size_t newIndiceCount = 0;

						for (size_t i = 0; i < indiceCount; i += 3) {
							unsigned short a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);

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

						geom->setPrimitiveSet(primIndex, new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));

					}
					else {
						osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(geom->getPrimitiveSet(primIndex));
						osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
						for (unsigned int i = 0; i < indiceCount; ++i)
						{
							indices->push_back(drawElementsUInt->at(i));
						}
						osg::MixinVector<unsigned int> remap(positions->size());
						size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(remap.asVector().data(), &(*indices)[0], indices->size(), positions->size(),
							streams.asVector().data(), streams.size());

						//size_t uniqueVertexCount = meshopt_generateVertexRemap(&remap[0], &(*indices)[0], indices->size(), &(*positions)[0].x(), positions->size(), sizeof(osg::Vec3));
						osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(uniqueVertexCount);
						osg::ref_ptr<osg::FloatArray> optimizedBatchIds = new osg::FloatArray(uniqueVertexCount);
						osg::ref_ptr<osg::UIntArray> optimizedIndices = new osg::UIntArray(indices->size());
						meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), remap.asVector().data());
						meshopt_remapVertexBuffer(vertexData.asVector().data(), vertexData.asVector().data(), positions->size(), sizeof(Attr),
							remap.asVector().data());
						vertexData.resize(uniqueVertexCount);
						if (normals.valid()) {
							meshopt_remapVertexBuffer(normalData.asVector().data(), normalData.asVector().data(), normals->size(), sizeof(Attr),
								remap.asVector().data());
							normalData.resize(uniqueVertexCount);
						}
						if (texCoords.valid()) {
							meshopt_remapVertexBuffer(texCoordData.asVector().data(), texCoordData.asVector().data(), texCoords->size(), sizeof(Attr),
								remap.asVector().data());
							texCoordData.resize(uniqueVertexCount);
						}
						if (batchIds.valid())
						{
							meshopt_remapVertexBuffer(batchIdData.asVector().data(), batchIdData.asVector().data(), batchIds->size(), sizeof(Attr),
								remap.asVector().data());
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
						size_t newIndiceCount = 0;
						for (size_t i = 0; i < indiceCount; i += 3) {
							unsigned int a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);

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
						geom->setPrimitiveSet(primIndex, new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));
					}

				}
			}
		}
	}
}

void MeshOptimizer::simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio)
{
	const bool aggressive = false;
	const bool lockBorders = false;

	if (geom.valid()) {
		const float target_error = 1e-2f;
		const float target_error_aggressive = 1e-1f;
		const unsigned int options = lockBorders ? meshopt_SimplifyLockBorder : 0;

		const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
		const unsigned int psetCount = geom->getNumPrimitiveSets();
		if (psetCount > 0) {
			for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
				osg::ref_ptr<osg::DrawElements> drawElements = dynamic_cast<osg::DrawElements*>(geom->getPrimitiveSet(primIndex));
				if (drawElements.valid() && positions.valid()) {
					const unsigned int indiceCount = drawElements->getNumIndices();
					const unsigned int positionCount = positions->size();
					std::vector<float> vertices(positionCount * 3);
					for (size_t i = 0; i < positionCount; ++i)
					{
						const osg::Vec3& vertex = positions->at(i);
						vertices[i * 3] = vertex.x();
						vertices[i * 3 + 1] = vertex.y();
						vertices[i * 3 + 2] = vertex.z();

					}

					osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(geom->getPrimitiveSet(primIndex));
					if (drawElementsUShort.valid()) {
						osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
						for (unsigned int i = 0; i < indiceCount; ++i)
						{
							indices->push_back(drawElementsUShort->at(i));
						}
						osg::ref_ptr<osg::UShortArray> destination = new osg::UShortArray;
						destination->resize(indiceCount);
						const unsigned int targetIndexCount = indiceCount * simplifyRatio;
						size_t newLength = meshopt_simplify(&(*destination)[0], &(*indices)[0], static_cast<size_t>(indiceCount), vertices.data(), static_cast<size_t>(positionCount), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error, options);
						if (aggressive && newLength > targetIndexCount)
							newLength = meshopt_simplifySloppy(&(*destination)[0], &(*indices)[0], static_cast<size_t>(indiceCount), vertices.data(), static_cast<size_t>(positionCount), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error_aggressive);

						if (newLength > 0) {
							osg::ref_ptr<osg::UShortArray> newIndices = new osg::UShortArray;

							for (size_t i = 0; i < static_cast<size_t>(newLength); ++i)
							{
								newIndices->push_back(destination->at(i));
							}
							geom->setPrimitiveSet(primIndex, new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, newIndices->size(), &(*newIndices)[0]));
						}
						else
						{
							geom->removePrimitiveSet(primIndex);
							primIndex--;
						}
					}
					else {
						const osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(geom->getPrimitiveSet(primIndex));
						osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
						for (unsigned int i = 0; i < indiceCount; ++i)
						{
							indices->push_back(drawElementsUInt->at(i));
						}
						osg::ref_ptr<osg::UIntArray> destination = new osg::UIntArray;
						destination->resize(indiceCount);
						const unsigned int targetIndexCount = indiceCount * simplifyRatio;
						size_t newLength = meshopt_simplify(&(*destination)[0], &(*indices)[0], static_cast<size_t>(indiceCount), vertices.data(), static_cast<size_t>(positionCount), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error, options);
						if (aggressive && newLength > targetIndexCount)
							newLength = meshopt_simplifySloppy(&(*destination)[0], &(*indices)[0], static_cast<size_t>(indiceCount), vertices.data(), static_cast<size_t>(positionCount), (size_t)(sizeof(float) * 3), static_cast<size_t>(targetIndexCount), target_error_aggressive);

						osg::ref_ptr<osg::UIntArray> newIndices = new osg::UIntArray;
						if (newLength > 0) {
							for (size_t i = 0; i < static_cast<size_t>(newLength); ++i)
							{
								newIndices->push_back(destination->at(i));
							}
							geom->setPrimitiveSet(primIndex, new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, newIndices->size(), &(*newIndices)[0]));
						}
						else
						{
							geom->removePrimitiveSet(primIndex);
							primIndex--;
						}
					}

				}
			}
		}
	}


}
