#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/ValueObject>
#include "osgdb_gltf/b3dm/UserDataVisitor.h"
#include <iostream>

void BatchIdVisitor::apply(osg::Node& node) {
    traverse(node);
}


void BatchIdVisitor::apply(osg::Drawable& drawable) {
    osg::Geometry* geom = drawable.asGeometry();
    if (geom) {
        const osg::Vec3Array* positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
        if (positions) {
            osg::ref_ptr<osg::UShortArray> batchIds = new osg::UShortArray;
            batchIds->assign(positions->size(), static_cast<unsigned int>(_batchId));

            const osg::Array* vertexAttrib = geom->getVertexAttribArray(0);
            if (vertexAttrib) {
                OSG_WARN << "Warning: geometry's VertexAttribArray(0 channel) is not Null, it will be overwritten!" << std::endl;
            }
            geom->setVertexAttribArray(0, batchIds, osg::Array::BIND_PER_VERTEX);

            _batchId++;
        }
    }
}
