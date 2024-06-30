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
#endif // !OSG_GIS_PLUGINS_MESHSIMPLIFIER_H
