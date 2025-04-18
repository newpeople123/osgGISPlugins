#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <osg/Geometry>
#include <osg/Geode>
using namespace osgGISPlugins;
void BatchIdVisitor::apply(osg::Drawable& drawable) {
    _bAdd = false;
    osg::Geometry* geometry = drawable.asGeometry();
    if (geometry) {
        const osg::Vec3Array* positions = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
        osg::ref_ptr<osg::FloatArray> batchIds = new osg::FloatArray;
        if (positions&&positions->size()) {
            batchIds->assign(positions->size(), _currentBatchId);

            const osg::Array* vertexAttrib = geometry->getVertexAttribArray(0);
            if (vertexAttrib) {
                OSG_WARN << "Warning: geometry's VertexAttribArray(0 channel) is not null, it will be overwritten!" << std::endl;
            }
            geometry->setVertexAttribArray(0, batchIds, osg::Array::BIND_PER_VERTEX);
        }else{
            batchIds->assign(1, _currentBatchId);
            geometry->setVertexAttribArray(0, batchIds, osg::Array::BIND_PER_VERTEX);

        }
        _bAdd = true;
    }
}

void BatchIdVisitor::apply(osg::Geode& geode)
{
    if (geode.getNumDrawables()) {
        traverse(geode);
        if(_bAdd)
            _currentBatchId++;
    }
}
