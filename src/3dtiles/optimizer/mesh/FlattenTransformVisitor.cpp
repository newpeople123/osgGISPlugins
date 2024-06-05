#include "3dtiles/optimizer/mesh/FlattenTransformVisitor.h"
FlattenTransformVisitor::FlattenTransformVisitor()
    : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

void FlattenTransformVisitor::apply(osg::Drawable& drawable) {
    const osg::MatrixList matrix_list = drawable.getWorldMatrices();
    osg::Matrixd mat;
    for (const osg::Matrixd& matrix : matrix_list) {
        mat = mat * matrix;
    }
    const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(drawable.asGeometry()->getVertexArray());
    if (mat != osg::Matrixd::identity()) {
        for (auto& i : *positions) {
            i = i * mat;
        }
        drawable.dirtyBound();
        drawable.computeBound();
    }
}

void FlattenTransformVisitor::apply(osg::Transform& transform) {
    traverse(transform);
}

void FlattenTransformVisitor::apply(osg::Group& group)
{
    traverse(group);
}


