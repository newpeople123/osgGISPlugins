#ifndef OSG_GIS_PLUGINS_SIMPLIFIER_H
#define OSG_GIS_PLUGINS_SIMPLIFIER_H
#include <osgUtil/Simplifier>
#include <unordered_set>
class Simplifier :public osgUtil::Simplifier
{
private:
	bool _aggressive = false, _lockBorders = false;
	float _targetError = 1e-2f, _targetErrorAggressive = 1e-1f;
public:
	Simplifier(const double sampleRatio, const bool aggressive = false,
		const bool lockBorders = false, const float targetError = 1e-2f) :osgUtil::Simplifier(sampleRatio),
		_aggressive(aggressive), _lockBorders(lockBorders) {
		if (_aggressive)
			_targetErrorAggressive = targetError;
		else
			_targetError = targetError;
	}

	META_NodeVisitor(osgGISPlugins, Simplifier)

	void apply(osg::Geometry& geom) override
    {
        simplifyMesh(geom);
    }
protected:
	void simplifyMesh(osg::Geometry& geometry);

	template<typename DrawElementsType, typename IndexArrayType>
	void simplifyPrimitiveSet(osg::Geometry& geom, osg::ref_ptr<DrawElementsType> drawElements,unsigned int& psetIndex);

};
template<typename DrawElementsType, typename IndexArrayType>
inline void Simplifier::simplifyPrimitiveSet(osg::Geometry& geom, osg::ref_ptr<DrawElementsType> drawElements,unsigned int& psetIndex)
{
    const unsigned int options = _lockBorders ? meshopt_SimplifyLockBorder : 0;

    const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom.getVertexArray());
    if (!positions.valid()) return;
    const osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom.getNormalArray());
    const osg::ref_ptr<osg::Vec2Array> texcoords = dynamic_cast<osg::Vec2Array*>(geom.getTexCoordArray(0));

    const unsigned int indiceCount = drawElements->getNumIndices();
    const unsigned int positionCount = positions->size();
    std::vector<float> vertices;
    vertices.reserve(positionCount * 3);  // 预先分配足够的空间

    for (const auto& pos : *positions) {
        vertices.push_back(pos.x());
        vertices.push_back(pos.y());
        vertices.push_back(pos.z());
    }
    osg::ref_ptr<IndexArrayType> indices = new IndexArrayType;
    indices->reserve(indiceCount);
    indices->insert(indices->end(), drawElements->begin(), drawElements->begin() + indiceCount);

    osg::ref_ptr<IndexArrayType> destination = new IndexArrayType(indiceCount);
    const unsigned int targetIndexCount = static_cast<unsigned int>(indiceCount * _sampleRatio);
    const size_t stride = sizeof(osg::Vec3);
    destination->resize(meshopt_simplify(&(*destination)[0], &(*indices)[0], indiceCount, vertices.data(), positionCount, stride, targetIndexCount, _targetError, options));

    if (_aggressive && destination->size() > targetIndexCount) {
        destination->resize(meshopt_simplifySloppy(&(*destination)[0], &(*indices)[0], indiceCount, vertices.data(), positionCount, stride, targetIndexCount, _targetErrorAggressive));
    }

    // 创建用于查找使用过的顶点的集合
    std::unordered_set<size_t> usedVertices(destination->begin(), destination->end());

    // 创建映射，用于重新排列顶点
    std::vector<unsigned int> remap(positionCount, -1);
    unsigned int newIndex = 0;

    for (size_t i = 0; i < positionCount; ++i) {
        if (usedVertices.find(i) != usedVertices.end()) {
            remap[i] = newIndex++;
        }
    }

    // 创建新的顶点数组
    osg::ref_ptr<osg::Vec3Array> newPositions = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec3Array> newNormals = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec2Array> newTexcoords = new osg::Vec2Array;

    for (size_t i = 0; i < positionCount; ++i) {
        if (remap[i] != -1) {
            newPositions->push_back(positions->at(i));
            if (normals.valid()) {
                newNormals->push_back(normals->at(i));
            }
            if (texcoords.valid()) {
                newTexcoords->push_back(texcoords->at(i));
            }
        }
    }

    // 重新排列索引
    for (size_t i = 0; i < destination->size(); ++i) {
        destination->at(i) = remap[destination->at(i)];
    }

    // 更新几何图形的顶点数组、法线和纹理坐标
    geom.setVertexArray(newPositions);
    if (normals.valid()) {
        geom.setNormalArray(newNormals);
    }
    if (texcoords.valid()) {
        geom.setTexCoordArray(0, newTexcoords);
    }

    geom.setPrimitiveSet(psetIndex, new DrawElementsType(osg::PrimitiveSet::TRIANGLES, destination->size(), &(*destination)[0]));
}
#endif // !OSG_GIS_PLUGINS_SIMPLIFIER_H
