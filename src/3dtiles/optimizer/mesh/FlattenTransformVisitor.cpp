#include "3dtiles/optimizer/mesh/FlattenTransformVisitor.h"
FlattenTransformVisitor::FlattenTransformVisitor()
    : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

void FlattenTransformVisitor::apply(osg::Geode& geode) {
    osg::Matrixd worldMatrix = _transformStack.empty() ? osg::Matrixd::identity() : _transformStack.back();

    for (unsigned int i = 0; i < geode.getNumDrawables(); ++i) {
        osg::Geometry* geometry = dynamic_cast<osg::Geometry*>(geode.getDrawable(i));
        if (geometry) {
            osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
            if (vertices) {
                for (auto& vertex : *vertices) {
                    vertex = vertex * worldMatrix;
                }
            }
        }
    }
}

void FlattenTransformVisitor::apply(osg::Transform& transform) {
    osg::Matrixd matrix;
    if (transform.asMatrixTransform()) {
        matrix = transform.asMatrixTransform()->getMatrix();
    }
    else if (transform.asPositionAttitudeTransform()) {
        matrix = osg::Matrixd::translate(transform.asPositionAttitudeTransform()->getPosition()) *
            osg::Matrixd::rotate(transform.asPositionAttitudeTransform()->getAttitude());
    }

    _transformStack.push_back(_transformStack.empty() ? matrix : _transformStack.back() * matrix);

    traverse(transform);

    _transformStack.pop_back();
}

void FlattenTransformVisitor::apply(osg::Group& group) {
    traverse(group);
}
