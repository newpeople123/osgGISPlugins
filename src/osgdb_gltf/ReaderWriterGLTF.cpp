#include <osgDB/Options>
#include <osgDB/Registry>
#include <osgdb_gltf/ReaderWriterGLTF.h>
#include <osgDB/FileNameUtils>
#include "osgdb_gltf/Osg2Gltf.h"
#include "osgdb_gltf/GltfProcessorManager.h"
#include <nlohmann/json.hpp>
#include <osgDB/ConvertUTF>
#include <osg/MatrixTransform>
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
	if (ext == "b3dm" || ext == "i3dm")
		nc_node.accept(bthv);

	Osg2Gltf osg2gltf;
	nc_node.accept(osg2gltf);
	tinygltf::Model gltfModel = osg2gltf.getGltfModel();

	const bool embedImages = true;
	bool embedBuffers = false, prettyPrint = false, isBinary = ext != "gltf", quantize = false, mergeMaterial = true, mergeMesh = true;
	GltfDracoCompressor::DracoCompressionOptions dracoCompressOption;
	GltfMeshQuantizeCompressor::MeshQuantizeCompressionOptions quantizeCompressOption;
	GltfProcessorManager processorManager;

	if (options) {
		std::istringstream iss(options->getOptionString());
		std::string opt;

		while (iss >> opt) {
			if (opt == "quantize") {
				quantize = true;
			}
			else if (opt == "eb") {
				embedBuffers = true;
			}
			else if (opt == "pp") {
				prettyPrint = true;
			}
			else if (opt == "noMergeMaterial")
				mergeMaterial = false;
			else if (opt == "noMergeMesh")
				mergeMesh = false;

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
						processorManager.addProcessor(new GltfMeshQuantizeCompressor(gltfModel, quantizeCompressOption));
					}
					else {
						processorManager.addProcessor(new GltfDracoCompressor(gltfModel, dracoCompressOption));
					}
				}
				else if (compressionTypeStr == "meshopt") {
					if (quantize) {
						processorManager.addProcessor(new GltfMeshQuantizeCompressor(gltfModel, quantizeCompressOption));
					}
					processorManager.addProcessor(new GltfMeshOptCompressor(gltfModel));
				}
			}
		}
	}

	GltfMerger* gltfMerger = new GltfMerger(gltfModel, mergeMaterial, mergeMesh);
	processorManager.addProcessor(gltfMerger);
	processorManager.process();

	tinygltf::TinyGLTF writer;
	if (ext != "b3dm" && ext != "i3dm") {
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
	else {
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
			osg::Vec3f positionMin(FLT_MAX, FLT_MAX, FLT_MAX);
			osg::Vec3f positionMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			osg::ref_ptr<osg::Group> matrixTransforms = nc_node.asGroup();
			const size_t length = matrixTransforms->getNumChildren();
			for (size_t i = 0; i < length; ++i)
			{
				osg::ref_ptr<osg::MatrixTransform> matrixTransform = dynamic_cast<osg::MatrixTransform*>(matrixTransforms->getChild(i));

				if (matrixTransform.valid())
				{
					const osg::Vec3f position = matrixTransform->getMatrix().getTrans();
					positionMin.set(osg::minimum(positionMin.x(), position.x()), osg::minimum(positionMin.y(), position.y()), osg::minimum(positionMin.z(), position.z()));
					positionMax.set(osg::maximum(positionMax.x(), position.x()), osg::maximum(positionMax.y(), position.y()), osg::maximum(positionMax.z(), position.z()));
				}
			}
			const osg::Vec3f volumeOffset = positionMin;
			const osg::Vec3f volumeScale = positionMax - positionMin;

			i3dmFile.featureTableJSON = createFeatureI3DMTableJSON(length, volumeOffset, volumeScale);
			i3dmFile.featureTableBinary = createFeatureI3DMTableBinary(matrixTransforms, volumeOffset, volumeScale);
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
	while ((featureTableStr.size() + 28) % 8 != 0) {
		featureTableStr.push_back(' ');
	}
	return featureTableStr;
}

std::string ReaderWriterGLTF::createFeatureI3DMTableJSON(const unsigned int length, const osg::Vec3 volumeOffset, const osg::Vec3 volumeScale) const
{
	json featureTable;
	featureTable["INSTANCES_LENGTH"] = length;

	// 量化体积偏移和比例
	featureTable["QUANTIZED_VOLUME_OFFSET"] = { volumeOffset.x(), volumeOffset.y(), volumeOffset.z() };
	featureTable["QUANTIZED_VOLUME_SCALE"] = { volumeScale.x(), volumeScale.y(), volumeScale.z() };

	// 计算每个字段的 byteOffset 和 byteLength
	size_t byteOffset = 0;

	// POSITION_QUANTIZED
	featureTable["POSITION_QUANTIZED"]["byteOffset"] = byteOffset;
	featureTable["POSITION_QUANTIZED"]["byteLength"] = length * 3 * sizeof(uint16_t);
	featureTable["POSITION_QUANTIZED"]["componentType"] = "UNSIGNED_SHORT"; // uint16_t

	byteOffset += length * 3 * sizeof(uint16_t);  // 3 components for each position
	byteOffset += (4 - (byteOffset % 4)) % 4;

	// NORMAL_UP_OCT32P
	featureTable["NORMAL_UP_OCT32P"]["byteOffset"] = byteOffset;
	featureTable["NORMAL_UP_OCT32P"]["byteLength"] = length * 2 * sizeof(uint16_t);
	featureTable["NORMAL_UP_OCT32P"]["componentType"] = "UNSIGNED_SHORT"; // uint16_t

	byteOffset += length * 2 * sizeof(uint16_t);  // 2 components for normal UP

	// NORMAL_RIGHT_OCT32P
	featureTable["NORMAL_RIGHT_OCT32P"]["byteOffset"] = byteOffset;
	featureTable["NORMAL_RIGHT_OCT32P"]["byteLength"] = length * 2 * sizeof(uint16_t);
	featureTable["NORMAL_RIGHT_OCT32P"]["componentType"] = "UNSIGNED_SHORT"; // uint16_t

	byteOffset += length * 2 * sizeof(uint16_t);  // 2 components for normal RIGHT

	// SCALE_NON_UNIFORM
	featureTable["SCALE_NON_UNIFORM"]["byteOffset"] = byteOffset;
	featureTable["SCALE_NON_UNIFORM"]["byteLength"] = length * 3 * sizeof(float);
	featureTable["SCALE_NON_UNIFORM"]["componentType"] = "FLOAT"; // float

	byteOffset += length * 3 * sizeof(float);  // 3 components for scale
	byteOffset += (4 - (byteOffset % 4)) % 4;

	// BATCH_ID
	featureTable["BATCH_ID"]["byteOffset"] = byteOffset;
	featureTable["BATCH_ID"]["byteLength"] = length * sizeof(uint32_t);
	featureTable["BATCH_ID"]["componentType"] = "UNSIGNED_INT"; // uint32_t

	// 调整 JSON 字符串长度到 8 字节对齐
	std::string featureTableStr = featureTable.dump();
	while (featureTableStr.size() % 8 != 0) {
		featureTableStr.push_back(' ');
	}

	return featureTableStr;
}

std::string ReaderWriterGLTF::createFeatureI3DMTableBinary(osg::ref_ptr<osg::Group> matrixTransforms, const osg::Vec3 volumeOffset, const osg::Vec3 volumeScale) const
{
	const unsigned int length = matrixTransforms->getNumChildren();

	// 预先分配足够的内存空间，避免动态扩展
	std::vector<uint16_t> positionQuantized(length * 3);
	std::vector<uint16_t> normalUpOct32P(length * 2);
	std::vector<uint16_t> normalRightOct32P(length * 2);
	std::vector<float> scaleNonUniform(length * 3);
	std::vector<uint32_t> batchID(length);

	for (unsigned int i = 0; i < length; ++i)
	{
		osg::ref_ptr<osg::MatrixTransform> matrixTransform = dynamic_cast<osg::MatrixTransform*>(matrixTransforms->getChild(i));

		if (!matrixTransform.valid()) {
			continue;  // 跳过无效节点，增加健壮性
		}

		// 获取位置信息并进行量化
		const osg::Vec3f position = matrixTransform->getMatrix().getTrans();
		positionQuantized[i * 3] = static_cast<uint16_t>(((position.x() - volumeOffset.x()) / volumeScale.x()) * 65535);
		positionQuantized[i * 3 + 1] = static_cast<uint16_t>(((position.y() - volumeOffset.y()) / volumeScale.y()) * 65535);
		positionQuantized[i * 3 + 2] = static_cast<uint16_t>(((position.z() - volumeOffset.z()) / volumeScale.z()) * 65535);

		// 进行OCT32P编码
		const osg::Vec3f up = matrixTransform->getMatrix().getRotate() * osg::Vec3f(0.0f, 0.0f, 1.0f);  // 上方向
		const osg::Vec3f right = matrixTransform->getMatrix().getRotate() * osg::Vec3f(1.0f, 0.0f, 0.0f);  // 右方向

		auto encodedUp = osgGISPlugins::I3DMFile::encodeNormalToOct32P(up);
		auto encodedRight = osgGISPlugins::I3DMFile::encodeNormalToOct32P(right);

		normalUpOct32P[i * 2] = encodedUp.first;
		normalUpOct32P[i * 2 + 1] = encodedUp.second;
		normalRightOct32P[i * 2] = encodedRight.first;
		normalRightOct32P[i * 2 + 1] = encodedRight.second;

		// 提取缩放信息
		const osg::Vec3f scale = matrixTransform->getMatrix().getScale();
		scaleNonUniform[i * 3] = scale.x();
		scaleNonUniform[i * 3 + 1] = scale.y();
		scaleNonUniform[i * 3 + 2] = scale.z();

		// 记录批次ID
		batchID[i] = i;
	}

	// 提前分配内存
	std::ostringstream oss;
	oss.write(reinterpret_cast<const char*>(positionQuantized.data()), length * 3 * sizeof(uint16_t));//6字节*length
	//4字节对齐
	// 计算当前写入的总字节数
	size_t currentSize = oss.tellp();
	size_t padding = (4 - (currentSize % 4)) % 4;  // 计算需要的填充字节数
	oss.write(std::string(padding, 0).c_str(), padding);  // 写入填充字节

	oss.write(reinterpret_cast<const char*>(normalUpOct32P.data()), length * 2 * sizeof(uint16_t));
	oss.write(reinterpret_cast<const char*>(normalRightOct32P.data()), length * 2 * sizeof(uint16_t));

	oss.write(reinterpret_cast<const char*>(scaleNonUniform.data()), length * 3 * sizeof(float));
	// BATCH_ID must be aligned to a 4-byte boundary
	// 计算当前写入的总字节数
	currentSize = oss.tellp();
	// 确保 BATCH_ID 的写入位置是 4 字节对齐的
	padding = (4 - (currentSize % 4)) % 4;  // 计算需要的填充字节数
	oss.write(std::string(padding, 0).c_str(), padding);  // 写入填充字节
	oss.write(reinterpret_cast<const char*>(batchID.data()), length * sizeof(uint32_t));

	std::string featureTableBinaryStr = oss.str();
	// 调整 JSON 字符串长度到 8 字节对齐
	while (featureTableBinaryStr.size() % 8 != 0) {
		featureTableBinaryStr.push_back(' ');
	}

	return featureTableBinaryStr;
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
	if (!fout) {
		return WriteResult::ERROR_IN_WRITING_FILE;
	}

	fout.write(reinterpret_cast<const char*>(i3dmBuffer.data()), i3dmBuffer.size());
	if (fout.fail()) {
		return WriteResult::ERROR_IN_WRITING_FILE;
	}

	fout.close();
	return WriteResult::FILE_SAVED;
}

REGISTER_OSGPLUGIN(gltf, ReaderWriterGLTF)
