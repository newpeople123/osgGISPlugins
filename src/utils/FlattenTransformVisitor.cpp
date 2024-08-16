#include "utils/FlattenTransformVisitor.h"
using namespace osgGISPlugins;
FlattenTransformVisitor::FlattenTransformVisitor()
    : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

void FlattenTransformVisitor::apply(osg::Drawable& drawable) {
    const osg::MatrixList matrix_list = drawable.getWorldMatrices();
    osg::Matrixd mat;
    for (const osg::Matrixd& matrix : matrix_list) {
        mat = mat * matrix;
    }
    osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(drawable.asGeometry()->getVertexArray());
    if (mat != osg::Matrixd::identity()) {
        for (auto& i : *positions) {
            i = i * mat;
        }
        drawable.asGeometry()->setVertexArray(positions);
        drawable.dirtyBound();
        drawable.computeBound();
    }
}

void FlattenTransformVisitor::apply(osg::Transform& transform) {
    traverse(transform);
    osg::ref_ptr<osg::MatrixTransform> matrixTransform = transform.asMatrixTransform();
    if (matrixTransform.valid()) {
        matrixTransform->setMatrix(osg::Matrixd::identity());
    }
    else {
        osg::ref_ptr<osg::PositionAttitudeTransform> positionAttitudeTransform = transform.asPositionAttitudeTransform();
        if (positionAttitudeTransform.valid()) {
            positionAttitudeTransform->setPosition(osg::Vec3());
            positionAttitudeTransform->setAttitude(osg::Quat());
        }
    }
    transform.dirtyBound();
    transform.computeBound();
}

void FlattenTransformVisitor::apply(osg::Group& group)
{
    traverse(group);
}
