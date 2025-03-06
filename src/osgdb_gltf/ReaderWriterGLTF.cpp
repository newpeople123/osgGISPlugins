#include <osgDB/Options>
#include <osgDB/Registry>
#include <osgdb_gltf/ReaderWriterGLTF.h>
#include <osgDB/FileNameUtils>
#include "osgdb_gltf/Osg2Gltf.h"
#include "osgdb_gltf/GltfProcessorManager.h"
#include <nlohmann/json.hpp>
#include <osgDB/ConvertUTF>
#include <osg/MatrixTransform>
#include <osg/CoordinateSystemNode>
using namespace nlohmann;
osgDB::ReaderWriter::ReadResult ReaderWriterGLTF::readNode(const std::string& filenameInit,
    const Options* options) const
{

    osg::notify(osg::FATAL) << "Error: this gltf plugin can't read gltf to osg, because it is for processing data rather than displaying it in osg engine!" << std::endl;
    return ReadResult::ERROR_IN_READING_FILE;
}

osgDB::ReaderWriter::WriteResult ReaderWriterGLTF::writeNode(
    const osg::Node& node,
    const std::string& filename,
    const Options* options) const
{
    std::string ext = osgDB::getLowerCaseFileExtension(filename);
    if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

    osg::Node& nc_node = const_cast<osg::Node&>(node); // won't change it, promise :)
    nc_node.ref();
    BatchTableHierarchyVisitor bthv;
    if (ext == "b3dm" || ext == "i3dm")
        nc_node.accept(bthv);

    Osg2Gltf osg2gltf;
    if (ext == "i3dm")
    {
        osg::ref_ptr<osg::Group> geodes = nc_node.asGroup()->getChild(0)->asGroup();
        osg::ref_ptr<osg::Group> waitConvertGeodes = new osg::Group;
        for (size_t i = 0; i < geodes->getNumChildren(); ++i)
        {
            waitConvertGeodes->addChild(geodes->getChild(i));
        }
        waitConvertGeodes->accept(osg2gltf);
    }
    else
        nc_node.accept(osg2gltf);
    tinygltf::Model gltfModel = osg2gltf.getGltfModel();

    const bool embedImages = true;
    bool embedBuffers = false, prettyPrint = false, isBinary = ext != "gltf", mergeMaterial = true, mergeMesh = true, unlit = false;
    GltfDracoCompressor::DracoCompressionOptions dracoCompressOption;
    GltfMeshQuantizeCompressor::MeshQuantizeCompressionOptions quantizeCompressOption;
    GltfProcessorManager processorManager;

    if (options)
    {
        std::istringstream iss(options->getOptionString());
        std::string opt;
        bool quantize = false;
        while (iss >> opt)
        {
            if (opt == "quantize")
            {
                quantize = true;
            }
            else if (opt == "eb")
            {
                embedBuffers = true;
            }
            else if (opt == "pp")
            {
                prettyPrint = true;
            }
            else if (opt == "noMergeMaterial")
                mergeMaterial = false;
            else if (opt == "noMergeMesh")
                mergeMesh = false;
            else if (opt == "unlit")
                unlit = true;

            std::string key;
            std::string val;
            size_t found = opt.find("=");
            if (found != std::string::npos)
            {
                key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else
            {
                key = opt;
            }

            if (key == "vp")
            {
                dracoCompressOption.PositionQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.PositionQuantizationBits = dracoCompressOption.PositionQuantizationBits;
            }
            else if (key == "vt")
            {
                dracoCompressOption.TexCoordQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.TexCoordQuantizationBits = dracoCompressOption.TexCoordQuantizationBits;
            }
            else if (key == "vn")
            {
                dracoCompressOption.NormalQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.NormalQuantizationBits = dracoCompressOption.NormalQuantizationBits;
            }
            else if (key == "vc")
            {
                dracoCompressOption.ColorQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.ColorQuantizationBits = dracoCompressOption.ColorQuantizationBits;
            }
            else if (key == "vg")
            {
                dracoCompressOption.GenericQuantizationBits = std::atoi(val.c_str());
            }
        }

        iss.clear();
        iss.str(options->getOptionString());
        while (iss >> opt)
        {
            std::string key;
            std::string val;
            size_t found = opt.find("=");
            if (found != std::string::npos)
            {
                key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else
            {
                key = opt;
            }

            if (key == "ct")
            {
                const std::string compressionTypeStr = osgDB::convertToLowerCase(val);
                if (compressionTypeStr == "draco")
                {
                    if (quantize)
                    {
                        osg::notify(osg::WARN) << "Warning: Draco compression and vector quantization cannot be enabled simultaneously. If both are enabled, only vector quantization will be performed!" << std::endl;
                        processorManager.addProcessor(new GltfMeshQuantizeCompressor(gltfModel, quantizeCompressOption));
                    }
                    else
                    {
                        processorManager.addProcessor(new GltfDracoCompressor(gltfModel, dracoCompressOption));
                    }
                }
                else if (compressionTypeStr == "meshopt")
                {
                    if (quantize)
                    {
                        processorManager.addProcessor(new GltfMeshQuantizeCompressor(gltfModel, quantizeCompressOption));
                    }
                    processorManager.addProcessor(new GltfMeshOptCompressor(gltfModel));
                }
            }
        }
    }
    if (unlit)
    {
        gltfModel.extensionsRequired.push_back("KHR_materials_unlit");
        gltfModel.extensionsUsed.push_back("KHR_materials_unlit");
    }

    GltfMerger* gltfMerger = new GltfMerger(gltfModel, mergeMaterial, mergeMesh);
    processorManager.addProcessor(gltfMerger);
    processorManager.process();

    tinygltf::TinyGLTF writer;
    if (ext != "b3dm" && ext != "i3dm")
    {
        try
        {
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
        catch (const std::exception& e)
        {
            nc_node.unref_nodelete();
            OSG_FATAL << "Exception caught while writing file: " << e.what() << std::endl;
            return WriteResult::ERROR_IN_WRITING_FILE;
        }
    }
    else
    {
        std::ostringstream gltfBuf;
        writer.WriteGltfSceneToStream(&gltfModel, gltfBuf, false, isBinary);

        if (ext == "b3dm")
        {
            B3DMFile b3dmFile;
            const osg::Vec3 center;
            b3dmFile.featureTableJSON = createFeatureB3DMTableJSON(center, bthv.getBatchLength());
            b3dmFile.batchTableJSON = createBatchTableJSON(bthv);
            b3dmFile.glbData = gltfBuf.str();
            b3dmFile.calculateHeaderSizes();
            nc_node.unref_nodelete();
            return writeB3DMFile(osgDB::convertStringFromUTF8toCurrentCodePage(filename), b3dmFile);
        }
        else
        {
            I3DMFile i3dmFile;
            osg::ref_ptr<osg::Group> matrixTransforms = nc_node.asGroup();
            const size_t length = matrixTransforms->getNumChildren();

            i3dmFile.featureTableJSON = createFeatureI3DMTableJSON(length);
            i3dmFile.featureTableBinary = createFeatureI3DMTableBinary(matrixTransforms);
            i3dmFile.batchTableJSON = createBatchTableJSON(bthv);
            i3dmFile.glbData = gltfBuf.str();
            i3dmFile.calculateHeaderSizes();
            nc_node.unref_nodelete();
            return writeI3DMFile(osgDB::convertStringFromUTF8toCurrentCodePage(filename), i3dmFile);
        }
    }
    nc_node.unref_nodelete();
    return WriteResult::ERROR_IN_WRITING_FILE;
}

std::string ReaderWriterGLTF::createFeatureB3DMTableJSON(const osg::Vec3& center, unsigned short batchLength) const
{
    json featureTable;
    featureTable["BATCH_LENGTH"] = batchLength;
    featureTable["RTC_CENTER"] = { center.x(), center.y(), center.z() };
    std::string featureTableStr = featureTable.dump();
    while ((featureTableStr.size() + 28) % 8 != 0)
    {
        featureTableStr.push_back(' ');
    }
    return featureTableStr;
}

std::string ReaderWriterGLTF::createFeatureI3DMTableJSON(const unsigned int length) const
{
    json featureTable;
    featureTable["INSTANCES_LENGTH"] = length;

    // 计算每个字段的 byteOffset 和 byteLength
    size_t byteOffset = 0;

    // POSITION
    featureTable["POSITION"]["byteOffset"] = byteOffset;
    byteOffset += length * 3 * sizeof(float);  // 3 components for each position
    byteOffset += (4 - (byteOffset % 4)) % 4;

    // NORMAL_UP
    featureTable["NORMAL_UP"]["byteOffset"] = byteOffset;
    byteOffset += length * 3 * sizeof(float);  // 3 components for normal UP
    byteOffset += (4 - (byteOffset % 4)) % 4;

    // NORMAL_RIGHT
    featureTable["NORMAL_RIGHT"]["byteOffset"] = byteOffset;
    byteOffset += length * 3 * sizeof(float);  // 3 components for normal RIGHT
    byteOffset += (4 - (byteOffset % 4)) % 4;

    // SCALE_NON_UNIFORM
    featureTable["SCALE_NON_UNIFORM"]["byteOffset"] = byteOffset;
    byteOffset += length * 3 * sizeof(float);  // 3 components for scale
    byteOffset += (4 - (byteOffset % 4)) % 4;

    // BATCH_ID
    featureTable["BATCH_ID"]["byteOffset"] = byteOffset;
    if (length < 256)
    {
        featureTable["BATCH_ID"]["byteLength"] = length * sizeof(uint8_t);
        featureTable["BATCH_ID"]["componentType"] = "UNSIGNED_BYTE";
    }
    else 	if (length < 65536)
    {
        featureTable["BATCH_ID"]["byteLength"] = length * sizeof(uint16_t);
        featureTable["BATCH_ID"]["componentType"] = "UNSIGNED_SHORT";
    }
    else
    {
        featureTable["BATCH_ID"]["byteLength"] = length * sizeof(uint32_t);
        featureTable["BATCH_ID"]["componentType"] = "UNSIGNED_INT";
    }

    // 调整 JSON 字符串长度到 8 字节对齐
    std::string featureTableStr = featureTable.dump();
    while (featureTableStr.size() % 8 != 0)
    {
        featureTableStr.push_back(' ');
    }

    return featureTableStr;
}

std::string ReaderWriterGLTF::createFeatureI3DMTableBinary(osg::ref_ptr<osg::Group> matrixTransforms) const
{
    const unsigned int length = matrixTransforms->getNumChildren();

    // 预先分配足够的内存空间，避免动态扩展
    std::vector<float> positions(length * 3);
    std::vector<float> normalUps(length * 3);
    std::vector<float> normalRights(length * 3);
    std::vector<float> scaleNonUniforms(length * 3);
    std::vector<uint8_t> batchIDUint8s(length);
    std::vector<uint16_t> batchIDUint16s(length);
    std::vector<uint32_t> batchIDUint32s(length);

    for (unsigned int i = 0; i < length; ++i)
    {
        osg::ref_ptr<osg::MatrixTransform> matrixTransform = dynamic_cast<osg::MatrixTransform*>(matrixTransforms->getChild(i));
        if (!matrixTransform.valid())
        {
            continue;
        }
        const osg::Matrixd yUpToZUpRotationMatrix = osg::Matrixd::rotate(osg::PI / 2, osg::Vec3(1.0, 0.0, 0.0));
        const osg::Matrixd zUpToYUpRotationMatrix = osg::Matrixd::rotate(-osg::PI / 2, osg::Vec3(1.0, 0.0, 0.0));
        // Cesium默认是Z轴朝上的，由于设置了gltfUpAxis的值为Y，
        // 需要保证节点(matrixTransform)本身是Y轴朝上的，同时需要将Cesium默认的Z轴朝上转换为Y轴朝上。
        // 注：当Cesium加载3dtiles并识别到gltfUpAxis的值为Y时，会自动左乘一个Axis.Y_UP_TO_Z_UP矩阵
        //     （即与 yUpToZUpRotationMatrix 值相同）。这样，两个相反的变换矩阵（yUpToZUp 和 zUpToYUp）就抵消了，
        //     模型的姿态才会正确。如果不进行 zUpToYUpRotationMatrix 的预乘，模型的姿态和位置在渲染时会不正确。
        const osg::Matrixd transformedMatrix = zUpToYUpRotationMatrix * matrixTransform->getMatrix();

        osg::Vec3d position;
        osg::Quat rotation;
        osg::Vec3d scale;
        osg::Quat scaleOrientation; // 对应缩放的旋转

        transformedMatrix.decompose(position, rotation, scale, scaleOrientation);
        // 从transformedMatrix中提取出的position仍然是Y轴朝上的，
        // 但是Cesium要求Z轴朝上，所以需要将position转换到Z轴朝上坐标系。
        position = yUpToZUpRotationMatrix.preMult(position);
        positions[i * 3 + 0] = position.x();
        positions[i * 3 + 1] = position.y();
        positions[i * 3 + 2] = position.z();

        // 类似地，rotation（旋转）也需要转换
        const osg::Vec3f upVector = yUpToZUpRotationMatrix.preMult(rotation * osg::Vec3f(0, 1, 0));
        normalUps[i * 3 + 0] = upVector.x();
        normalUps[i * 3 + 1] = upVector.y();
        normalUps[i * 3 + 2] = upVector.z();
        const osg::Vec3f rightVector = yUpToZUpRotationMatrix.preMult(rotation * osg::Vec3f(1, 0, 0));
        normalRights[i * 3 + 0] = rightVector.x();
        normalRights[i * 3 + 1] = rightVector.y();
        normalRights[i * 3 + 2] = rightVector.z();


        scaleNonUniforms[i * 3 + 0] = scale.x();
        scaleNonUniforms[i * 3 + 1] = scale.y();
        scaleNonUniforms[i * 3 + 2] = scale.z();

        batchIDUint8s[i] = i;
        batchIDUint16s[i] = i;
        batchIDUint32s[i] = i;
    }

    // 提前分配内存
    std::ostringstream oss;
    oss.write(reinterpret_cast<const char*>(positions.data()), length * 3 * sizeof(float));
    //4字节对齐
    // 计算当前写入的总字节数
    size_t currentSize = oss.tellp();
    size_t padding = (4 - (currentSize % 4)) % 4;  // 计算需要的填充字节数
    oss.write(std::string(padding, 0).c_str(), padding);  // 写入填充字节

    oss.write(reinterpret_cast<const char*>(normalUps.data()), length * 3 * sizeof(float));
    currentSize = oss.tellp();
    padding = (4 - (currentSize % 4)) % 4;  // 计算需要的填充字节数
    oss.write(std::string(padding, 0).c_str(), padding);  // 写入填充字节

    oss.write(reinterpret_cast<const char*>(normalRights.data()), length * 3 * sizeof(float));
    currentSize = oss.tellp();
    padding = (4 - (currentSize % 4)) % 4;  // 计算需要的填充字节数
    oss.write(std::string(padding, 0).c_str(), padding);  // 写入填充字节


    oss.write(reinterpret_cast<const char*>(scaleNonUniforms.data()), length * 3 * sizeof(float));
    currentSize = oss.tellp();
    padding = (4 - (currentSize % 4)) % 4;  // 计算需要的填充字节数
    oss.write(std::string(padding, 0).c_str(), padding);  // 写入填充字节

    if (length < 256)
        oss.write(reinterpret_cast<const char*>(batchIDUint8s.data()), length * sizeof(uint8_t));
    else if (length < 65536)
        oss.write(reinterpret_cast<const char*>(batchIDUint16s.data()), length * sizeof(uint16_t));
    else
        oss.write(reinterpret_cast<const char*>(batchIDUint32s.data()), length * sizeof(uint32_t));

    std::string featureTableBinaryStr = oss.str();
    // 调整 JSON 字符串长度到 8 字节对齐
    while (featureTableBinaryStr.size() % 8 != 0)
    {
        featureTableBinaryStr.push_back(' ');
    }

    return featureTableBinaryStr;
}

std::string ReaderWriterGLTF::createBatchTableJSON(BatchTableHierarchyVisitor& batchTableHierarchyVisitor) const
{
    json batchTable = json::object();

    const auto attributeNameBatchIdsMap = batchTableHierarchyVisitor.getAttributeNameBatchIdsMap();
    if (!attributeNameBatchIdsMap.empty())
    {
        json extensions;
        BatchTableHierarchy hierarchy;
        const auto batchIdAttributesMap = batchTableHierarchyVisitor.getBatchIdAttributesMap();
        const auto parentIdMap = batchTableHierarchyVisitor.getBatchParentIdMap();

        hierarchy.instancesLength = batchTableHierarchyVisitor.getBatchLength();
        hierarchy.classIds.resize(batchTableHierarchyVisitor.getBatchLength(), -1);
        hierarchy.parentIds.resize(batchTableHierarchyVisitor.getBatchLength(), -1);

        int classIndex = 0;
        for (const auto& attributeNameBatchIds : attributeNameBatchIdsMap)
        {
            Class currentClass;
            currentClass.name = "type_" + std::to_string(classIndex);
            currentClass.length = attributeNameBatchIds.second.size();
            currentClass.instances = json::object();

            for (const auto& attributeName : attributeNameBatchIds.first)
            {
                currentClass.instances[attributeName] = json::array();
            }

            for (const auto batchId : attributeNameBatchIds.second)
            {
                const auto& attributes = batchIdAttributesMap.at(batchId);
                hierarchy.classIds[batchId] = classIndex;
                hierarchy.parentIds[batchId] = parentIdMap.at(batchId);

                for (const auto& attribute : attributes)
                {
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
    while (batchTableStr.size() % 8 != 0)
    {
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
    if (!fout)
    {
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    fout.write(reinterpret_cast<const char*>(b3dmBuffer.data()), b3dmBuffer.size());
    if (fout.fail())
    {
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    fout.close();
    return WriteResult::FILE_SAVED;
}

osgDB::ReaderWriter::WriteResult ReaderWriterGLTF::writeI3DMFile(const std::string& filename, const I3DMFile& i3dmFile) const
{
    std::vector<unsigned char> i3dmBuffer;
    i3dmBuffer.insert(i3dmBuffer.end(), i3dmFile.header.magic, i3dmFile.header.magic + 4);
    putVal(i3dmBuffer, i3dmFile.header.version);
    putVal(i3dmBuffer, i3dmFile.header.byteLength);
    putVal(i3dmBuffer, i3dmFile.header.featureTableJSONByteLength);
    putVal(i3dmBuffer, i3dmFile.header.featureTableBinaryByteLength);
    putVal(i3dmBuffer, i3dmFile.header.batchTableJSONByteLength);
    putVal(i3dmBuffer, i3dmFile.header.batchTableBinaryByteLength);
    putVal(i3dmBuffer, i3dmFile.gltfFormat);

    i3dmBuffer.insert(i3dmBuffer.end(), i3dmFile.featureTableJSON.begin(), i3dmFile.featureTableJSON.end());
    i3dmBuffer.insert(i3dmBuffer.end(), i3dmFile.featureTableBinary.begin(), i3dmFile.featureTableBinary.end());
    i3dmBuffer.insert(i3dmBuffer.end(), i3dmFile.batchTableJSON.begin(), i3dmFile.batchTableJSON.end());
    i3dmBuffer.insert(i3dmBuffer.end(), i3dmFile.batchTableBinary.begin(), i3dmFile.batchTableBinary.end());
    i3dmBuffer.insert(i3dmBuffer.end(), i3dmFile.glbData.begin(), i3dmFile.glbData.end());
    i3dmBuffer.insert(i3dmBuffer.end(), i3dmFile.glbPadding, 0);
    i3dmBuffer.insert(i3dmBuffer.end(), i3dmFile.totalPadding, 0);

    std::ofstream fout(filename, std::ios::binary);
    if (!fout)
    {
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    fout.write(reinterpret_cast<const char*>(i3dmBuffer.data()), i3dmBuffer.size());
    if (fout.fail())
    {
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    fout.close();
    return WriteResult::FILE_SAVED;
}

REGISTER_OSGPLUGIN(gltf, ReaderWriterGLTF)
