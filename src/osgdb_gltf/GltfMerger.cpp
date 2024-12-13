#include "osgdb_gltf/GltfMerger.h"
#include <osg/Math>
#include <osg/MatrixTransform>
#include <algorithm>
#include "osgdb_gltf/compress/GltfMeshQuantizeCompressor.h"

using namespace osgGISPlugins;
void GltfMerger::mergeMeshes()
{
	// 矩阵对应的primitive
	std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual> matrixPrimitiveMap;
	collectMeshNodes(_model.scenes[0].nodes[0], matrixPrimitiveMap);
	//if (matrixPrimitiveMap.size() <= 1)
	//	return;

	// 新的accessor、bufferView和buffer
	std::vector<tinygltf::Accessor> newAccessors;
	std::vector<tinygltf::BufferView> newBufferViews;
	std::vector<tinygltf::Buffer> newBuffers;

	//复制图像相关的bufferView和buffer
	for (tinygltf::Image& image : _model.images) {
		tinygltf::BufferView& bufferView = _model.bufferViews[image.bufferView];
		tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];

		bufferView.buffer = newBuffers.size();
		image.bufferView = newBufferViews.size();
		newBufferViews.push_back(bufferView);
		newBuffers.push_back(buffer);
	}
	//构建一个新的矩阵到mesh的映射
	std::unordered_map<osg::Matrixd, std::vector<tinygltf::Mesh>, MatrixHash, MatrixEqual> matrixMeshMap;

	for (const auto& matrixPrimitiveItem : matrixPrimitiveMap) {
		// 材质对应的primitve集合(此时这些primitive都有相同的变换矩阵)
		std::map<int, std::vector<tinygltf::Primitive>> materialPrimitiveMap;
		for (const tinygltf::Primitive& primitive : matrixPrimitiveItem.second) {

			materialPrimitiveMap[primitive.material].push_back(primitive);
		}

		for (const auto& materialPrimitiveItem : materialPrimitiveMap) {
			std::vector<tinygltf::Mesh> combinedMeshes = mergePrimitives(materialPrimitiveItem, newAccessors, newBufferViews, newBuffers);
			for (const tinygltf::Mesh& combinedMesh : combinedMeshes)
				matrixMeshMap[matrixPrimitiveItem.first].push_back(combinedMesh);

		}
	}

	_model.buffers = newBuffers;
	_model.bufferViews = newBufferViews;
	_model.accessors = newAccessors;
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
			for (const tinygltf::Mesh& mesh : it->second)
			{
				parentNode.children.push_back(_model.nodes.size());
				_model.nodes.emplace_back();
				tinygltf::Node& node = _model.nodes.back();
				node.mesh = _model.meshes.size();
				_model.meshes.push_back(mesh);

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
	for (tinygltf::BufferView& bufferView : _model.bufferViews) {
		const auto& meshoptExtension = bufferView.extensions.find("EXT_meshopt_compression");
		if (meshoptExtension != bufferView.extensions.end()) {
			const tinygltf::Value& bufferIndex = meshoptExtension->second.Get("buffer");
			tinygltf::Buffer& buffer = _model.buffers[bufferIndex.GetNumberAsInt()];
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
			const int fallbackNeedAdd = (4 - (fallbackByteOffset % 4)) % 4;
			if (fallbackNeedAdd != 0) {
				for (int i = 0; i < fallbackNeedAdd; ++i) {
					fallbackByteOffset++;
				}
			}
		}
		else {
			tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
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
	std::vector<tinygltf::Material> newMaterials;

	std::vector<tinygltf::Material> materials;
	std::set<int> uniqueNoMergeMaterialIndex, uniqueMergeMaterialIndex;
	for (const tinygltf::Mesh& mesh : _model.meshes)
	{
		for (const tinygltf::Primitive& primitive : mesh.primitives)
		{
			if (primitive.material == -1) continue;
			tinygltf::Material material = _model.materials[primitive.material];
			//如果含有emissive、normal、occlusion贴图则不合并
			if (!(material.emissiveTexture.index == -1 &&
				material.normalTexture.index == -1 &&
				material.occlusionTexture.index == -1))

			{
				uniqueNoMergeMaterialIndex.insert(primitive.material);
				continue;
			}
			//如果含有其他纹理扩展或者metallicRoughness贴图不为空则不合并
			if (!(material.extensions.size() == 0 && material.pbrMetallicRoughness.metallicRoughnessTexture.index == -1))
			{
				uniqueNoMergeMaterialIndex.insert(primitive.material);
				continue;
			}

			tinygltf::TextureInfo& baseColorTexture = material.pbrMetallicRoughness.baseColorTexture;
			//如果没有baseColor贴图则不合并
			if (baseColorTexture.index == -1)
			{
				uniqueNoMergeMaterialIndex.insert(primitive.material);
				continue;
			}
			//如果没有纹理坐标则不合并
			const auto findTexcoordIndex = primitive.attributes.find("TEXCOORD_0");
			if (findTexcoordIndex == primitive.attributes.end())
			{
				uniqueNoMergeMaterialIndex.insert(primitive.material);
				tinygltf::Material& localMaterial = _model.materials[primitive.material];
				tinygltf::TextureInfo& localBaseColorTexture = localMaterial.pbrMetallicRoughness.baseColorTexture;
				localBaseColorTexture.extensions.erase(transformExtensionName);
				continue;
			}
			//如果baseColor贴图没有启用KHR_texture_transform扩展则不合并
			const auto& extensionItem = baseColorTexture.extensions.find(transformExtensionName);
			if (extensionItem == baseColorTexture.extensions.end())
			{
				uniqueNoMergeMaterialIndex.insert(primitive.material);
				continue;
			}

			const int texcoordIndex = findTexcoordIndex->second;
			const tinygltf::Accessor& texcoordAccessor = _model.accessors[texcoordIndex];
			std::vector<float> oldTexcoords = getBufferData<float>(texcoordAccessor);

			if (oldTexcoords.size() != texcoordAccessor.count * 2)
			{
				uniqueNoMergeMaterialIndex.insert(primitive.material);
				continue;
			}

			osg::ref_ptr<osg::Vec2Array> texcoords = new osg::Vec2Array(texcoordAccessor.count);
			for (size_t j = 0; j < texcoordAccessor.count; ++j)
			{
				const float x = oldTexcoords[2 * j + 0];
				const float y = oldTexcoords[2 * j + 1];
				if (osg::absolute(x) > 1 || osg::absolute(y) > 1)
				{
					uniqueNoMergeMaterialIndex.insert(primitive.material);
					continue;
				}
			}
		}

	}

	for (const tinygltf::Mesh& mesh : _model.meshes)
	{
		for (const tinygltf::Primitive& primitive : mesh.primitives)
		{
			if (primitive.material == -1) continue;
			if (uniqueNoMergeMaterialIndex.find(primitive.material) == uniqueNoMergeMaterialIndex.end())
			{
				tinygltf::Material material = _model.materials[primitive.material];
				materials.push_back(material);
				uniqueMergeMaterialIndex.insert(primitive.material);
			}
		}
	}

	for (size_t i = 0; i < materials.size(); i++)
	{
		tinygltf::Material& oldMaterial = materials.at(i);
		oldMaterial.pbrMetallicRoughness.baseColorTexture.extensions.clear();

		bool found = false;
		for (size_t j = 0; j < newMaterials.size(); j++)
		{
			const tinygltf::Material& newMaterial = newMaterials.at(j);
			if (newMaterial == oldMaterial)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			newMaterials.push_back(oldMaterial);
		}
	}


	if (newMaterials.size() + uniqueNoMergeMaterialIndex.size() == _model.materials.size())
		return;

	newMaterials.clear();
	std::map<int, int> materialRemap;


	for (const tinygltf::Mesh& mesh : _model.meshes)
	{
		for (const tinygltf::Primitive& primitive : mesh.primitives)
		{
			if (primitive.material == -1) continue;
			if (uniqueMergeMaterialIndex.find(primitive.material) == uniqueMergeMaterialIndex.end()) continue;

			tinygltf::Material& material = _model.materials[primitive.material];

			tinygltf::TextureInfo& baseColorTexture = material.pbrMetallicRoughness.baseColorTexture;
			//如果没有纹理坐标则不合并
			const auto findTexcoordIndex = primitive.attributes.find("TEXCOORD_0");
			//如果baseColor贴图没有启用KHR_texture_transform扩展则不合并
			const auto& extensionItem = baseColorTexture.extensions.find(transformExtensionName);
			const tinygltf::Value extensionVal = extensionItem->second;
			const tinygltf::Value::Array offset = extensionVal.Get("offset").Get<tinygltf::Value::Array>();
			const tinygltf::Value::Array scale = extensionVal.Get("scale").Get<tinygltf::Value::Array>();
			const double offsetX = offset[0].GetNumberAsDouble();
			const double offsetY = offset[1].GetNumberAsDouble();
			const double scaleX = scale[0].GetNumberAsDouble();
			const double scaleY = scale[1].GetNumberAsDouble();

			const int texcoordIndex = findTexcoordIndex->second;
			tinygltf::Accessor& texcoordAccessor = _model.accessors[texcoordIndex];
			texcoordAccessor.maxValues[0] = -FLT_MAX;
			texcoordAccessor.maxValues[1] = -FLT_MAX;
			texcoordAccessor.minValues[0] = FLT_MAX;
			texcoordAccessor.minValues[1] = FLT_MAX;

			std::vector<float> oldTexcoords = getBufferData<float>(texcoordAccessor);

			osg::ref_ptr<osg::Vec2Array> texcoords = new osg::Vec2Array(texcoordAccessor.count);
			for (size_t j = 0; j < texcoordAccessor.count; ++j)
			{
				const float x = oldTexcoords[2 * j + 0] * scaleX + offsetX;
				const float y = oldTexcoords[2 * j + 1] * scaleY + offsetY;
				if (texcoordAccessor.maxValues.size())
				{
					texcoordAccessor.maxValues[0] = osg::maximum(texcoordAccessor.maxValues[0], (double)x);
					texcoordAccessor.maxValues[1] = osg::maximum(texcoordAccessor.maxValues[1], (double)y);

					texcoordAccessor.minValues[0] = osg::minimum(texcoordAccessor.minValues[0], (double)x);
					texcoordAccessor.minValues[1] = osg::minimum(texcoordAccessor.minValues[1], (double)y);
				}
				// 提取结果
				const osg::Vec2 resultUv(x, y);
				texcoords->at(j) = (resultUv);
			}
			tinygltf::BufferView& texcoordBufferView = _model.bufferViews[texcoordAccessor.bufferView];
			tinygltf::Buffer& buffer = _model.buffers[texcoordBufferView.buffer];
			restoreBuffer(buffer, texcoordBufferView, texcoords);
		}
	}

	for (const int index : uniqueMergeMaterialIndex)
	{
		_model.materials[index].pbrMetallicRoughness.baseColorTexture.extensions.clear();
	}

	for (size_t i = 0; i < _model.materials.size(); i++)
	{
		tinygltf::Material& oldMaterial = _model.materials.at(i);

		int index = -1;
		for (size_t j = 0; j < newMaterials.size(); j++)
		{
			const tinygltf::Material& newMateiral = newMaterials.at(j);
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
	for (tinygltf::Mesh& mesh : _model.meshes)
	{
		for (tinygltf::Primitive& primitive : mesh.primitives)
		{
			if (primitive.material != -1)
				primitive.material = materialRemap[primitive.material];
		}
	}
}

std::vector<tinygltf::Mesh> GltfMerger::mergePrimitives(const std::pair<int, std::vector<tinygltf::Primitive>>& materialWithPrimitves, std::vector<tinygltf::Accessor>& newAccessors, std::vector<tinygltf::BufferView>& newBufferViews, std::vector<tinygltf::Buffer>& newBuffers)
{
	std::vector<tinygltf::Mesh> combinedMeshes;

	std::vector<tinygltf::Primitive> trianglesModePrimitives;
	std::vector<tinygltf::Primitive> otherModePrimitives;

	for (const tinygltf::Primitive& primitive : materialWithPrimitves.second) {
		if (primitive.mode != TINYGLTF_MODE_TRIANGLES || primitive.indices == -1)
		{
			otherModePrimitives.push_back(primitive);
		}
		else
		{
			trianglesModePrimitives.push_back(primitive);
		}
	}

	// mode模式不是triangles的，不合并
	for (tinygltf::Primitive& primitive : otherModePrimitives)
	{
		for (const auto& item : primitive.attributes)
		{
			const std::string& name = item.first;
			tinygltf::Accessor oldAccessor = _model.accessors[primitive.attributes.at(name)];
			tinygltf::BufferView oldBufferView = _model.bufferViews[oldAccessor.bufferView];
			tinygltf::Buffer oldBuffer = _model.buffers[oldBufferView.buffer];

			oldBufferView.buffer = newBuffers.size();
			newBuffers.push_back(oldBuffer);

			oldAccessor.bufferView = newBufferViews.size();
			newBufferViews.push_back(oldBufferView);

			primitive.attributes.at(name) = newAccessors.size();
			newAccessors.push_back(oldAccessor);

		}

		tinygltf::Mesh newMesh;
		newMesh.primitives = { primitive };
		combinedMeshes.push_back(newMesh);
	}

	// map，用于按属性名收集相同的 primitive
	std::map<std::vector<std::string>, std::vector<tinygltf::Primitive>> groupedPrimitives;
	for (const tinygltf::Primitive& primitive : trianglesModePrimitives)
	{
		// 收集当前 primitive 的所有属性名
		std::vector<std::string> attributeNames;
		for (const auto& attribute : primitive.attributes) {
			if (primitive.attributes.at(attribute.first) > -1)
				attributeNames.push_back(attribute.first);
		}

		// 对属性名排序，以确保相同的属性名顺序一致
		std::sort(attributeNames.begin(), attributeNames.end());

		// 将具有相同属性的 Primitive 分组
		groupedPrimitives[attributeNames].push_back(primitive);

	}

	for (const auto& group : groupedPrimitives) {
		const std::vector<std::string>& attributeNames = group.first; // 属性名列表
		const std::vector<tinygltf::Primitive>& primitives = group.second; // 相同属性的 primitive

		tinygltf::Primitive combinedPrimitive;

		for (const std::string& name : attributeNames)
		{
			tinygltf::Accessor combinedAttributeAccessor;
			tinygltf::BufferView combinedAttributeBV;
			tinygltf::Buffer combinedAttributeBuffer;
			for (const tinygltf::Primitive& primitive : primitives) {
				const tinygltf::Accessor& oldAccessor = _model.accessors[primitive.attributes.at(name)];
				mergeAttribute(combinedAttributeAccessor, combinedAttributeBV, combinedAttributeBuffer, oldAccessor);
			}
			combinedAttributeBV.buffer = newBuffers.size();
			newBuffers.push_back(combinedAttributeBuffer);

			combinedAttributeAccessor.bufferView = newBufferViews.size();
			newBufferViews.push_back(combinedAttributeBV);

			combinedPrimitive.attributes[name] = newAccessors.size();
			newAccessors.push_back(combinedAttributeAccessor);
		}

		tinygltf::Accessor combinedIndiceAccessor;
		tinygltf::BufferView combinedIndiceBV;
		tinygltf::Buffer combinedIndiceBuffer;

		unsigned int positionCount = 0;
		for (const tinygltf::Primitive& primitive : primitives) {
			combinedPrimitive.material = primitive.material;
			const tinygltf::Accessor oldAccessor = _model.accessors[primitive.indices];
			const tinygltf::Accessor& positionAccessor = _model.accessors[primitive.attributes.at("POSITION")];
			mergeIndice(combinedIndiceAccessor, combinedIndiceBV, combinedIndiceBuffer, oldAccessor, positionCount);
			positionCount += positionAccessor.count;
		}
		combinedIndiceBV.buffer = newBuffers.size();
		newBuffers.push_back(combinedIndiceBuffer);

		combinedIndiceAccessor.bufferView = newBufferViews.size();
		newBufferViews.push_back(combinedIndiceBV);

		combinedPrimitive.indices = newAccessors.size();
		newAccessors.push_back(combinedIndiceAccessor);

		combinedPrimitive.mode = TINYGLTF_MODE_TRIANGLES;
		tinygltf::Mesh newMesh;
		newMesh.primitives = { combinedPrimitive };
		combinedMeshes.push_back(newMesh);
	}
	return combinedMeshes;
}

void GltfMerger::mergeIndice(tinygltf::Accessor& newIndiceAccessor, tinygltf::BufferView& newIndiceBV, tinygltf::Buffer& newIndiceBuffer, const tinygltf::Accessor oldIndiceAccessor, const unsigned int positionCount)
{
	newIndiceAccessor.type = oldIndiceAccessor.type;
	if (newIndiceAccessor.count == 0)
	{
		newIndiceAccessor = oldIndiceAccessor;
		newIndiceBV = _model.bufferViews[oldIndiceAccessor.bufferView];
		newIndiceBuffer = _model.buffers[newIndiceBV.buffer];
	}
	else
	{
		const size_t count = newIndiceAccessor.count + oldIndiceAccessor.count;
		if (newIndiceAccessor.componentType != oldIndiceAccessor.componentType)
		{
			if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
				&& oldIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
				&& count <= 65535
				)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				mergeIndices<uint8_t, uint16_t, osg::UShortArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
				newIndiceAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
				&& oldIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
				&& count <= 65535
				)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				mergeIndices<uint16_t, uint8_t, osg::UShortArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT
				&& oldIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
				)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				mergeIndices<uint32_t, uint16_t, osg::UIntArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
				&& oldIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT
				)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				mergeIndices<uint16_t, uint32_t, osg::UIntArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
				newIndiceAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT
				&& oldIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
				)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				mergeIndices<uint32_t, uint8_t, osg::UIntArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
				&& oldIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT
				)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				mergeIndices<uint8_t, uint32_t, osg::UIntArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
				newIndiceAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
			}
		}
		else
		{
			if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE && count <= 255)
			{
				osg::ref_ptr<osg::UByteArray> indices = new osg::UByteArray;
				mergeIndices<uint8_t, uint8_t, osg::UByteArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT && count <= 65535)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				mergeIndices<uint16_t, uint16_t, osg::UShortArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			{
				osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
				mergeIndices<uint8_t, uint8_t, osg::UShortArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
				newIndiceAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				mergeIndices<uint16_t, uint16_t, osg::UIntArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
				newIndiceAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
			}
			else if (newIndiceAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
			{
				osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
				mergeIndices<uint32_t, uint32_t, osg::UIntArray>(newIndiceAccessor, newIndiceBuffer, oldIndiceAccessor, indices, positionCount);
				restoreBuffer(newIndiceBuffer, newIndiceBV, indices);
			}
		}
		newIndiceAccessor.count = count;
	}
}

void GltfMerger::mergeAttribute(tinygltf::Accessor& newAttributeAccessor, tinygltf::BufferView& newAttributeBV, tinygltf::Buffer& newAttributeBuffer, const tinygltf::Accessor& oldAttributeAccessor)
{
	newAttributeAccessor.count += oldAttributeAccessor.count;
	newAttributeAccessor.componentType = oldAttributeAccessor.componentType;
	newAttributeAccessor.type = oldAttributeAccessor.type;

	/* minValues、maxValues*/
	if (!oldAttributeAccessor.minValues.empty()) {
		if (newAttributeAccessor.minValues.size() == oldAttributeAccessor.minValues.size()) {
			for (int k = 0; k < newAttributeAccessor.minValues.size(); ++k) {
				newAttributeAccessor.minValues[k] = osg::minimum(newAttributeAccessor.minValues[k], oldAttributeAccessor.minValues[k]);
				newAttributeAccessor.maxValues[k] = osg::maximum(newAttributeAccessor.maxValues[k], oldAttributeAccessor.maxValues[k]);
			}
		}
		else {
			newAttributeAccessor.minValues = oldAttributeAccessor.minValues;
			newAttributeAccessor.maxValues = oldAttributeAccessor.maxValues;
		}
	}

	const tinygltf::BufferView& oldAttributeBV = _model.bufferViews[oldAttributeAccessor.bufferView];
	newAttributeBV.byteStride = oldAttributeBV.byteStride;
	newAttributeBV.target = oldAttributeBV.target;
	newAttributeBV.byteLength += oldAttributeBV.byteLength;

	const tinygltf::Buffer& oldAttributeBuffer = _model.buffers[oldAttributeBV.buffer];
	newAttributeBuffer.data.insert(newAttributeBuffer.data.end(), oldAttributeBuffer.data.begin(), oldAttributeBuffer.data.end());
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
	if (_bMergeMeshes)
		mergeMeshes();
}

void GltfMerger::collectMeshNodes(size_t index, std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual>& matrixPrimitiveMap, osg::Matrixd matrix)
{
	const tinygltf::Node& node = _model.nodes[index];
	matrix.preMult(convertGltfNodeToOsgMatrix(node));

	if (node.mesh == -1) {
		for (size_t childIndex : node.children) {
			collectMeshNodes(childIndex, matrixPrimitiveMap, matrix);
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
			unsigned short* oldIndicesChar = reinterpret_cast<unsigned short*>(buffer.data.data());
			std::vector<unsigned int> indices;

			for (size_t i = 0; i < accessor.count; ++i) {
				unsigned short ushortVal = *oldIndicesChar++;
				unsigned int uintVal = static_cast<unsigned int>(ushortVal + sum);
				indices.push_back(uintVal);
			}

			unsigned char* indicesUChar = reinterpret_cast<unsigned char*>(indices.data());
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
				unsigned int* oldIndicesChar = reinterpret_cast<unsigned int*>(buffer.data.data());
				std::vector<unsigned int> indices;
				for (unsigned int i = 0; i < accessor.count; ++i) {
					unsigned int oldUintVal = *oldIndicesChar++;
					unsigned int uintVal = oldUintVal + sum;
					indices.push_back(uintVal);
				}
				unsigned char* indicesUChar = reinterpret_cast<unsigned char*>(indices.data());
				const unsigned int size = bv.byteLength;
				for (unsigned int i = 0; i < size; ++i) {
					tBuffer.data.push_back(*indicesUChar++);
				}
			}
			else {
				unsigned short* oldIndicesChar = reinterpret_cast<unsigned short*>(buffer.data.data());
				std::vector<unsigned short> indices;
				for (size_t i = 0; i < accessor.count; ++i) {
					unsigned short oldUshortVal = *oldIndicesChar++;
					unsigned short ushortVal = oldUshortVal + sum;
					indices.push_back(ushortVal);
				}
				unsigned char* indicesUChar = reinterpret_cast<unsigned char*>(indices.data());
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
