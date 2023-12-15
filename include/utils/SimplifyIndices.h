#pragma once
#ifndef OSG_GIS_PLUGINS_SIMPLIFYINDICES_H
#define OSG_GIS_PLUGINS_SIMPLIFYINDICES_H 1
#include "meshoptimizer.h"
#include "osgUtil/Optimizer"

class SimplifyGeometryNodeVisitor :public osg::NodeVisitor {
public:
	SimplifyGeometryNodeVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
	{
	}

	void apply(osg::Drawable& drawable) override {
		const double simplifyRadio = 0.5;
		osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
		osgUtil::Optimizer optimizer;
		optimizer.optimize(geom, osgUtil::Optimizer::INDEX_MESH);
		const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
		const unsigned int num = geom->getNumPrimitiveSets();
		if (num > 0) {
			for (unsigned int kk = 0; kk < num; ++kk) {
				osg::ref_ptr<osg::DrawElements> drawElements = dynamic_cast<osg::DrawElements*>(geom->getPrimitiveSet(kk));
				if (drawElements.valid() && positions.valid()) {
					const unsigned int numIndices = drawElements->getNumIndices();
					const unsigned int count = positions->size();
					std::vector<float> vertices(count * 3);
					for (size_t i = 0; i < count; ++i)
					{
						const osg::Vec3& vertex = positions->at(i);
						vertices[i * 3] = vertex.x();
						vertices[i * 3 + 1] = vertex.y();
						vertices[i * 3 + 2] = vertex.z();

					}

					osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(geom->getPrimitiveSet(kk));
					if (drawElementsUShort.valid()) {
						osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
						for (unsigned int i = 0; i < numIndices; ++i)
						{
							indices->push_back(drawElementsUShort->at(i));
						}
						osg::ref_ptr<osg::UShortArray> destination = new osg::UShortArray;
						destination->resize(numIndices);
						const size_t newLength = meshopt_simplify(&(*destination)[0], &(*indices)[0], (size_t)numIndices, vertices.data(), (size_t)count, (size_t)(sizeof(float) * 3), (size_t)(numIndices * simplifyRadio), 0.05f);
						osg::ref_ptr<osg::UShortArray> newIndices = new osg::UShortArray;
						if (newLength > destination->size())
						{
							std::cout << std::endl;
						}
						for (size_t i = 0; i < (size_t)newLength; ++i)
						{
							newIndices->push_back(destination->at(i));
						}
						if (newLength ==0)
						{
							geom->setPrimitiveSet(kk, new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, newIndices->size(), nullptr));//&(*newIndices)[0])
						}else
							geom->setPrimitiveSet(kk, new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, newIndices->size(), &(*newIndices)[0]));//&(*newIndices)[0])
					}
					else {
						osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(geom->getPrimitiveSet(kk));
						osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
						for (unsigned int i = 0; i < numIndices; ++i)
						{
							indices->push_back(drawElementsUInt->at(i));
						}
						osg::ref_ptr<osg::UIntArray> destination = new osg::UIntArray;
						destination->resize(numIndices);
						const size_t newLength = meshopt_simplify(&(*destination)[0], &(*indices)[0], (size_t)numIndices, vertices.data(), (size_t)count, (size_t)(sizeof(float) * 3), (size_t)(numIndices * simplifyRadio), 0.05f);
						osg::ref_ptr<osg::UIntArray> newIndices = new osg::UIntArray;
						if(newLength>destination->size())
						{
							std::cout << std::endl;
						}
						for (size_t i = 0; i < (size_t)newLength; ++i)
						{
							newIndices->push_back(destination->at(i));
						}
						if (newLength == 0)
						{
							geom->setPrimitiveSet(kk, new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, newIndices->size(), nullptr));//&(*newIndices)[0])
						}
						else
						geom->setPrimitiveSet(kk, new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, newIndices->size(), &(*newIndices)[0]));
					}

				}
			}
		}
	}
	void apply(osg::Group& group) override
	{
		traverse(group);
	}

};

#endif // !OSG_GIS_PLUGINS_SIMPLIFYINDICES_H
