#include "3dtiles/optimizer/MeshOptimizer.h"

MeshOptimizer::MeshOptimizer(MeshSimplifierBase* meshSimplifier, float simplifyRatio)
	: _meshSimplifier(meshSimplifier), _simplifyRatio(simplifyRatio), osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

void MeshOptimizer::apply(osg::Geometry& geometry)
{
	_meshSimplifier->reindexMesh(&geometry);
	if (_simplifyRatio < 1.0)
		_meshSimplifier->simplifyMesh(&geometry, _simplifyRatio);
	_meshSimplifier->mergePrimitives(&geometry);

}
