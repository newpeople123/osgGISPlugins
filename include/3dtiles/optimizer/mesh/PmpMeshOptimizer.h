#ifndef OSG_GIS_PLUGINS_PMPMESHOPTIMIZER_H
#define OSG_GIS_PLUGINS_PMPMESHOPTIMIZER_H
#include "MeshOptimizerBase.h"
#include <pmp/surface_mesh.h>
class PmpMeshOptimizer :public MeshOptimizerBase {
public:


	// 通过 MeshOptimizerBase 继承
	void reindexMesh(osg::ref_ptr<osg::Geometry> geom) override;

	void simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio) override;
private:
	pmp::SurfaceMesh* convertOsg2Pmp(osg::ref_ptr<osg::Geometry> geom);
};
#endif // !OSG_GIS_PLUGINS_PMPMESHOPTIMIZER_H
