#include "osgdb_gltf/GltfMerger.h"
#include <osg/Math>
#include <osg/MatrixTransform>
using namespace osgGISPlugins;
void GltfMerger::mergeMeshes()
{
	std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual> matrixPrimitiveMap;
	collectMeshNodes(_model.scenes[0].nodes[0], matrixPrimitiveMap, osg::Matrixd::identity());
	if (matrixPrimitiveMap.size() <= 1)
		return;

	std::vector<tinygltf::Accessor> accessors;
	std::vector<tinygltf::BufferView> bufferViews;
	std::vector<tinygltf::Buffer> buffers;

	for (auto& image : _model.images) {
		tinygltf::BufferView& bufferView = _model.bufferViews[image.bufferView];
		tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];

		bufferView.buffer = buffers.size();
		image.bufferView = bufferViews.size();
		bufferViews.push_back(bufferView);
		buffers.push_back(buffer);
	}
	std::unordered_map<osg::Matrixd, std::vector<tinygltf::Mesh>, MatrixHash, MatrixEqual> matrixMeshMap;
	//std::vector<tinygltf::Mesh> meshes;
	for (const auto& matrixPrimitiveItem : matrixPrimitiveMap) {
		std::map<int, std::vector<tinygltf::Primitive>> materialPrimitiveMap;
		for (auto& primitive : matrixPrimitiveItem.second) {
			materialPrimitiveMap[primitive.material].push_back(primitive);
		}

		for (const auto& materialPrimitiveItem : materialPrimitiveMap) {

			std::map<std::vector<std::string>, std::vector<tinygltf::Primitive>> attributePrimitiveMap;
			for (auto& primitive : materialPrimitiveItem.second) {
				std::vector<std::string> keys;
				for (const auto& pair : primitive.attributes) {
					keys.push_back(pair.first);
				}
				attributePrimitiveMap[keys].push_back(primitive);
			}

			std::vector<tinygltf::Primitive> primitives;
			for (const auto& attributePrimitiveItem : attributePrimitiveMap) {
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
				totalPrimitive.material = materialPrimitiveItem.first;

				unsigned int count = 0;

				for (const auto& prim : attributePrimitiveItem.second) {
					tinygltf::Accessor& oldIndicesAccessor = _model.accessors[prim.indices];
					totalIndicesAccessor = oldIndicesAccessor;
					tinygltf::Accessor& oldVerticesAccessor = _model.accessors[prim.attributes.find("POSITION")->second];
					totalVerticesAccessor = oldVerticesAccessor;
					const auto& normalAttr = prim.attributes.find("NORMAL");
					if (normalAttr != prim.attributes.end()) {
						tinygltf::Accessor& oldNormalsAccessor = _model.accessors[normalAttr->second];
						totalNormalsAccessor = oldNormalsAccessor;
					}
					const auto& texAttr = prim.attributes.find("TEXCOORD_0");
					if (texAttr != prim.attributes.end()) {
						tinygltf::Accessor& oldTexcoordsAccessor = _model.accessors[texAttr->second];
						totalTexcoordsAccessor = oldTexcoordsAccessor;
					}
					const auto& batchidAttr = prim.attributes.find("_BATCHID");
					if (batchidAttr != prim.attributes.end()) {
						tinygltf::Accessor& oldBatchIdsAccessor = _model.accessors[batchidAttr->second];
						totalBatchIdsAccessor = oldBatchIdsAccessor;
					}
					const auto& colorAttr = prim.attributes.find("COLOR_0");
					if (colorAttr != prim.attributes.end()) {
						tinygltf::Accessor& oldColorsAccessor = _model.accessors[colorAttr->second];
						totalColorsAccessor = oldColorsAccessor;
					}
					if (oldIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
						totalIndicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
						break;
					}
					count += oldIndicesAccessor.count;
					if (count > USHRT_MAX) {
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

				unsigned int sum = 0;
				for (const auto& prim : attributePrimitiveItem.second) {

					tinygltf::Accessor& oldVerticesAccessor = _model.accessors[prim.attributes.find("POSITION")->second];
					reindexBufferAndAccessor(oldVerticesAccessor, totalVerticesBufferView, totalVerticesBuffer, totalVerticesAccessor);

					tinygltf::Accessor& oldIndicesAccessor = _model.accessors[prim.indices];
					reindexBufferAndAccessor(oldIndicesAccessor, totalIndicesBufferView, totalIndicesBuffer, totalIndicesAccessor, sum, true);
					sum += oldVerticesAccessor.count;

					const auto& normalAttr = prim.attributes.find("NORMAL");
					if (normalAttr != prim.attributes.end()) {
						tinygltf::Accessor& oldNormalsAccessor = _model.accessors[normalAttr->second];
						reindexBufferAndAccessor(oldNormalsAccessor, totalNormalsBufferView, totalNormalsBuffer, totalNormalsAccessor);
					}
					const auto& texAttr = prim.attributes.find("TEXCOORD_0");
					if (texAttr != prim.attributes.end()) {
						tinygltf::Accessor& oldTexcoordsAccessor = _model.accessors[texAttr->second];
						reindexBufferAndAccessor(oldTexcoordsAccessor, totalTexcoordsBufferView, totalTexcoordsBuffer, totalTexcoordsAccessor);
					}
					const auto& batchidAttr = prim.attributes.find("_BATCHID");
					if (batchidAttr != prim.attributes.end()) {
						tinygltf::Accessor& oldBatchIdsAccessor = _model.accessors[batchidAttr->second];
						reindexBufferAndAccessor(oldBatchIdsAccessor, totalBatchIdsBufferView, totalBatchIdsBuffer, totalBatchIdsAccessor);
					}
					const auto& colorAttr = prim.attributes.find("COLOR_0");
					if (colorAttr != prim.attributes.end()) {
						tinygltf::Accessor& oldColorsAccessor = _model.accessors[colorAttr->second];
						reindexBufferAndAccessor(oldColorsAccessor, totalColorsBufferView, totalColorsBuffer, totalColorsAccessor);
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
			tinygltf::Mesh totalMesh;
			totalMesh.primitives = primitives;
			matrixMeshMap[matrixPrimitiveItem.first].push_back(totalMesh);

		}
	}

	_model.buffers = buffers;
	_model.bufferViews = bufferViews;
	_model.accessors = accessors;
	_model.meshes.clear();
	_model.nodes.clear();
	_model.scenes[0].nodes.clear();
	_model.scenes[0].nodes.push_back(_model.nodes.size());
	_model.nodes.emplace_back();
	tinygltf::Node& root = _model.nodes.back();
	size_t i = 0;
	for (auto it = matrixMeshMap.begin(); it != matrixMeshMap.end(); ++it, ++i)
	{
		if (it->second.size() == 1)
		{
			root.children.push_back(_model.nodes.size());
			_model.nodes.emplace_back();
			tinygltf::Node& node = _model.nodes.back();
			node.mesh = _model.meshes.size();
			_model.meshes.push_back(it->second[0]);

			const osg::Matrixd matrix = it->first;
			if (matrix != osg::Matrix::identity()) {
				decomposeMatrix(matrix, node);
			}
		}
		else
		{
			root.children.push_back(_model.nodes.size());
			_model.nodes.emplace_back();
			tinygltf::Node& parentNode = _model.nodes.back();
			for (const auto& item : it->second)
			{
				parentNode.children.push_back(_model.nodes.size());
				_model.nodes.emplace_back();
				tinygltf::Node& node = _model.nodes.back();
				node.mesh = _model.meshes.size();
				_model.meshes.push_back(item);

				const osg::Matrixd matrix = it->first;
				if (matrix != osg::Matrix::identity()) {
					decomposeMatrix(matrix, node);
				}
			}
		}
	}
}

void GltfMerger::mergeBuffers()
{
	tinygltf::Buffer totalBuffer;
	tinygltf::Buffer fallbackBuffer;
	fallbackBuffer.name = "fallback";
	int byteOffset = 0, fallbackByteOffset = 0;
	for (auto& bufferView : _model.bufferViews) {
		const auto& meshoptExtension = bufferView.extensions.find("EXT_meshopt_compression");
		if (meshoptExtension != bufferView.extensions.end()) {
			auto& bufferIndex = meshoptExtension->second.Get("buffer");
			auto& buffer = _model.buffers[bufferIndex.GetNumberAsInt()];
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
			auto& buffer = _model.buffers[bufferView.buffer];
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
	_model.buffers.clear();
	_model.buffers.push_back(totalBuffer);
	if (fallbackByteOffset > 0) {
		fallbackBuffer.data.resize(fallbackByteOffset);
		tinygltf::Value::Object bufferMeshoptExtension;
		bufferMeshoptExtension.insert(std::make_pair("fallback", true));
		fallbackBuffer.extensions.insert(std::make_pair("EXT_meshopt_compression", tinygltf::Value(bufferMeshoptExtension)));
		_model.buffers.push_back(fallbackBuffer);
	}
}

void GltfMerger::mergeMaterials()
{
	const std::string transformExtensionName = "KHR_texture_transform";
	std::vector<int> textureIndexes;
	for (const auto& mesh : _model.meshes)
	{
		for (const auto& primitive : mesh.primitives)
		{
			auto& material = _model.materials[primitive.material];
			//如果含有emissive、normal、occlusion贴图则不合并
			if (!(material.emissiveTexture.index == -1 &&
				material.normalTexture.index == -1 &&
				material.occlusionTexture.index == -1))
				break;
			//如果含有其他纹理扩展或者metallicRoughness贴图不为空则不合并
			if (!(material.extensions.size() == 0 && material.pbrMetallicRoughness.metallicRoughnessTexture.index == -1))
				break;

			auto& baseColorTexture = material.pbrMetallicRoughness.baseColorTexture;
			//如果没有baseColor贴图则不合并
			if (baseColorTexture.index == -1)
				break;
			//如果没有纹理坐标则不合并
			const auto findTexcoordIndex = primitive.attributes.find("TEXCOORD_0");
			if (findTexcoordIndex == primitive.attributes.end())
				break;
			//如果baseColor贴图没有启用KHR_texture_transform扩展则不合并
			const auto& extensionItem = baseColorTexture.extensions.find(transformExtensionName);
			if (extensionItem == baseColorTexture.extensions.end())
				break;
			const tinygltf::Value extensionVal = extensionItem->second;
			const auto offset = extensionVal.Get("offset").Get<tinygltf::Value::Array>();
			const auto scale = extensionVal.Get("scale").Get<tinygltf::Value::Array>();
			const double offsetX = offset[0].GetNumberAsDouble();
			const double offsetY = offset[1].GetNumberAsDouble();
			const double scaleX = scale[0].GetNumberAsDouble();
			const double scaleY = scale[1].GetNumberAsDouble();

			const int texcoordIndex = findTexcoordIndex->second;
			const tinygltf::Accessor& texcoordAccessor = _model.accessors[texcoordIndex];
			std::vector<float> oldTexcoords = getBufferData<float>(texcoordAccessor);

			if (oldTexcoords.size() != texcoordAccessor.count * 2)
				break;

			bool bResetTexcoords = true;
			osg::ref_ptr<osg::Vec2Array> texcoords = new osg::Vec2Array(texcoordAccessor.count);
			for (size_t j = 0; j < texcoordAccessor.count; ++j)
			{
				const float x = oldTexcoords[2 * j + 0];
				const float y = oldTexcoords[2 * j + 1];
				if (osg::absolute(x) > 1 || osg::absolute(y) > 1)
				{
					bResetTexcoords = false;
					break;
				}
				// 提取结果
				const osg::Vec2 resultUv(x * scaleX + offsetX, y * scaleY + offsetY);
				texcoords->at(j) = (resultUv);
			}

			if (bResetTexcoords) {
				tinygltf::BufferView& texcoordBufferView = _model.bufferViews[texcoordAccessor.bufferView];
				tinygltf::Buffer& buffer = _model.buffers[texcoordBufferView.buffer];
				restoreBuffer(buffer, texcoordBufferView, texcoords);
				textureIndexes.push_back(primitive.material);
			}
		}
	}
	for (const auto index : textureIndexes)
	{
		_model.materials[index].pbrMetallicRoughness.baseColorTexture.extensions.clear();
	}

	std::vector<tinygltf::Material> newMaterials;
	std::map<int, int> materialRemap;

	for (size_t i = 0; i < _model.materials.size(); i++)
	{
		auto& oldMaterial = _model.materials.at(i);

		int index = -1;
		for (size_t j = 0; j < newMaterials.size(); j++)
		{
			auto& newMateiral = newMaterials.at(j);
			if (newMateiral == oldMaterial)
				index = j;
		}
		if (index == -1)
		{
			index = newMaterials.size();
			newMaterials.push_back(oldMaterial);
		}
		materialRemap[i] = index;
	}

	_model.materials = newMaterials;
	for (auto& mesh : _model.meshes)
	{
		for (auto& primitive : mesh.primitives)
		{
			primitive.material = materialRemap[primitive.material];
		}
	}
}

osg::Matrixd GltfMerger::convertGltfNodeToOsgMatrix(const tinygltf::Node& node)
{
	osg::Matrixd osgMatrix;
	osgMatrix.makeIdentity();

	if (!node.matrix.empty()) {
		osgMatrix.set(node.matrix.data());
	}
	else {
		osg::Vec3d translation(0.0, 0.0, 0.0);
		osg::Quat rotation(0.0, 0.0, 0.0, 1.0);
		osg::Vec3d scale(1.0, 1.0, 1.0);

		if (!node.translation.empty()) {
			translation.set(node.translation[0], node.translation[1], node.translation[2]);
		}
		if (!node.rotation.empty()) {
			rotation.set(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
		}
		if (!node.scale.empty()) {
			scale.set(node.scale[0], node.scale[1], node.scale[2]);
		}

		osgMatrix.preMultTranslate(translation);
		osgMatrix.preMultRotate(rotation);
		osgMatrix.preMultScale(scale);
	}

	return osgMatrix;
}

void GltfMerger::decomposeMatrix(const osg::Matrixd& matrix, tinygltf::Node& node)
{
	osg::Vec3d translation;
	osg::Quat rotation;
	osg::Vec3d scale;
	osg::Quat scaleOrientation;

	matrix.decompose(translation, rotation, scale, scaleOrientation);

	node.translation = { translation.x(), translation.y(), translation.z() };
	node.scale = { scale.x(), scale.y(), scale.z() };
	osg::Vec4 rotationVec = rotation.asVec4();
	node.rotation = { rotationVec.x(), rotationVec.y(), rotationVec.z(), rotationVec.w() };
}

void GltfMerger::apply()
{
	if (_bMergeMaterials)
		mergeMaterials();
	if(_bMergeMeshes)
		mergeMeshes();
}

void GltfMerger::collectMeshNodes(size_t index, std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual>& matrixPrimitiveMap, osg::Matrixd& matrix)
{
	const tinygltf::Node& node = _model.nodes[index];
	matrix.preMult(convertGltfNodeToOsgMatrix(node));

	if (node.mesh == -1) {
		for (size_t childIndex : node.children) {
			collectMeshNodes(childIndex, matrixPrimitiveMap, osg::Matrixd(matrix));
		}
	}
	else {
		const tinygltf::Mesh& mesh = _model.meshes[node.mesh];
		matrixPrimitiveMap[matrix].insert(matrixPrimitiveMap[matrix].end(), mesh.primitives.begin(), mesh.primitives.end());
	}
}

void GltfMerger::reindexBufferAndAccessor(const tinygltf::Accessor& accessor, tinygltf::BufferView& tBv, tinygltf::Buffer& tBuffer, tinygltf::Accessor& newAccessor, unsigned int sum, bool isIndices)
{
	tinygltf::BufferView& bv = _model.bufferViews[accessor.bufferView];
	tinygltf::Buffer& buffer = _model.buffers[bv.buffer];

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
}
