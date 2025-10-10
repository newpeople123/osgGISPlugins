#include "osgdb_gltf/GltfMerger.h"
#include <osg/Math>
#include <osg/MatrixTransform>
#include <algorithm>
#include "osgdb_gltf/compress/GltfMeshQuantizeCompressor.h"

using namespace osgGISPlugins;

// --- helpers implementation ---
int GltfMerger::calcAlignPadding(int offset, int align)
{
	const int rem = offset % align;
	return (align - rem) % align;
}

void GltfMerger::appendPadding(tinygltf::Buffer& buffer, int& offset, int align)
{
	const int needAdd = calcAlignPadding(offset, align);
	for (int i = 0; i < needAdd; ++i) {
		buffer.data.push_back(0);
		++offset;
	}
}

bool GltfMerger::tryGetAccessor(int index, tinygltf::Accessor& out) const
{
	if (index < 0 || static_cast<size_t>(index) >= _model.accessors.size()) return false;
	out = _model.accessors[index];
	return true;
}

bool GltfMerger::tryGetBufferView(int index, tinygltf::BufferView& out) const
{
	if (index < 0 || static_cast<size_t>(index) >= _model.bufferViews.size()) return false;
	out = _model.bufferViews[index];
	return true;
}

bool GltfMerger::tryGetBuffer(int index, tinygltf::Buffer& out) const
{
	if (index < 0 || static_cast<size_t>(index) >= _model.buffers.size()) return false;
	out = _model.buffers[index];
	return true;
}

void GltfMerger::updateMinMax(tinygltf::Accessor& target, const tinygltf::Accessor& src)
{
	if (src.minValues.empty()) return;
	if (target.minValues.size() == src.minValues.size()) {
		for (int k = 0; k < static_cast<int>(target.minValues.size()); ++k) {
			target.minValues[k] = osg::minimum(target.minValues[k], src.minValues[k]);
			target.maxValues[k] = osg::maximum(target.maxValues[k], src.maxValues[k]);
		}
	}
	else {
		target.minValues = src.minValues;
		target.maxValues = src.maxValues;
	}
}

bool GltfMerger::checkAttributesCompatible(const tinygltf::Primitive& ref, const tinygltf::Primitive& cur, const std::vector<std::string>& names, const tinygltf::Model& model)
{
	for (const std::string& n : names) {
		const int refIdx = ref.attributes.at(n);
		const int curIdx = cur.attributes.at(n);
		if (refIdx < 0 || curIdx < 0) return false;
		if (static_cast<size_t>(refIdx) >= model.accessors.size() || static_cast<size_t>(curIdx) >= model.accessors.size()) return false;
		const tinygltf::Accessor& refAcc = model.accessors[refIdx];
		const tinygltf::Accessor& curAcc = model.accessors[curIdx];
		if (refAcc.bufferView < 0 || curAcc.bufferView < 0) return false;
		if (static_cast<size_t>(refAcc.bufferView) >= model.bufferViews.size() || static_cast<size_t>(curAcc.bufferView) >= model.bufferViews.size()) return false;
		const tinygltf::BufferView& refBv = model.bufferViews[refAcc.bufferView];
		const tinygltf::BufferView& curBv = model.bufferViews[curAcc.bufferView];
		if (refAcc.componentType != curAcc.componentType) return false;
		if (refAcc.type != curAcc.type) return false;
		if (refBv.byteStride != curBv.byteStride) return false;
	}
	return true;
}
void GltfMerger::mergeMeshes()
{
	if (!_model.scenes.size()) return;
	// 矩阵对应的primitive
	std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, Utils::MatrixHash, Utils::MatrixEqual> matrixPrimitiveMap;
	assert(_model.scenes.size() > 0);
	assert(_model.scenes[0].nodes.size() > 0);
	collectMeshNodes(_model.scenes[0].nodes[0], matrixPrimitiveMap);
	//if (matrixPrimitiveMap.size() <= 1)
	//	return;

	// 新的accessor、bufferView和buffer
	std::vector<tinygltf::Accessor> newAccessors;
	std::vector<tinygltf::BufferView> newBufferViews;
	std::vector<tinygltf::Buffer> newBuffers;

	//复制图像相关的bufferView和buffer
	for (tinygltf::Image& image : _model.images) {
		if (image.bufferView < 0 || static_cast<size_t>(image.bufferView) >= _model.bufferViews.size())
			continue;
		tinygltf::BufferView& bufferView = _model.bufferViews[image.bufferView];
		if (bufferView.buffer < 0 || static_cast<size_t>(bufferView.buffer) >= _model.buffers.size())
			continue;
		tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];

		bufferView.buffer = static_cast<int>(newBuffers.size());
		image.bufferView = static_cast<int>(newBufferViews.size());
		newBufferViews.push_back(bufferView);
		newBuffers.push_back(buffer);
	}
	//构建一个新的矩阵到mesh的映射
	std::unordered_map<osg::Matrixd, std::vector<tinygltf::Mesh>, Utils::MatrixHash, Utils::MatrixEqual> matrixMeshMap;

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
			if (!matrix.isIdentity()) {
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
				if (!matrix.isIdentity()) {
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
			const int bufIdx = bufferIndex.GetNumberAsInt();
			if (bufIdx < 0 || static_cast<size_t>(bufIdx) >= _model.buffers.size()) continue;
			tinygltf::Buffer& buffer = _model.buffers[bufIdx];
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
			appendPadding(totalBuffer, byteOffset, 4);

			bufferView.buffer = 1;
			bufferView.byteOffset = fallbackByteOffset;
			fallbackByteOffset += bufferView.byteLength;
			//4-byte aligned 
			appendPadding(fallbackBuffer, fallbackByteOffset, 4);
		}
		else {
			if (bufferView.buffer < 0 || static_cast<size_t>(bufferView.buffer) >= _model.buffers.size()) continue;
			tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
			totalBuffer.data.insert(totalBuffer.data.end(), buffer.data.begin(), buffer.data.end());
			bufferView.buffer = 0;
			bufferView.byteOffset = byteOffset;
			byteOffset += bufferView.byteLength;

			//4-byte aligned 
			appendPadding(totalBuffer, byteOffset, 4);
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
	std::vector<tinygltf::Material> newMaterials;
	std::map<int, int> materialRemap;

	for (size_t i = 0; i < _model.materials.size(); ++i)
	{
		const auto& mat = _model.materials[i];

		// 查找 newMaterials 中是否已有相同材质
		auto it = std::find_if(newMaterials.begin(), newMaterials.end(),
			[&mat](const tinygltf::Material& m) { return m == mat; });

		if (it != newMaterials.end())
		{
			// 已存在，记录映射
			int newIndex = std::distance(newMaterials.begin(), it);
			materialRemap[i] = newIndex;
		}
		else
		{
			// 不存在，加入新材质
			int newIndex = (int)newMaterials.size();
			newMaterials.push_back(mat);
			materialRemap[i] = newIndex;
		}
	}

	// 替换 _model.materials
	_model.materials = newMaterials;

	// 更新每个 primitive 的材质索引
	for (auto& mesh : _model.meshes)
	{
		for (auto& primitive : mesh.primitives)
		{
			if (primitive.material != -1)
			{
				auto it = materialRemap.find(primitive.material);
				if (it != materialRemap.end())
					primitive.material = it->second;
			}
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
		if (primitive.indices != -1)
		{
			if (primitive.indices >= 0 && static_cast<size_t>(primitive.indices) < _model.accessors.size()) {
				tinygltf::Accessor oldIndicesAccessor = _model.accessors[primitive.indices];
				if (oldIndicesAccessor.bufferView >= 0 && static_cast<size_t>(oldIndicesAccessor.bufferView) < _model.bufferViews.size()) {
					tinygltf::BufferView oldIndicesBufferView = _model.bufferViews[oldIndicesAccessor.bufferView];
					if (oldIndicesBufferView.buffer >= 0 && static_cast<size_t>(oldIndicesBufferView.buffer) < _model.buffers.size()) {
						tinygltf::Buffer oldIndicesBuffer = _model.buffers[oldIndicesBufferView.buffer];

						oldIndicesBufferView.buffer = static_cast<int>(newBuffers.size());
						newBuffers.push_back(oldIndicesBuffer);

						oldIndicesAccessor.bufferView = static_cast<int>(newBufferViews.size());
						newBufferViews.push_back(oldIndicesBufferView);

						primitive.indices = static_cast<int>(newAccessors.size());
						newAccessors.push_back(oldIndicesAccessor);
					}
				}
			}
		}
		for (const auto& item : primitive.attributes)
		{
			const std::string& name = item.first;
			const int accIndex = primitive.attributes.at(name);
			if (accIndex < 0 || static_cast<size_t>(accIndex) >= _model.accessors.size()) continue;
			tinygltf::Accessor oldAccessor = _model.accessors[accIndex];
			if (oldAccessor.bufferView < 0 || static_cast<size_t>(oldAccessor.bufferView) >= _model.bufferViews.size()) continue;
			tinygltf::BufferView oldBufferView = _model.bufferViews[oldAccessor.bufferView];
			if (oldBufferView.buffer < 0 || static_cast<size_t>(oldBufferView.buffer) >= _model.buffers.size()) continue;
			tinygltf::Buffer oldBuffer = _model.buffers[oldBufferView.buffer];

			oldBufferView.buffer = static_cast<int>(newBuffers.size());
			newBuffers.push_back(oldBuffer);

			oldAccessor.bufferView = static_cast<int>(newBufferViews.size());
			newBufferViews.push_back(oldBufferView);

			primitive.attributes.at(name) = static_cast<int>(newAccessors.size());
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

		// 校验分组内属性兼容性与 POSITION 存在性
		bool canMerge = true;
		for (const tinygltf::Primitive& primitive : primitives) {
			// 必须包含 POSITION
			if (primitive.attributes.find("POSITION") == primitive.attributes.end()) { canMerge = false; break; }
			for (const std::string& name : attributeNames) {
				const int accIdx = primitive.attributes.at(name);
				if (accIdx < 0 || static_cast<size_t>(accIdx) >= _model.accessors.size()) { canMerge = false; break; }
				const tinygltf::Accessor& acc = _model.accessors[accIdx];
				if (acc.bufferView < 0 || static_cast<size_t>(acc.bufferView) >= _model.bufferViews.size()) { canMerge = false; break; }
				const tinygltf::BufferView& bv = _model.bufferViews[acc.bufferView];
				// 简单一致性：componentType 和 type、byteStride 相同
				if (&primitive != &primitives.front()) {
					const int refIdx = primitives.front().attributes.at(name);
					const tinygltf::Accessor& refAcc = _model.accessors[refIdx];
					const tinygltf::BufferView& refBv = _model.bufferViews[refAcc.bufferView];
					if (acc.componentType != refAcc.componentType || acc.type != refAcc.type || bv.byteStride != refBv.byteStride) { canMerge = false; break; }
				}
			}
			if (!canMerge) break;
		}
		if (!canMerge) {
			// 回退：不合并，逐个输出
			for (const tinygltf::Primitive& p : primitives) {
				tinygltf::Mesh m; m.primitives = { p }; combinedMeshes.push_back(m);
			}
			continue;
		}

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

		size_t positionCount = 0;
		for (const tinygltf::Primitive& primitive : primitives) {
			combinedPrimitive.material = primitive.material;
			const tinygltf::Accessor oldAccessor = _model.accessors[primitive.indices];
			const tinygltf::Accessor& positionAccessor = _model.accessors[primitive.attributes.at("POSITION")];
			mergeIndice(combinedIndiceAccessor, combinedIndiceBV, combinedIndiceBuffer, oldAccessor, static_cast<unsigned int>(positionCount));
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

void GltfMerger::collectMeshNodes(size_t index, std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, Utils::MatrixHash, Utils::MatrixEqual>& matrixPrimitiveMap, osg::Matrixd matrix)
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
