#ifndef OSG_GIS_PLUGINS_MESHSIMPLIFIER_H
#define OSG_GIS_PLUGINS_MESHSIMPLIFIER_H
#include "MeshSimplifierBase.h"
class MeshSimplifier :public MeshSimplifierBase {
public:

	// 通过 MeshSimplifierBase 继承
	void reindexMesh(osg::ref_ptr<osg::Geometry> geom) override;
	void simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio) override;
private:
	template<typename DrawElementsType, typename IndexArrayType>
	void simplifyPrimitiveSet(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const float simplifyRatio, unsigned int& psetIndex);

	template<typename DrawElementsType, typename IndexArrayType>
	void reindexPrimitiveSet(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex);
};
#endif // !OSG_GIS_PLUGINS_MESHSIMPLIFIER_H
