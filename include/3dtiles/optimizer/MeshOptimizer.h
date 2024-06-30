#ifndef OSG_GIS_PLUGINS_MESHOPTIMIZER_H
#define OSG_GIS_PLUGINS_MESHOPTIMIZER_H
#include <osg/NodeVisitor>
#include "MeshSimplifierBase.h"
class MeshOptimizer :public osg::NodeVisitor {
public:
	MeshOptimizer(MeshSimplifierBase* meshOptimizer, float simplifyRatio);
	void apply(osg::Geometry& geometry) override;
private:
	MeshSimplifierBase* _meshSimplifier;
	float _simplifyRatio = 1.0;
	bool _compressmore = false;

	template <typename DrawElementsType, typename IndexArrayType>
	void processDrawElements(osg::PrimitiveSet* pset, IndexArrayType& indices);

	void vertexCacheOptimize(osg::Geometry& geometry);

	void overdrawOptimize(osg::Geometry& geometry);

	void vertexFetchOptimize(osg::Geometry& geometry);

	void reindexMesh(osg::ref_ptr<osg::Geometry> geom);

	template<typename DrawElementsType, typename IndexArrayType>
	void reindexMesh(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex);

	template<typename DrawElementsType, typename IndexArrayType>
	osg::ref_ptr<IndexArrayType> reindexMesh(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex, osg::MixinVector<unsigned int>& remap);


};
#endif // !OSG_GIS_PLUGINS_MESHOPTIMIZER_H
