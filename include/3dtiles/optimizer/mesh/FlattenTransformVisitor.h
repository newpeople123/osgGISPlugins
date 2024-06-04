#ifndef OSG_GIS_PLUGINS_FLATTENTRANSFORMVISITOR_H
#define OSG_GIS_PLUGINS_FLATTENTRANSFORMVISITOR_H

#include <osg/NodeVisitor>
#include <osg/Transform>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>

class FlattenTransformVisitor : public osg::NodeVisitor {
public:
    FlattenTransformVisitor();

    void apply(osg::Geode& geode) override;
    void apply(osg::Transform& transform) override;
    void apply(osg::Group& group) override;

private:
    osg::MixinVector<osg::Matrixd> _transformStack;
};
#endif // !OSG_GIS_PLUGINS_FLATTENTRANSFORMVISITOR_H
