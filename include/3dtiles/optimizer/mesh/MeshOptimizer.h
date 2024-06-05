#ifndef OSG_GIS_PLUGINS_MESHOPTIMIZER_H
#define OSG_GIS_PLUGINS_MESHOPTIMIZER_H
#include "MeshOptimizerBase.h"
class MeshOptimizer :public MeshOptimizerBase {
public:

	// 通过 MeshOptimizerBase 继承
	void reindexMesh(osg::ref_ptr<osg::Geometry> geom) override;
	void simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio) override;
private:
	template<typename DrawElementsType, typename IndexArrayType>
	void simplifyPrimitiveSet(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const float simplifyRatio, unsigned int& psetIndex);
};
#endif // !OSG_GIS_PLUGINS_MESHOPTIMIZER_H
