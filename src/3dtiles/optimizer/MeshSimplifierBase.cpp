#include "3dtiles/optimizer/MeshSimplifierBase.h"
void MeshSimplifierBase::mergePrimitives(osg::ref_ptr<osg::Geometry> geom) {
	if (geom.valid()) {
		osg::ref_ptr<osg::PrimitiveSet> mergePrimitiveset = nullptr;
		const unsigned int psetCount = geom->getNumPrimitiveSets();
		for (size_t i = 0; i < psetCount; ++i) {
			osg::PrimitiveSet* pset = geom->getPrimitiveSet(i);;
			const osg::PrimitiveSet::Type type = pset->getType();
			switch (type)
			{
			case osg::PrimitiveSet::PrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArraysPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArrayLengthsPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUBytePrimitiveType:
				if (mergePrimitiveset == nullptr) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					const auto primitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(pset);
					const osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					if (mergeType == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
						const auto mergePrimitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(mergePrimitiveset.get());
						mergePrimitiveUByte->insert(mergePrimitiveUByte->end(), primitiveUByte->begin(), primitiveUByte->end());
					}
					else {
						const auto mergePrimitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(mergePrimitiveset.get());
						if (mergeType == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {

							for (size_t k = 0; k < primitiveUByte->getNumIndices(); ++k) {
								unsigned short index = primitiveUByte->at(k);
								mergePrimitiveUShort->push_back(index);
							}
						}
						else if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
							const auto mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
							for (size_t k = 0; k < primitiveUByte->getNumIndices(); ++k) {
								unsigned int index = primitiveUByte->at(k);
								mergePrimitiveUInt->push_back(index);
							}
						}
					}
				}

				break;
			case osg::PrimitiveSet::DrawElementsUShortPrimitiveType:
				if (mergePrimitiveset == nullptr) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					osg::ref_ptr<osg::DrawElementsUShort> primitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(pset);
					const osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					const osg::ref_ptr<osg::DrawElementsUShort> mergePrimitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(mergePrimitiveset.get());
					if (mergeType == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {
						mergePrimitiveUShort->insert(mergePrimitiveUShort->end(), primitiveUShort->begin(), primitiveUShort->end());
						primitiveUShort.release();
					}
					else {
						if (mergeType == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
							const auto mergePrimitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(mergePrimitiveset.get());
							const auto newMergePrimitvieUShort = new osg::DrawElementsUShort;
							for (size_t k = 0; k < mergePrimitiveUByte->getNumIndices(); ++k) {
								unsigned short index = mergePrimitiveUByte->at(k);
								newMergePrimitvieUShort->push_back(index);
							}
							newMergePrimitvieUShort->insert(newMergePrimitvieUShort->end(), primitiveUShort->begin(), primitiveUShort->end());
							primitiveUShort.release();
							mergePrimitiveset.release();
							mergePrimitiveset = newMergePrimitvieUShort;
						}
						else if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
							const auto mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
							for (size_t k = 0; k < primitiveUShort->getNumIndices(); ++k) {
								unsigned int index = primitiveUShort->at(k);
								mergePrimitiveUInt->push_back(index);
							}
							primitiveUShort.release();
						}
					}

				}
				break;
			case osg::PrimitiveSet::DrawElementsUIntPrimitiveType:
				if (mergePrimitiveset == nullptr) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					osg::ref_ptr<osg::DrawElementsUInt> primitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(pset);
					const osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
						const auto mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
						mergePrimitiveUInt->insert(mergePrimitiveUInt->end(), primitiveUInt->begin(), primitiveUInt->end());
						primitiveUInt.release();
					}
					else {
						const auto mergePrimitive = dynamic_cast<osg::DrawElements*>(mergePrimitiveset.get());
						const auto newMergePrimitvieUInt = new osg::DrawElementsUInt;
						for (unsigned int k = 0; k < mergePrimitive->getNumIndices(); ++k) {
							unsigned int index = mergePrimitive->getElement(k);
							newMergePrimitvieUInt->push_back(index);
						}
						newMergePrimitvieUInt->insert(newMergePrimitvieUInt->end(), primitiveUInt->begin(), primitiveUInt->end());
						primitiveUInt.release();
						mergePrimitiveset.release();
						mergePrimitiveset = newMergePrimitvieUInt;
					}
				}
				break;
			case osg::PrimitiveSet::MultiDrawArraysPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArraysIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUByteIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUShortIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUIntIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawArraysIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUByteIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUShortIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUIntIndirectPrimitiveType:
				break;
			default:
				break;
			}
		}
		if (mergePrimitiveset != nullptr) {
			for (unsigned i = 0; i < psetCount; ++i) {
				geom->removePrimitiveSet(0);
			}
			geom->addPrimitiveSet(mergePrimitiveset);
		}
	}
}
void MeshSimplifierBase::mergeGeometries(osg::ref_ptr<osg::Group> group) {

}
template<typename DrawElementsType, typename IndexArrayType>
osg::ref_ptr<IndexArrayType> MeshSimplifierBase::reindexPrimitiveSet(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex, osg::MixinVector<unsigned int>& remap)
{
	osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geom->getVertexAttribArray(0));
	osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
	osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
	osg::ref_ptr<osg::Vec2Array> texCoords = nullptr;
	if (geom->getNumTexCoordArrays())
		texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));

	if (!positions || (!normals && geom->getNormalArray()) || (!texCoords && geom->getNumTexCoordArrays())) {
		return NULL;
	}

	if (normals.valid())
		if (positions->size() != normals->size() || positions->size() != texCoords->size()) {
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

	size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(&remap.asVector()[0], &(*indices)[0], indices->size(), count,
		&streams.asVector()[0], streams.size());
	if (uniqueVertexCount > count) return NULL;

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
	return optimizedIndices.release();
}
