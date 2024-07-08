#include "3dtiles/optimizer/MeshSimplifier.h"
#include <osg/PrimitiveSet>
#include <meshoptimizer.h>
#include <osgUtil/Optimizer>
#include <iostream>

void MeshSimplifier::simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio)
{
    if (!geom.valid()) return;
    const unsigned int psetCount = geom->getNumPrimitiveSets();
    for (unsigned int primIndex = 0; primIndex < psetCount; ++primIndex) {
        osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(primIndex);
        if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES) {
            if (typeid(*pset.get()) == typeid(osg::DrawElementsUShort)) {
                osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
                simplifyPrimitiveSet<osg::DrawElementsUShort, osg::UShortArray>(geom, drawElementsUShort, simplifyRatio, primIndex);
            }
            else if (typeid(*pset.get()) == typeid(osg::DrawElementsUInt)) {
                osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
                simplifyPrimitiveSet<osg::DrawElementsUInt, osg::UIntArray>(geom, drawElementsUInt, simplifyRatio, primIndex);
            }
        }
        else {
            std::cerr << "Geometry primitive's mode is not Triangles,please optimize the Geometry using osgUtil::Optimizer::INDEX_MESH." << std::endl;
        }
    }


}
