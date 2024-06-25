#ifndef OSG_GIS_PLUGINS_MESHOPTIMIZERVISITOR_H
#define OSG_GIS_PLUGINS_MESHOPTIMIZERVISITOR_H
#include <osg/NodeVisitor>
#include "MeshOptimizerBase.h"
class MeshOptimizerVisitor :public osg::NodeVisitor {
public:
	MeshOptimizerVisitor(MeshOptimizerBase* meshOptimizer, float simplifyRatio);
	void apply(osg::Geometry& geometry) override;
private:
	MeshOptimizerBase* _meshOptimizer;
	float _simplifyRatio = 1.0;
};
#endif // !OSG_GIS_PLUGINS_MESHOPTIMIZERVISITOR_H
