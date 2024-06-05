#ifndef OSG_GIS_PLUGINS_FLATTENTRANSFORMVISITOR_H
#define OSG_GIS_PLUGINS_FLATTENTRANSFORMVISITOR_H

#include <osg/NodeVisitor>
#include <osg/Transform>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
class GeometryNodeVisitor :public osg::NodeVisitor {
	bool _recomputeBoundingBox;
public:
	GeometryNodeVisitor(const bool recomputeBoundingBox = true) : NodeVisitor(TRAVERSE_ALL_CHILDREN),
		_recomputeBoundingBox(recomputeBoundingBox)
	{
	}

	void apply(osg::Drawable& drawable) override {
		if (_recomputeBoundingBox)
			drawable.dirtyBound();
		const osg::ref_ptr<osg::Vec4Array> colors = dynamic_cast<osg::Vec4Array*>(drawable.asGeometry()->getColorArray());
		const osg::MatrixList matrix_list = drawable.getWorldMatrices();
		osg::Matrixd mat;
		for (const osg::Matrixd& matrix : matrix_list) {
			mat = mat * matrix;
		}
		const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(drawable.asGeometry()->getVertexArray());
		if (mat != osg::Matrixd::identity()) {
			for (auto& i : *positions)
			{
				i = mat.preMult(i);
			}
		}
	}
	void apply(osg::Group& group) override
	{
		traverse(group);
	}

};

class TransformNodeVisitor :public osg::NodeVisitor {
public:
	TransformNodeVisitor() : NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

	void apply(osg::Group& group) override
	{
		traverse(group);
	}

	void apply(osg::Transform& mtransform) override
	{
		//TestNodeVisitor tnv(mtransform.asMatrixTransform()->getMatrix());
		//mtransform.accept(tnv);
		osg::ref_ptr<osg::MatrixTransform> matrixTransform = mtransform.asMatrixTransform();
		if (matrixTransform.valid()) {
			matrixTransform->setMatrix(osg::Matrixd::identity());
		}
		else {
			osg::ref_ptr<osg::PositionAttitudeTransform> positionAttitudeTransform = mtransform.asPositionAttitudeTransform();
			if (positionAttitudeTransform.valid()) {
				// 重置位置和姿态  
				positionAttitudeTransform->setPosition(osg::Vec3());
				positionAttitudeTransform->setAttitude(osg::Quat());
			}
		}

		apply(static_cast<osg::Group&>(mtransform));
	};
};

class FlattenTransformVisitor : public osg::NodeVisitor {
public:
    FlattenTransformVisitor();

	void apply(osg::Drawable& drawable) override;
    void apply(osg::Transform& transform) override;
    void apply(osg::Group& group) override;
};
#endif // !OSG_GIS_PLUGINS_FLATTENTRANSFORMVISITOR_H
