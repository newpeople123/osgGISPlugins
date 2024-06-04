#include "3dtiles/optimizer/mesh/MeshOptimizerVisitor.h"

MeshOptimizerVisitor::MeshOptimizerVisitor(MeshOptimizerBase* meshOptimizer, float simplifyRatio)
	: _meshOptimizer(meshOptimizer), _simplifyRatio(simplifyRatio), osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

void MeshOptimizerVisitor::apply(osg::Geometry& geometry)
{
	_meshOptimizer->reindexMesh(&geometry);
	if (_simplifyRatio < 1.0)
		_meshOptimizer->simplifyMesh(&geometry, _simplifyRatio);
	_meshOptimizer->mergePrimitives(&geometry);

}
