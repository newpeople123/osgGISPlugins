#include <osg/NodeVisitor>
class MeshOptimizerBase {
private:
public:
	virtual reindexMesh(osg::ref_ptr<osg::Geometry> geom) = 0;
	virtual simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio) = 0;
	void mergePrimitives(osg::ref_ptr<osg::Geometry> geom);
	void mergeGeometries(osg::ref_ptr<osg::Group> group);
};
