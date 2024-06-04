#ifndef OSG_GIS_PLUGINS_MESHOPTIMIZERBASE_H
#define OSG_GIS_PLUGINS_MESHOPTIMIZERBASE_H
#include <osg/NodeVisitor>
#include <osg/Geometry>
class MeshOptimizerBase {
public:
	virtual void reindexMesh(osg::ref_ptr<osg::Geometry> geom) = 0;
	virtual void simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio) = 0;
	void mergePrimitives(osg::ref_ptr<osg::Geometry> geom);
	void mergeGeometries(osg::ref_ptr<osg::Group> group);

	MeshOptimizerBase() = default;
};
#endif // !OSG_GIS_PLUGINS_MESHOPTIMIZERBASE_H
