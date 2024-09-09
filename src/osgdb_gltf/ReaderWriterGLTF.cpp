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
			b3dmFile.featureTableJSON = createFeatureTableJSON(center, bthv.getBatchLength());
			b3dmFile.batchTableJSON = createBatchTableJSON(bthv);
			b3dmFile.glbData = gltfBuf.str();
			b3dmFile.calculateHeaderSizes();
			nc_node.unref_nodelete();
			return writeB3DMFile(osgDB::convertStringFromUTF8toCurrentCodePage(filename), b3dmFile);
		}
		else
		{ 
			I3DMFile i3dmFile;

			i3dmFile.featureTableJSON = createFeatureI3DMTableJSON(nc_node.asGroup()->getNumChildren());
			i3dmFile.featureTableBinary = createFeatureI3DMTableBinary(nc_node.asGroup());
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

std::string ReaderWriterGLTF::createFeatureI3DMTableJSON(const unsigned int length) const
{
	json featureTable;
	featureTable["INSTANCES_LENGTH"] = length;

	// 更新JSON以包含byteLength  
	featureTable["POSITION_QUANTIZED"]["byteLength"] = length * sizeof(uint16_t);
	featureTable["NORMAL_UP_OCT32P"]["byteLength"] = length * sizeof(uint16_t);
	featureTable["NORMAL_RIGHT_OCT32P"]["byteLength"] = length * sizeof(uint16_t);
	featureTable["SCALE_NON_UNIFORM"]["byteLength"] = length * sizeof(float);
	featureTable["BATCH_ID"]["byteLength"] = length * sizeof(uint32_t);



	std::string featureTableStr = featureTable.dump();
	while ((featureTableStr.size() + 28) % 8 != 0) {
		featureTableStr.push_back(' ');
	}
	return featureTableStr;
}

std::string ReaderWriterGLTF::createFeatureI3DMTableBinary(osg::ref_ptr<osg::Group> matrixTransforms) const
{
	const unsigned int length = matrixTransforms->getNumChildren();
	// 存储实例的量化位置、法线和缩放等数据
	std::vector<int> positionQuantized;
	std::vector<int> normalUpOct32P;
	std::vector<int> normalRightOct32P;
	std::vector<float> scaleNonUniform;
	std::vector<unsigned int> batchID;

	osg::Vec3f positionMin(FLT_MAX, FLT_MAX, FLT_MAX);
	osg::Vec3f positionMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t i = 0; i < length; ++i)
	{
		osg::ref_ptr<osg::MatrixTransform> matrixTransform = dynamic_cast<osg::MatrixTransform*>(matrixTransforms->getChild(i));

		if (matrixTransform.valid())
		{
			const osg::Vec3f position = matrixTransform->getMatrix().getTrans();
			positionMin.x() = positionMin.x() < position.x() ? positionMin.x() : position.x();
			positionMin.y() = positionMin.y() < position.y() ? positionMin.y() : position.y();
			positionMin.z() = positionMin.z() < position.z() ? positionMin.z() : position.z();

			positionMax.x() = positionMax.x() > position.x() ? positionMax.x() : position.x();
			positionMax.y() = positionMax.y() > position.y() ? positionMax.y() : position.y();
			positionMax.z() = positionMax.z() > position.z() ? positionMax.z() : position.z();

		}
	}


	// 假设QUANTIZED_VOLUME_OFFSET和QUANTIZED_VOLUME_SCALE已经预定义
	osg::Vec3f volumeOffset = positionMin;
	osg::Vec3f volumeScale = positionMax - positionMin;
	for (size_t i = 0; i < length; ++i)
	{
		osg::ref_ptr<osg::MatrixTransform> matrixTransform = dynamic_cast<osg::MatrixTransform*>(matrixTransforms->getChild(i));

		if (matrixTransform.valid())
		{
			// 获取位置信息，并进行量化
			const osg::Vec3f position = matrixTransform->getMatrix().getTrans();
			positionQuantized.push_back(static_cast<int>(((position.x() - volumeOffset.x()) / volumeScale.x()) * 65535));
			positionQuantized.push_back(static_cast<int>(((position.y() - volumeOffset.y()) / volumeScale.y()) * 65535));
			positionQuantized.push_back(static_cast<int>(((position.z() - volumeOffset.z()) / volumeScale.z()) * 65535));

			// 提取朝向并进行OCT32P编码
			const osg::Vec3f up = matrixTransform->getMatrix().getRotate() * osg::Vec3f(0.0f, 0.0f, 1.0f);  // 上方向
			const osg::Vec3f right = matrixTransform->getMatrix().getRotate() * osg::Vec3f(1.0f, 0.0f, 0.0f);  // 右方向

			auto encodedUp = osgGISPlugins::I3DMFile::encodeNormalToOct32P(up);
			auto encodedRight = osgGISPlugins::I3DMFile::encodeNormalToOct32P(right);
			normalUpOct32P.push_back(encodedUp.first);
			normalUpOct32P.push_back(encodedUp.second);
			normalRightOct32P.push_back(encodedRight.first);
			normalRightOct32P.push_back(encodedRight.second);

			// 提取缩放信息
			const osg::Vec3f scale = matrixTransform->getMatrix().getScale();
			scaleNonUniform.push_back(scale.x());
			scaleNonUniform.push_back(scale.y());
			scaleNonUniform.push_back(scale.z());

			// 记录批次ID
			batchID.push_back(i);

		}
	}


	// 写入二进制数据的长度
	//size_t totalBinarySize =
	//	length * sizeof(uint16_t) +
	//	length * sizeof(uint16_t) +
	//	length * sizeof(uint16_t) +
	//	length * sizeof(float) +
	//	length * sizeof(uint32_t);

	std::ostringstream oss;

	// 写入二进制数据的长度（这部分通常是可选的，取决于接收方是否需要预先知道数据长度）  
	// 但这里我们直接写入数据，不单独写长度  

	// 序列化数据  
	for (size_t i = 0; i < length; ++i) {
		// 写入位置（量化后的整数）  
		oss.write(reinterpret_cast<const char*>(&positionQuantized[i * 3 + 0]), sizeof(int));
		oss.write(reinterpret_cast<const char*>(&positionQuantized[i * 3 + 1]), sizeof(int));
		oss.write(reinterpret_cast<const char*>(&positionQuantized[i * 3 + 2]), sizeof(int));

		// 写入上向量和右向量的OCT32P编码  
		oss.write(reinterpret_cast<const char*>(&normalUpOct32P[i * 2 + 0]), sizeof(int));
		oss.write(reinterpret_cast<const char*>(&normalUpOct32P[i * 2 + 1]), sizeof(int));
		oss.write(reinterpret_cast<const char*>(&normalRightOct32P[i * 2 + 0]), sizeof(int));
		oss.write(reinterpret_cast<const char*>(&normalRightOct32P[i * 2 + 1]), sizeof(int));

		// 写入缩放信息（假设为float）  
		oss.write(reinterpret_cast<const char*>(&scaleNonUniform[i * 3 + 0]), sizeof(float));
		oss.write(reinterpret_cast<const char*>(&scaleNonUniform[i * 3 + 1]), sizeof(float));
		oss.write(reinterpret_cast<const char*>(&scaleNonUniform[i * 3 + 2]), sizeof(float));

		// 写入批次ID（假设为uint32_t）  
		oss.write(reinterpret_cast<const char*>(&batchID[i]), sizeof(unsigned int));
	}

	// 将ostringstream转换为string  
	return oss.str();
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

osgDB::ReaderWriter::WriteResult ReaderWriterGLTF::writeI3DMFile(const std::string& filename, const I3DMFile& b3dmFile) const
{
	return WriteResult();
}

REGISTER_OSGPLUGIN(gltf, ReaderWriterGLTF)
