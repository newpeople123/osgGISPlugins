#ifndef OSG_GIS_PLUGINS_MESHOPTIMIZER_H
#define OSG_GIS_PLUGINS_MESHOPTIMIZER_H
#include <osg/NodeVisitor>
#include "MeshSimplifierBase.h"
#include <assert.h>
class MeshOptimizer :public osg::NodeVisitor {
public:
    MeshOptimizer(MeshSimplifierBase* meshOptimizer, float simplifyRatio);
    void apply(osg::Geometry& geometry) override;
private:
    MeshSimplifierBase* _meshSimplifier;
    float _simplifyRatio = 1.0;
    bool _compressmore = true;

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
template<typename DrawElementsType, typename IndexArrayType>
inline void MeshOptimizer::reindexMesh(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex) {
    osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geom->getVertexAttribArray(0));
    osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
    osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
    osg::ref_ptr<osg::Vec2Array> texCoords = nullptr;
    if (geom->getNumTexCoordArrays())
        texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));

    if (!positions.valid()) {
        return;
    }

    if (normals.valid())
        if (positions->size() != normals->size()) {
            return;
        }
    if (texCoords.valid())
        if (positions->size() != texCoords->size()) {
            return;
        }

    osg::ref_ptr<IndexArrayType> indices = new IndexArrayType;
    const unsigned int indiceCount = drawElements->getNumIndices();
    osg::MixinVector<meshopt_Stream> streams;
    struct Attr
    {
        float f[4];
    };
    const size_t count = positions->size();
    osg::MixinVector<Attr> vertexData, normalData, texCoordData, batchIdData;
    for (size_t i = 0; i < count; ++i)
    {
        const osg::Vec3& vertex = positions->at(i);
        Attr v = { vertex.x(), vertex.y(), vertex.z(), 0.0f };
        vertexData.push_back(v);

        if (normals.valid()) {
            const osg::Vec3& normal = normals->at(i);
            Attr n = { normal.x(), normal.y(), normal.z(), 0.0f };
            normalData.push_back(n);
        }
        if (texCoords.valid()) {
            const osg::Vec2& texCoord = texCoords->at(i);
            Attr t = { texCoord.x(), texCoord.y(), 0.0f, 0.0f };
            texCoordData.push_back(t);
        }
        if (batchIds.valid())
        {
            Attr b = { batchIds->at(i), 0.0f, 0.0f, 0.0f };
            batchIdData.push_back(b);
        }
    }
    meshopt_Stream vertexStream = { vertexData.asVector().data(), sizeof(Attr), sizeof(Attr) };
    streams.push_back(vertexStream);
    if (normals.valid()) {
        meshopt_Stream normalStream = { normalData.asVector().data(), sizeof(Attr), sizeof(Attr) };
        streams.push_back(normalStream);
    }
    if (texCoords.valid()) {
        meshopt_Stream texCoordStream = { texCoordData.asVector().data(), sizeof(Attr), sizeof(Attr) };
        streams.push_back(texCoordStream);
    }

    for (size_t i = 0; i < indiceCount; ++i)
    {
        indices->push_back(drawElements->at(i));
    }

    osg::MixinVector<unsigned int> remap(positions->size());

    size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(&remap.asVector()[0], &(*indices)[0], indices->size(), count,
        &streams.asVector()[0], streams.size());
    assert(uniqueVertexCount <= count);

    osg::ref_ptr<IndexArrayType> optimizedIndices = reindexMesh<DrawElementsType, IndexArrayType>(geom, drawElements, psetIndex, remap);
    if (!optimizedIndices) return;
#pragma region filterTriangles
    size_t newIndiceCount = 0;
    for (size_t i = 0; i < indiceCount; i += 3) {
        const auto a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);
        if (a != b && a != c && b != c)
        {
            optimizedIndices->at(newIndiceCount + 0) = a;
            optimizedIndices->at(newIndiceCount + 1) = b;
            optimizedIndices->at(newIndiceCount + 2) = c;
            newIndiceCount += 3;
        }
    }
    optimizedIndices->resize(newIndiceCount);
#pragma endregion

    geom->setPrimitiveSet(psetIndex, new DrawElementsType(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));

}

template<typename DrawElementsType, typename IndexArrayType>
inline osg::ref_ptr<IndexArrayType> MeshOptimizer::reindexMesh(osg::ref_ptr<osg::Geometry> geom, osg::ref_ptr<DrawElementsType> drawElements, const unsigned int psetIndex, osg::MixinVector<unsigned int>& remap)
{
    osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geom->getVertexAttribArray(0));
    osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
    osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
    osg::ref_ptr<osg::Vec2Array> texCoords = nullptr;
    if (geom->getNumTexCoordArrays())
        texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));

    if (!positions.valid()) {
        return NULL;
    }

    if (normals.valid())
        if (positions->size() != normals->size()) {
            return NULL;
        }
    if (texCoords.valid())
        if (positions->size() != texCoords->size()) {
            return NULL;
        }

    osg::ref_ptr<IndexArrayType> indices = new IndexArrayType;
    const unsigned int indiceCount = drawElements->getNumIndices();
    osg::MixinVector<meshopt_Stream> streams;
    struct Attr
    {
        float f[4];
    };
    const size_t count = positions->size();
    osg::MixinVector<Attr> vertexData, normalData, texCoordData, batchIdData;

    for (size_t i = 0; i < count; ++i)
    {
        const osg::Vec3& vertex = positions->at(i);
        Attr v = { vertex.x(), vertex.y(), vertex.z(), 0.0f };
        vertexData.push_back(v);

        if (normals.valid()) {
            const osg::Vec3& normal = normals->at(i);
            Attr n = { normal.x(), normal.y(), normal.z(), 0.0f };
            normalData.push_back(n);
        }
        if (texCoords.valid()) {
            const osg::Vec2& texCoord = texCoords->at(i);
            Attr t = { texCoord.x(), texCoord.y(), 0.0f, 0.0f };
            texCoordData.push_back(t);
        }
        if (batchIds.valid())
        {
            Attr b = { batchIds->at(i), 0.0f, 0.0f, 0.0f };
            batchIdData.push_back(b);
        }
    }
    meshopt_Stream vertexStream = { vertexData.asVector().data(), sizeof(Attr), sizeof(Attr) };
    streams.push_back(vertexStream);
    if (normals.valid()) {
        meshopt_Stream normalStream = { normalData.asVector().data(), sizeof(Attr), sizeof(Attr) };
        streams.push_back(normalStream);
    }
    if (texCoords.valid()) {
        meshopt_Stream texCoordStream = { texCoordData.asVector().data(), sizeof(Attr), sizeof(Attr) };
        streams.push_back(texCoordStream);
    }

    for (size_t i = 0; i < indiceCount; ++i)
    {
        indices->push_back(drawElements->at(i));
    }

    size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(&remap.asVector()[0], &(*indices)[0], indices->size(), count,
        &streams.asVector()[0], streams.size());
    if (uniqueVertexCount > count) return NULL;

    osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(uniqueVertexCount);
    osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(uniqueVertexCount);
    osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(uniqueVertexCount);
    osg::ref_ptr<osg::FloatArray> optimizedBatchIds = new osg::FloatArray(uniqueVertexCount);
    osg::ref_ptr<IndexArrayType> optimizedIndices = new IndexArrayType(indices->size());

    meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), &remap.asVector()[0]);
    //osg::MixinVector<unsigned int>& remap
    meshopt_remapVertexBuffer(&vertexData.asVector()[0], &vertexData.asVector()[0], uniqueVertexCount, sizeof(Attr), &remap.asVector()[0]);
    vertexData.resize(uniqueVertexCount);

    if (normals.valid()) {
        meshopt_remapVertexBuffer(&normalData.asVector()[0], &normalData.asVector()[0], uniqueVertexCount, sizeof(Attr), &remap.asVector()[0]);
        normalData.resize(uniqueVertexCount);
    }
    if (texCoords.valid()) {
        meshopt_remapVertexBuffer(&texCoordData.asVector()[0], &texCoordData.asVector()[0], uniqueVertexCount, sizeof(Attr), &remap.asVector()[0]);
        texCoordData.resize(uniqueVertexCount);
    }
    if (batchIds.valid())
    {
        meshopt_remapVertexBuffer(&batchIdData.asVector()[0], &batchIdData.asVector()[0], uniqueVertexCount, sizeof(Attr), &remap.asVector()[0]);
        batchIdData.resize(uniqueVertexCount);
    }
    for (size_t i = 0; i < uniqueVertexCount; ++i) {
        optimizedVertices->at(i) = osg::Vec3(vertexData[i].f[0], vertexData[i].f[1], vertexData[i].f[2]);
        if (normals.valid())
        {
            optimizedNormals->at(i) = osg::Vec3(normalData[i].f[0], normalData[i].f[1], normalData[i].f[2]);
        }
        if (texCoords.valid())
            optimizedTexCoords->at(i) = osg::Vec2(texCoordData[i].f[0], texCoordData[i].f[1]);
        if (batchIds.valid())
        {
            optimizedBatchIds->at(i) = batchIdData[i].f[0];
        }
    }

    geom->setVertexArray(optimizedVertices);
    if (normals.valid())
        geom->setNormalArray(optimizedNormals, osg::Array::BIND_PER_VERTEX);
    if (texCoords.valid())
        geom->setTexCoordArray(0, optimizedTexCoords);
    if (batchIds.valid())
        geom->setVertexAttribArray(0, optimizedBatchIds);

    return optimizedIndices.release();
}

template <typename DrawElementsType, typename IndexArrayType>
inline void MeshOptimizer::processDrawElements(osg::PrimitiveSet* pset, IndexArrayType& indices)
{
    auto* drawElements = dynamic_cast<DrawElementsType*>(pset);
    if (drawElements) {
        indices.insert(indices.end(), drawElements->begin(), drawElements->end());
    }
}
#endif // !OSG_GIS_PLUGINS_MESHOPTIMIZER_H
