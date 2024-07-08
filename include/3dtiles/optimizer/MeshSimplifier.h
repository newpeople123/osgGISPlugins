#ifndef OSG_GIS_PLUGINS_MESHSIMPLIFIER_H
#define OSG_GIS_PLUGINS_MESHSIMPLIFIER_H
#include "MeshSimplifierBase.h"
class MeshSimplifier :public MeshSimplifierBase {
public:
    void simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio) override;
private:
    template<typename DrawElementsType, typename IndexArrayType>
    void simplifyPrimitiveSet(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const float simplifyRatio, unsigned int& psetIndex);
};
template<typename DrawElementsType, typename IndexArrayType>
inline void MeshSimplifier::simplifyPrimitiveSet(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const float simplifyRatio, unsigned int& psetIndex)
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

#endif // !OSG_GIS_PLUGINS_MESHSIMPLIFIER_H
