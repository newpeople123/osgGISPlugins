#include "3dtiles/optimizer/MeshOptimizer.h"
#include <meshoptimizer.h>
MeshOptimizer::MeshOptimizer(MeshSimplifierBase* meshSimplifier, float simplifyRatio)
    : _meshSimplifier(meshSimplifier), _simplifyRatio(simplifyRatio), osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

void MeshOptimizer::reindexMesh(osg::ref_ptr<osg::Geometry> geom)
{
    if (geom.valid()) {
        const unsigned int psetCount = geom->getNumPrimitiveSets();
        if (psetCount <= 0) return;
        for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
            osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(primIndex);
            if (typeid(*pset.get()) == typeid(osg::DrawElementsUShort)) {
                osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
                reindexMesh<osg::DrawElementsUShort, osg::UShortArray>(geom, drawElementsUShort, primIndex);
            }
            else if (typeid(*pset.get()) == typeid(osg::DrawElementsUInt)) {
                osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
                reindexMesh<osg::DrawElementsUInt, osg::UIntArray>(geom, drawElementsUInt, primIndex);
            }
        }
    }
}

void MeshOptimizer::apply(osg::Geometry& geometry)
{
    //顺序很重要
    //1
    reindexMesh(&geometry);
    //2
    if (_simplifyRatio < 1.0)
        _meshSimplifier->simplifyMesh(&geometry, _simplifyRatio);
    //3
    vertexCacheOptimize(geometry);
    //4
    overdrawOptimize(geometry);
    //5
    vertexFetchOptimize(geometry);
    //6
    _meshSimplifier->mergePrimitives(&geometry);

}

void MeshOptimizer::vertexCacheOptimize(osg::Geometry& geometry)
{
    const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
    if (!positions) return;
    const size_t vertexCount = positions->size();
    const unsigned int psetCount = geometry.getNumPrimitiveSets();
    if (psetCount <= 0) return;

    for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
        osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
        if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES) {
            const osg::PrimitiveSet::Type type = pset->getType();
            if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
                osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
                processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
                if (indices->empty()) continue;
                if (_compressmore)
                    //能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
                    meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
                else
                    meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);

                auto* drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
                std::copy(indices->begin(), indices->end(), drawElements->begin());
            }
            else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {
                osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
                processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
                if (indices->empty()) continue;
                if (_compressmore)
                    //能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
                    meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
                else
                    meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);

                auto* drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
                std::copy(indices->begin(), indices->end(), drawElements->begin());
            }
            else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
                osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
                processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
                if (indices->empty()) continue;
                if (_compressmore)
                    //能顾提高压缩效率，但是GPU顶点缓存利用不如meshopt_optimizeVertexCache
                    meshopt_optimizeVertexCacheStrip(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);
                else
                    meshopt_optimizeVertexCache(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), vertexCount);

                auto* drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
                std::copy(indices->begin(), indices->end(), drawElements->begin());
            }
        }
    }
}

void MeshOptimizer::overdrawOptimize(osg::Geometry& geometry)
{
    const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
    if (!positions) return;
    const size_t vertexCount = positions->size();
    const unsigned int psetCount = geometry.getNumPrimitiveSets();
    if (psetCount <= 0) return;

    for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
        osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
        if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES) {
            const osg::PrimitiveSet::Type type = pset->getType();
            if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
                osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
                processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
                if (indices->empty()) continue;
                meshopt_optimizeOverdraw(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), &positions->at(0).x(), positions->size(), sizeof(osg::Vec3f), 1.05f);

                auto* drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
                std::copy(indices->begin(), indices->end(), drawElements->begin());
            }
            else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {
                osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
                processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
                if (indices->empty()) continue;
                meshopt_optimizeOverdraw(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), &positions->at(0).x(), positions->size(), sizeof(osg::Vec3f), 1.05f);

                auto* drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
                std::copy(indices->begin(), indices->end(), drawElements->begin());
            }
            else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
                osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
                processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
                if (indices->empty()) continue;
                meshopt_optimizeOverdraw(indices->asVector().data(), indices->asVector().data(), indices->asVector().size(), &positions->at(0).x(), positions->size(), sizeof(osg::Vec3f), 1.05f);

                auto* drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
                std::copy(indices->begin(), indices->end(), drawElements->begin());
            }
        }
    }
}

void MeshOptimizer::vertexFetchOptimize(osg::Geometry& geometry)
{
    const osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
    if (!positions) return;
    const size_t vertexCount = positions->size();
    const unsigned int psetCount = geometry.getNumPrimitiveSets();
    if (psetCount <= 0) return;
    for (size_t primIndex = 0; primIndex < psetCount; ++primIndex) {
        osg::ref_ptr<osg::PrimitiveSet> pset = geometry.getPrimitiveSet(primIndex);
        if (pset->getMode() == osg::PrimitiveSet::Mode::TRIANGLES) {
            const osg::PrimitiveSet::Type type = pset->getType();
            if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
                osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
                processDrawElements<osg::DrawElementsUByte, osg::UByteArray>(pset.get(), *indices);
                if (indices->empty()) continue;

                osg::MixinVector<unsigned int> remap(vertexCount);
                size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap[0], indices->asVector().data(), indices->asVector().size(), vertexCount);
                assert(uniqueVertices <= vertexCount);
                osg::ref_ptr<osg::DrawElementsUByte> drawElements = dynamic_cast<osg::DrawElementsUByte*>(pset.get());
                osg::ref_ptr<osg::UByteArray> optimizedIndices = reindexMesh<osg::DrawElementsUByte, osg::UByteArray>(&geometry, drawElements, primIndex, remap);
                if (!optimizedIndices) continue;
                geometry.setPrimitiveSet(primIndex, new osg::DrawElementsUByte(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));
            }
            else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {
                osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
                processDrawElements<osg::DrawElementsUShort, osg::UShortArray>(pset.get(), *indices);
                if (indices->empty()) continue;
                
                osg::MixinVector<unsigned int> remap(vertexCount);
                size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap[0], indices->asVector().data(), indices->asVector().size(), vertexCount);
                assert(uniqueVertices <= vertexCount);
                osg::ref_ptr<osg::DrawElementsUShort> drawElements = dynamic_cast<osg::DrawElementsUShort*>(pset.get());
                osg::ref_ptr<osg::UShortArray> optimizedIndices = reindexMesh<osg::DrawElementsUShort, osg::UShortArray>(&geometry, drawElements, primIndex, remap);
                if (!optimizedIndices) continue;
                geometry.setPrimitiveSet(primIndex, new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));
            }
            else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
                osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
                processDrawElements<osg::DrawElementsUInt, osg::UIntArray>(pset.get(), *indices);
                if (indices->empty()) continue;
                
                osg::MixinVector<unsigned int> remap(vertexCount);
                size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(&remap[0], indices->asVector().data(), indices->asVector().size(), vertexCount);
                assert(uniqueVertices <= vertexCount);
                osg::ref_ptr<osg::DrawElementsUInt> drawElements = dynamic_cast<osg::DrawElementsUInt*>(pset.get());
                osg::ref_ptr<osg::UIntArray> optimizedIndices = reindexMesh<osg::DrawElementsUInt, osg::UIntArray>(&geometry, drawElements, primIndex, remap);
                if (!optimizedIndices) continue;
                geometry.setPrimitiveSet(primIndex, new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));
            }
        }
    }

    
}
