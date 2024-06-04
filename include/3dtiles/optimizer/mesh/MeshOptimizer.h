#include "3dtiles/optimizer/mesh/MeshOptimizerBase.h"
class MeshOptimizer :public MeshOptimizerBase {
public:

	// 通过 MeshOptimizerBase 继承
	reindexMesh(osg::ref_ptr<osg::Geometry> geom) override;
	simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio) override;
};
