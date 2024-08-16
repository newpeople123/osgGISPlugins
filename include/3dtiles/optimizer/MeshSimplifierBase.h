#ifndef OSG_GIS_PLUGINS_MESHSIMPLIFIERBASE_H
#define OSG_GIS_PLUGINS_MESHSIMPLIFIERBASE_H
#include <osg/NodeVisitor>
#include <osg/Geometry>
namespace osgGISPlugins {
    class MeshSimplifierBase {
    public:
        virtual void simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio) = 0;
        void mergePrimitives(osg::ref_ptr<osg::Geometry> geom);
        void mergeGeometries(osg::ref_ptr<osg::Group> group);

        MeshSimplifierBase() = default;
        virtual ~MeshSimplifierBase() = default;
    };
}
#endif // !OSG_GIS_PLUGINS_MESHSIMPLIFIERBASE_H
