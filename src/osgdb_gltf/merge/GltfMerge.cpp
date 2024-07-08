#include "osgdb_gltf/merge/GltfMerge.h"
#include <osg/Math>
void mergeBuffers(tinygltf::Model& model)
{
    tinygltf::Buffer totalBuffer;
    tinygltf::Buffer fallbackBuffer;
    fallbackBuffer.name = "fallback";
    int byteOffset = 0, fallbackByteOffset = 0;
    for (auto& bufferView : model.bufferViews) {
        const auto& meshoptExtension = bufferView.extensions.find("EXT_meshopt_compression");
        if (meshoptExtension != bufferView.extensions.end()) {
            auto& bufferIndex = meshoptExtension->second.Get("buffer");
            auto& buffer = model.buffers[bufferIndex.GetNumberAsInt()];
            totalBuffer.data.insert(totalBuffer.data.end(), buffer.data.begin(), buffer.data.end());

            tinygltf::Value::Object newMeshoptExtension;
            newMeshoptExtension.insert(std::make_pair("buffer", 0));
            newMeshoptExtension.insert(std::make_pair("byteOffset", tinygltf::Value(byteOffset)));
            newMeshoptExtension.insert(std::make_pair("byteLength", meshoptExtension->second.Get("byteLength")));
            newMeshoptExtension.insert(std::make_pair("byteStride", meshoptExtension->second.Get("byteStride")));
            newMeshoptExtension.insert(std::make_pair("mode", meshoptExtension->second.Get("mode")));
            newMeshoptExtension.insert(std::make_pair("count", meshoptExtension->second.Get("count")));
            byteOffset += meshoptExtension->second.Get("byteLength").GetNumberAsInt();
            bufferView.extensions.clear();
            bufferView.extensions.insert(std::make_pair("EXT_meshopt_compression", newMeshoptExtension));
            //4-byte aligned 
            const int needAdd = 4 - byteOffset % 4;
            if (needAdd % 4 != 0) {
                for (int i = 0; i < needAdd; ++i) {
                    totalBuffer.data.push_back(0);
                    byteOffset++;
                }
            }

            bufferView.buffer = 1;
            bufferView.byteOffset = fallbackByteOffset;
            fallbackByteOffset += bufferView.byteLength;
            //4-byte aligned 
            const int fallbackNeedAdd = 4 - fallbackByteOffset % 4;
            if (fallbackNeedAdd != 0) {
                for (int i = 0; i < fallbackNeedAdd; ++i) {
                    fallbackByteOffset++;
                }
            }
        }
        else {
            auto& buffer = model.buffers[bufferView.buffer];
            totalBuffer.data.insert(totalBuffer.data.end(), buffer.data.begin(), buffer.data.end());
            bufferView.buffer = 0;
            bufferView.byteOffset = byteOffset;
            byteOffset += bufferView.byteLength;

            //4-byte aligned 
            const int needAdd = 4 - byteOffset % 4;
            if (needAdd % 4 != 0) {
                for (int i = 0; i < needAdd; ++i) {
                    totalBuffer.data.push_back(0);
                    byteOffset++;
                }
            }
        }
    }
    model.buffers.clear();
    model.buffers.push_back(totalBuffer);
    if (fallbackByteOffset > 0) {
        fallbackBuffer.data.resize(fallbackByteOffset);
        tinygltf::Value::Object bufferMeshoptExtension;
        bufferMeshoptExtension.insert(std::make_pair("fallback", true));
        fallbackBuffer.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(bufferMeshoptExtension)));
        model.buffers.push_back(fallbackBuffer);
    }
}

void mergeMeshes(tinygltf::Model& model)
{
    std::map<int, std::vector<tinygltf::Primitive>> materialPrimitiveMap;
    for (auto& mesh : model.meshes) {
        if (!mesh.primitives.empty()) {
            const auto& item = materialPrimitiveMap.find(mesh.primitives[0].material);
            if (item != materialPrimitiveMap.end()) {
                item->second.push_back(mesh.primitives[0]);
            }
            else {
                std::vector<tinygltf::Primitive> primitives;
                primitives.push_back(mesh.primitives[0]);
                materialPrimitiveMap.insert(std::make_pair(mesh.primitives[0].material, primitives));
            }
        }
    }

    std::vector<tinygltf::Accessor> accessors;
    std::vector<tinygltf::BufferView> bufferViews;
    std::vector<tinygltf::Buffer> buffers;
    std::vector<tinygltf::Primitive> primitives;
    for (auto& image : model.images) {
        tinygltf::BufferView bufferView = model.bufferViews[image.bufferView];
        tinygltf::Buffer buffer = model.buffers[bufferView.buffer];

        bufferView.buffer = buffers.size();
        image.bufferView = bufferViews.size();
        bufferViews.push_back(bufferView);
        buffers.push_back(buffer);
    }

    for (const auto& pair : materialPrimitiveMap) {
        tinygltf::Accessor totalIndicesAccessor;
        totalIndicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        tinygltf::BufferView totalIndicesBufferView;
        tinygltf::Buffer totalIndicesBuffer;

        tinygltf::BufferView totalNormalsBufferView;
        tinygltf::Buffer totalNormalsBuffer;
        tinygltf::Accessor totalNormalsAccessor;

        tinygltf::BufferView totalVerticesBufferView;
        tinygltf::Buffer totalVerticesBuffer;
        tinygltf::Accessor totalVerticesAccessor;

        tinygltf::BufferView totalTexcoordsBufferView;
        tinygltf::Buffer totalTexcoordsBuffer;
        tinygltf::Accessor totalTexcoordsAccessor;

        tinygltf::BufferView totalBatchIdsBufferView;
        tinygltf::Buffer totalBatchIdsBuffer;
        tinygltf::Accessor totalBatchIdsAccessor;

        tinygltf::BufferView totalColorsBufferView;
        tinygltf::Buffer totalColorsBuffer;
        tinygltf::Accessor totalColorsAccessor;


        tinygltf::Primitive totalPrimitive;
        totalPrimitive.mode = 4;//triangles
        totalPrimitive.material = pair.first;

        unsigned int count = 0;

        for (const auto& prim : pair.second) {
            tinygltf::Accessor& oldIndicesAccessor = model.accessors[prim.indices];
            totalIndicesAccessor = oldIndicesAccessor;
            tinygltf::Accessor& oldVerticesAccessor = model.accessors[prim.attributes.find("POSITION")->second];
            totalVerticesAccessor = oldVerticesAccessor;
            const auto& normalAttr = prim.attributes.find("NORMAL");
            if (normalAttr != prim.attributes.end()) {
                tinygltf::Accessor& oldNormalsAccessor = model.accessors[normalAttr->second];
                totalNormalsAccessor = oldNormalsAccessor;
            }
            const auto& texAttr = prim.attributes.find("TEXCOORD_0");
            if (texAttr != prim.attributes.end()) {
                tinygltf::Accessor& oldTexcoordsAccessor = model.accessors[texAttr->second];
                totalTexcoordsAccessor = oldTexcoordsAccessor;
            }
            const auto& batchidAttr = prim.attributes.find("_BATCHID");
            if (batchidAttr != prim.attributes.end()) {
                tinygltf::Accessor& oldBatchIdsAccessor = model.accessors[batchidAttr->second];
                totalBatchIdsAccessor = oldBatchIdsAccessor;
            }
            const auto& colorAttr = prim.attributes.find("COLOR_0");
            if (colorAttr != prim.attributes.end()) {
                tinygltf::Accessor& oldColorsAccessor = model.accessors[colorAttr->second];
                totalColorsAccessor = oldColorsAccessor;
            }

            const tinygltf::Accessor accessor = model.accessors[prim.indices];
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                totalIndicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                break;
            }
            count += accessor.count;
            if (count > std::numeric_limits<unsigned short>::max()) {
                totalIndicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                break;
            }
        }

        totalIndicesAccessor.count = 0;
        totalVerticesAccessor.count = 0;
        totalNormalsAccessor.count = 0;
        totalTexcoordsAccessor.count = 0;
        totalBatchIdsAccessor.count = 0;
        totalColorsAccessor.count = 0;

        auto reindexBufferAndBvAndAccessor = [&](const tinygltf::Accessor& accessor, tinygltf::BufferView& tBv, tinygltf::Buffer& tBuffer, tinygltf::Accessor& newAccessor, unsigned int sum = 0, bool isIndices = false) {
            tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
            tinygltf::Buffer& buffer = model.buffers[bv.buffer];

            tBv.byteStride = bv.byteStride;
            tBv.target = bv.target;
            if (accessor.componentType != newAccessor.componentType) {
                if (newAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT && accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    unsigned short* oldIndicesChar = (unsigned short*)buffer.data.data();
                    std::vector<unsigned int> indices;
                    for (size_t i = 0; i < accessor.count; ++i) {
                        unsigned short ushortVal = *oldIndicesChar++;
                        unsigned int uintVal = (unsigned int)(ushortVal + sum);
                        indices.push_back(uintVal);
                    }
                    unsigned char* indicesUChar = (unsigned char*)indices.data();
                    const unsigned int size = bv.byteLength * 2;
                    for (unsigned int k = 0; k < size; ++k) {
                        tBuffer.data.push_back(*indicesUChar++);
                    }
                    tBv.byteLength += bv.byteLength * 2;
                }

            }
            else {
                if (isIndices) {
                    if (newAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        unsigned int* oldIndicesChar = (unsigned int*)buffer.data.data();
                        std::vector<unsigned int> indices;
                        for (unsigned int i = 0; i < accessor.count; ++i) {
                            unsigned int oldUintVal = *oldIndicesChar++;
                            unsigned int uintVal = oldUintVal + sum;
                            indices.push_back(uintVal);
                        }
                        unsigned char* indicesUChar = (unsigned char*)indices.data();
                        const unsigned int size = bv.byteLength;
                        for (unsigned int i = 0; i < size; ++i) {
                            tBuffer.data.push_back(*indicesUChar++);
                        }
                    }
                    else {
                        unsigned short* oldIndicesChar = (unsigned short*)buffer.data.data();
                        std::vector<unsigned short> indices;
                        for (size_t i = 0; i < accessor.count; ++i) {
                            unsigned short oldUshortVal = *oldIndicesChar++;
                            unsigned short ushortVal = oldUshortVal + sum;
                            indices.push_back(ushortVal);
                        }
                        unsigned char* indicesUChar = (unsigned char*)indices.data();
                        const unsigned int size = bv.byteLength;
                        for (size_t i = 0; i < size; ++i) {
                            tBuffer.data.push_back(*indicesUChar++);
                        }
                    }
                }
                else {
                    tBuffer.data.insert(tBuffer.data.end(), buffer.data.begin(), buffer.data.end());
                }
                tBv.byteLength += bv.byteLength;
            }
            newAccessor.count += accessor.count;

            if (!accessor.minValues.empty()) {
                if (newAccessor.minValues.size() == accessor.minValues.size()) {
                    for (int k = 0; k < newAccessor.minValues.size(); ++k) {
                        newAccessor.minValues[k] = osg::minimum(newAccessor.minValues[k], accessor.minValues[k]);
                        newAccessor.maxValues[k] = osg::maximum(newAccessor.maxValues[k], accessor.maxValues[k]);
                    }
                }
                else {
                    newAccessor.minValues = accessor.minValues;
                    newAccessor.maxValues = accessor.maxValues;
                }
            }
            };

        unsigned int sum = 0;
        for (const auto& prim : pair.second) {

            tinygltf::Accessor& oldVerticesAccessor = model.accessors[prim.attributes.find("POSITION")->second];
            reindexBufferAndBvAndAccessor(oldVerticesAccessor, totalVerticesBufferView, totalVerticesBuffer, totalVerticesAccessor);

            tinygltf::Accessor& oldIndicesAccessor = model.accessors[prim.indices];
            reindexBufferAndBvAndAccessor(oldIndicesAccessor, totalIndicesBufferView, totalIndicesBuffer, totalIndicesAccessor, sum, true);
            sum += oldVerticesAccessor.count;

            const auto& normalAttr = prim.attributes.find("NORMAL");
            if (normalAttr != prim.attributes.end()) {
                tinygltf::Accessor& oldNormalsAccessor = model.accessors[normalAttr->second];
                reindexBufferAndBvAndAccessor(oldNormalsAccessor, totalNormalsBufferView, totalNormalsBuffer, totalNormalsAccessor);
            }
            const auto& texAttr = prim.attributes.find("TEXCOORD_0");
            if (texAttr != prim.attributes.end()) {
                tinygltf::Accessor& oldTexcoordsAccessor = model.accessors[texAttr->second];
                reindexBufferAndBvAndAccessor(oldTexcoordsAccessor, totalTexcoordsBufferView, totalTexcoordsBuffer, totalTexcoordsAccessor);
            }
            const auto& batchidAttr = prim.attributes.find("_BATCHID");
            if (batchidAttr != prim.attributes.end()) {
                tinygltf::Accessor& oldBatchIdsAccessor = model.accessors[batchidAttr->second];
                reindexBufferAndBvAndAccessor(oldBatchIdsAccessor, totalBatchIdsBufferView, totalBatchIdsBuffer, totalBatchIdsAccessor);
            }
            const auto& colorAttr = prim.attributes.find("COLOR_0");
            if (colorAttr != prim.attributes.end()) {
                tinygltf::Accessor& oldColorsAccessor = model.accessors[colorAttr->second];
                reindexBufferAndBvAndAccessor(oldColorsAccessor, totalColorsBufferView, totalColorsBuffer, totalColorsAccessor);
            }
        }

        totalIndicesBufferView.buffer = buffers.size();
        totalIndicesAccessor.bufferView = bufferViews.size();
        buffers.push_back(totalIndicesBuffer);
        bufferViews.push_back(totalIndicesBufferView);
        totalPrimitive.indices = accessors.size();
        accessors.push_back(totalIndicesAccessor);

        totalVerticesBufferView.buffer = buffers.size();
        totalVerticesAccessor.bufferView = bufferViews.size();
        buffers.push_back(totalVerticesBuffer);
        bufferViews.push_back(totalVerticesBufferView);
        totalPrimitive.attributes.insert(std::make_pair("POSITION", accessors.size()));
        accessors.push_back(totalVerticesAccessor);

        if (totalNormalsAccessor.count) {
            totalNormalsBufferView.buffer = buffers.size();
            totalNormalsAccessor.bufferView = bufferViews.size();
            buffers.push_back(totalNormalsBuffer);
            bufferViews.push_back(totalNormalsBufferView);
            totalPrimitive.attributes.insert(std::make_pair("NORMAL", accessors.size()));
            accessors.push_back(totalNormalsAccessor);
        }

        if (totalTexcoordsAccessor.count) {
            totalTexcoordsBufferView.buffer = buffers.size();
            totalTexcoordsAccessor.bufferView = bufferViews.size();
            buffers.push_back(totalTexcoordsBuffer);
            bufferViews.push_back(totalTexcoordsBufferView);
            totalPrimitive.attributes.insert(std::make_pair("TEXCOORD_0", accessors.size()));
            accessors.push_back(totalTexcoordsAccessor);
        }

        if (totalBatchIdsAccessor.count) {
            totalBatchIdsBufferView.buffer = buffers.size();
            totalBatchIdsAccessor.bufferView = bufferViews.size();
            buffers.push_back(totalBatchIdsBuffer);
            bufferViews.push_back(totalBatchIdsBufferView);
            totalPrimitive.attributes.insert(std::make_pair("_BATCHID", accessors.size()));
            accessors.push_back(totalBatchIdsAccessor);
        }

        if (totalColorsAccessor.count) {
            totalColorsBufferView.buffer = buffers.size();
            totalColorsAccessor.bufferView = bufferViews.size();
            buffers.push_back(totalColorsBuffer);
            bufferViews.push_back(totalColorsBufferView);
            totalPrimitive.attributes.insert(std::make_pair("COLOR_0", accessors.size()));
            accessors.push_back(totalColorsAccessor);
        }

        primitives.push_back(totalPrimitive);
    }


    model.buffers.clear();
    model.bufferViews.clear();
    model.accessors.clear();
    model.buffers = buffers;
    model.bufferViews = bufferViews;
    model.accessors = accessors;

    tinygltf::Mesh totalMesh;
    totalMesh.primitives = primitives;
    //for (auto& mesh : model.meshes) {
    //	if (mesh.primitives.size() > 0) {
    //		totalMesh.primitives.push_back(mesh.primitives[0]);
    //	}
    //}


    model.nodes.clear();
    tinygltf::Node node;
    node.mesh = 0;
    model.nodes.push_back(node);
    model.meshes.clear();
    model.meshes.push_back(totalMesh);
    model.scenes[0].nodes.clear();
    model.scenes[0].nodes.push_back(0);



}

