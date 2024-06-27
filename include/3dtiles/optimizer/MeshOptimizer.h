#ifndef OSG_GIS_PLUGINS_MESHOPTIMIZER_H
#define OSG_GIS_PLUGINS_MESHOPTIMIZER_H
#include <osg/NodeVisitor>
#include "MeshSimplifierBase.h"
class MeshOptimizer :public osg::NodeVisitor {
public:
	MeshOptimizer(MeshSimplifierBase* meshOptimizer, float simplifyRatio);
	void apply(osg::Geometry& geometry) override;
private:
	MeshSimplifierBase* _meshSimplifier;
	float _simplifyRatio = 1.0;
};
#endif // !OSG_GIS_PLUGINS_MESHOPTIMIZER_H
