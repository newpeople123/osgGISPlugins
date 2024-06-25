#include "3dtiles/optimizer/MeshOptimizerBase.h"
void MeshOptimizerBase::mergePrimitives(osg::ref_ptr<osg::Geometry> geom) {
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
void MeshOptimizerBase::mergeGeometries(osg::ref_ptr<osg::Group> group) {

}
