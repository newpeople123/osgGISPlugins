#include <osgDB/Options>
#include <osgDB/Registry>
#include <osgdb_gltf/ReaderWriterGLTF.h>
#include <osgDB/FileNameUtils>
#include "osgdb_gltf/Osg2Gltf.h"
#include "osgdb_gltf/compress/GltfDracoCompressor.h"
#include "osgdb_gltf/compress/GltfMeshOptCompressor.h"
#include "osgdb_gltf/compress/GltfMeshQuantizeCompressor.h"
#include "osgdb_gltf/merge/GltfMerge.h"
#include <nlohmann/json.hpp>
#include <osgDB/ConvertUTF>
using namespace nlohmann;
osgDB::ReaderWriter::ReadResult ReaderWriterGLTF::readNode(const std::string& filenameInit,
    const Options* options) const {

    osg::notify(osg::FATAL) << "Error: this gltf plugin can't read gltf to osg, because it is for processing data rather than displaying it in osg engine!" << std::endl;
    return ReadResult::ERROR_IN_READING_FILE;
}

osgDB::ReaderWriter::WriteResult ReaderWriterGLTF::writeNode(
    const osg::Node& node,
    const std::string& filename,
    const Options* options) const {
    std::string ext = osgDB::getLowerCaseFileExtension(filename);
    if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

    osg::Node& nc_node = const_cast<osg::Node&>(node); // won't change it, promise :)
    nc_node.ref();
    BatchTableHierarchyVisitor bthv;
    if (ext == "b3dm")
        nc_node.accept(bthv);

    Osg2Gltf osg2gltf;
    nc_node.accept(osg2gltf);
    tinygltf::Model gltfModel = osg2gltf.getGltfModel();

    GltfMerger gltfMerger(gltfModel);
    gltfMerger.mergeMeshes();

    const bool embedImages = true;
    bool embedBuffers = false, prettyPrint = false, isBinary = ext != "gltf";
    bool quantize = false;
    GltfDracoCompressor::DracoCompressionOptions dracoCompressOption;
    GltfMeshQuantizeCompressor::MeshQuantizeCompressionOptions quantizeCompressOption;

    if (options) {
        std::istringstream iss(options->getOptionString());
        std::string opt;

        while (iss >> opt) {
            if (opt == "q") {
                quantize = true;
            }
            else if (opt == "eb") {
                embedBuffers = true;
            }
            else if (opt == "pp") {
                prettyPrint = true;
            }

            std::string key;
            std::string val;
            size_t found = opt.find("=");
            if (found != std::string::npos) {
                key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else {
                key = opt;
            }

            if (key == "vp") {
                dracoCompressOption.PositionQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.PositionQuantizationBits = dracoCompressOption.PositionQuantizationBits;
            }
            else if (key == "vt") {
                dracoCompressOption.TexCoordQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.TexCoordQuantizationBits = dracoCompressOption.TexCoordQuantizationBits;
            }
            else if (key == "vn") {
                dracoCompressOption.NormalQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.NormalQuantizationBits = dracoCompressOption.NormalQuantizationBits;
            }
            else if (key == "vc") {
                dracoCompressOption.ColorQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.ColorQuantizationBits = dracoCompressOption.ColorQuantizationBits;
            }
            else if (key == "vg") {
                dracoCompressOption.GenericQuantizationBits = std::atoi(val.c_str());
            }
        }

        iss.clear();
        iss.str(options->getOptionString());
        while (iss >> opt) {
            std::string key;
            std::string val;
            size_t found = opt.find("=");
            if (found != std::string::npos) {
                key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else {
                key = opt;
            }

            if (key == "ct") {
                const std::string compressionTypeStr = osgDB::convertToLowerCase(val);
                if (compressionTypeStr == "draco") {
                    if (quantize) {
                        osg::notify(osg::WARN) << "Warning: Draco compression and vector quantization cannot be enabled simultaneously. If both are enabled, only vector quantization will be performed!" << std::endl;
                        GltfMeshQuantizeCompressor compressor(gltfModel, quantizeCompressOption);
                    }
                    else {
                        GltfDracoCompressor compressor(gltfModel, dracoCompressOption);
                    }
                }
                else if (compressionTypeStr == "meshopt") {
                    if (quantize) {
                        GltfMeshQuantizeCompressor compressor(gltfModel, quantizeCompressOption);
                    }
                    GltfMeshOptCompressor compressor(gltfModel);
                }
            }
        }
    }
    gltfMerger.mergeBuffers();

    tinygltf::TinyGLTF writer;
    if (ext != "b3dm") {
        try {
            bool isSuccess = writer.WriteGltfSceneToFile(
                &gltfModel,
                filename,
                embedImages,           // embedImages
                embedBuffers,           // embedBuffers
                prettyPrint,           // prettyPrint
                isBinary);
            nc_node.unref_nodelete();
            return isSuccess ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
        }
        catch (const std::exception& e) {
            nc_node.unref_nodelete();
            OSG_FATAL << "Exception caught while writing file: " << e.what() << std::endl;
            return WriteResult::ERROR_IN_WRITING_FILE;
        }
    }
    else  {
        std::ostringstream gltfBuf;
        writer.WriteGltfSceneToStream(&gltfModel, gltfBuf, false, isBinary);

        B3DMFile b3dmFile;
        const osg::Vec3 center;
        b3dmFile.featureTableJSON = createFeatureTableJSON(center, bthv.getBatchLength());
        b3dmFile.batchTableJSON = createBatchTableJSON(bthv);
        b3dmFile.glbData = gltfBuf.str();
        b3dmFile.calculateHeaderSizes();
        nc_node.unref_nodelete();
        return writeB3DMFile(osgDB::convertStringFromUTF8toCurrentCodePage(filename), b3dmFile);
    }
    nc_node.unref_nodelete();
    return WriteResult::ERROR_IN_WRITING_FILE;
}

std::string ReaderWriterGLTF::createFeatureTableJSON(const osg::Vec3& center, unsigned short batchLength) const
{
    json featureTable;
    featureTable["BATCH_LENGTH"] = batchLength;
    featureTable["RTC_CENTER"] = { center.x(), center.y(), center.z() };
    std::string featureTableStr = featureTable.dump();
    while ((featureTableStr.size() + 28) % 8 != 0) {
        featureTableStr.push_back(' ');
    }
    return featureTableStr;
}

std::string ReaderWriterGLTF::createBatchTableJSON(BatchTableHierarchyVisitor& batchTableHierarchyVisitor) const
{
    json batchTable = json::object();

    const auto attributeNameBatchIdsMap = batchTableHierarchyVisitor.getAttributeNameBatchIdsMap();
    if (!attributeNameBatchIdsMap.empty()) {
        json extensions;
        BatchTableHierarchy hierarchy;
        const auto batchIdAttributesMap = batchTableHierarchyVisitor.getBatchIdAttributesMap();
        const auto parentIdMap = batchTableHierarchyVisitor.getBatchParentIdMap();

        hierarchy.instancesLength = batchTableHierarchyVisitor.getBatchLength();
        hierarchy.classIds.resize(batchTableHierarchyVisitor.getBatchLength(), -1);
        hierarchy.parentIds.resize(batchTableHierarchyVisitor.getBatchLength(), -1);

        int classIndex = 0;
        for (const auto& attributeNameBatchIds : attributeNameBatchIdsMap) {
            Class currentClass;
            currentClass.name = "type_" + std::to_string(classIndex);
            currentClass.length = attributeNameBatchIds.second.size();
            currentClass.instances = json::object();

            for (const auto& attributeName : attributeNameBatchIds.first) {
                currentClass.instances[attributeName] = json::array();
            }

            for (const auto batchId : attributeNameBatchIds.second) {
                const auto& attributes = batchIdAttributesMap.at(batchId);
                hierarchy.classIds[batchId] = classIndex;
                hierarchy.parentIds[batchId] = parentIdMap.at(batchId);

                for (const auto& attribute : attributes) {
                    currentClass.instances[attribute.first].push_back(attribute.second);
                }
            }

            hierarchy.classes.push_back(currentClass);
            classIndex++;
        }

        extensions["3DTILES_batch_table_hierarchy"] = hierarchy.toJson();
        batchTable["extensions"] = extensions;
    }


    std::string batchTableStr = batchTable.dump();
    while (batchTableStr.size() % 8 != 0) {
        batchTableStr.push_back(' ');
    }
    return batchTableStr;
}

osgDB::ReaderWriter::WriteResult ReaderWriterGLTF::writeB3DMFile(const std::string& filename, const B3DMFile& b3dmFile) const
{
    std::vector<unsigned char> b3dmBuffer;
    b3dmBuffer.insert(b3dmBuffer.end(), b3dmFile.header.magic, b3dmFile.header.magic + 4);
    putVal(b3dmBuffer, b3dmFile.header.version);
    putVal(b3dmBuffer, b3dmFile.header.byteLength);
    putVal(b3dmBuffer, b3dmFile.header.featureTableJSONByteLength);
    putVal(b3dmBuffer, b3dmFile.header.featureTableBinaryByteLength);
    putVal(b3dmBuffer, b3dmFile.header.batchTableJSONByteLength);
    putVal(b3dmBuffer, b3dmFile.header.batchTableBinaryByteLength);

    b3dmBuffer.insert(b3dmBuffer.end(), b3dmFile.featureTableJSON.begin(), b3dmFile.featureTableJSON.end());
    b3dmBuffer.insert(b3dmBuffer.end(), b3dmFile.featureTableBinary.begin(), b3dmFile.featureTableBinary.end());
    b3dmBuffer.insert(b3dmBuffer.end(), b3dmFile.batchTableJSON.begin(), b3dmFile.batchTableJSON.end());
    b3dmBuffer.insert(b3dmBuffer.end(), b3dmFile.batchTableBinary.begin(), b3dmFile.batchTableBinary.end());
    b3dmBuffer.insert(b3dmBuffer.end(), b3dmFile.glbData.begin(), b3dmFile.glbData.end());
    b3dmBuffer.insert(b3dmBuffer.end(), b3dmFile.glbPadding, 0);
    b3dmBuffer.insert(b3dmBuffer.end(), b3dmFile.totalPadding, 0);

    std::ofstream fout(filename, std::ios::binary);
    if (!fout) {
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    fout.write(reinterpret_cast<const char*>(b3dmBuffer.data()), b3dmBuffer.size());
    if (fout.fail()) {
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    fout.close();
    return WriteResult::FILE_SAVED;
}

REGISTER_OSGPLUGIN(gltf, ReaderWriterGLTF)
