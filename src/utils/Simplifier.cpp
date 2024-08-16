#include "utils/Simplifier.h"
#include <meshoptimizer.h>
void Simplifier::simplifyMesh(osg::Geometry& geometry)
{
    const unsigned int psetCount = geometry.getNumPrimitiveSets();
    for (unsigned int primIndex = 0; primIndex < psetCount; ++primIndex) {
        osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
        if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES) {
            if (typeid(*pset.get()) == typeid(osg::DrawElementsUShort)) {
                osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
                simplifyPrimitiveSet<osg::DrawElementsUShort, osg::UShortArray>(geometry, drawElementsUShort, primIndex);
            }
            else if (typeid(*pset.get()) == typeid(osg::DrawElementsUInt)) {
                osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
                simplifyPrimitiveSet<osg::DrawElementsUInt, osg::UIntArray>(geometry, drawElementsUInt, primIndex);
            }
        }
        else {
            OSG_FATAL << "Geometry primitive's mode is not Triangles,please optimize the Geometry using osgUtil::Optimizer::INDEX_MESH." << std::endl;
        }
    }

}
