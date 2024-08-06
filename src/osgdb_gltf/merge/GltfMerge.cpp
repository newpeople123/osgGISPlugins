#include "osgdb_gltf/merge/GltfMerge.h"
#include <osg/Math>
#include <osg/Matrixd>
#include <unordered_map>
#include <osg/MatrixTransform>
const double TOLERANCE = 0.01;

osg::Matrixd convertGltfNode2OsgMatrix(const tinygltf::Node& node)
{
	osg::Matrixd osgMatrix;

	// 检查是否有矩阵
	if (!node.matrix.empty()) {
		// tinygltf::Node 的矩阵是以列优先顺序存储的
		osgMatrix.set(node.matrix.data());
	}
	else {
		osg::Vec3d translation(0.0, 0.0, 0.0);
		osg::Quat rotation(0.0, 0.0, 0.0, 1.0); // 默认为单位四元数
		osg::Vec3d scale(1.0, 1.0, 1.0);

		// 检查是否有平移
		if (!node.translation.empty()) {
			translation.set(node.translation[0], node.translation[1], node.translation[2]);
		}

		// 检查是否有旋转
		if (!node.rotation.empty()) {
			rotation.set(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
		}

		// 检查是否有缩放
		if (!node.scale.empty()) {
			scale.set(node.scale[0], node.scale[1], node.scale[2]);
		}

		osgMatrix.makeIdentity();
		osgMatrix.makeTranslate(translation);

		osg::Matrixd rotationMatrix;
		rotationMatrix.makeRotate(rotation);
		osgMatrix.preMult(rotationMatrix);

		osg::Matrixd scaleMatrix;
		scaleMatrix.makeScale(scale);
		osgMatrix.preMult(scaleMatrix);
	}

	return osgMatrix;
}

double determinant(const osg::Matrixd& matrix) {
	double det =
		matrix(0, 3) * matrix(1, 2) * matrix(2, 1) * matrix(3, 0) - matrix(0, 2) * matrix(1, 3) * matrix(2, 1) * matrix(3, 0)
		- matrix(0, 3) * matrix(1, 1) * matrix(2, 2) * matrix(3, 0) + matrix(0, 1) * matrix(1, 3) * matrix(2, 2) * matrix(3, 0)
		+ matrix(0, 2) * matrix(1, 1) * matrix(2, 3) * matrix(3, 0) - matrix(0, 1) * matrix(1, 2) * matrix(2, 3) * matrix(3, 0)
		- matrix(0, 3) * matrix(1, 2) * matrix(2, 0) * matrix(3, 1) + matrix(0, 2) * matrix(1, 3) * matrix(2, 0) * matrix(3, 1)
		+ matrix(0, 3) * matrix(1, 0) * matrix(2, 2) * matrix(3, 1) - matrix(0, 0) * matrix(1, 3) * matrix(2, 2) * matrix(3, 1)
		- matrix(0, 2) * matrix(1, 0) * matrix(2, 3) * matrix(3, 1) + matrix(0, 0) * matrix(1, 2) * matrix(2, 3) * matrix(3, 1)
		+ matrix(0, 3) * matrix(1, 1) * matrix(2, 0) * matrix(3, 2) - matrix(0, 1) * matrix(1, 3) * matrix(2, 0) * matrix(3, 2)
		- matrix(0, 3) * matrix(1, 0) * matrix(2, 1) * matrix(3, 2) + matrix(0, 0) * matrix(1, 3) * matrix(2, 1) * matrix(3, 2)
		+ matrix(0, 1) * matrix(1, 0) * matrix(2, 3) * matrix(3, 2) - matrix(0, 0) * matrix(1, 1) * matrix(2, 3) * matrix(3, 2)
		- matrix(0, 2) * matrix(1, 1) * matrix(2, 0) * matrix(3, 3) + matrix(0, 1) * matrix(1, 2) * matrix(2, 0) * matrix(3, 3)
		+ matrix(0, 2) * matrix(1, 0) * matrix(2, 1) * matrix(3, 3) - matrix(0, 0) * matrix(1, 2) * matrix(2, 1) * matrix(3, 3)
		- matrix(0, 1) * matrix(1, 0) * matrix(2, 2) * matrix(3, 3) + matrix(0, 0) * matrix(1, 1) * matrix(2, 2) * matrix(3, 3);
	return det;
}

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

// 计算哈希值的函数对象
struct MatrixHash {
	std::size_t operator()(const osg::Matrixd& matrix) const {
		std::size_t seed = 0;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				seed ^= std::hash<double>()(matrix(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			}
		}
		return seed;
	}
};

// 比较相等性的函数对象
struct MatrixEqual {
	bool operator()(const osg::Matrixd& lhs, const osg::Matrixd& rhs) const {
		if (osg::absolute(determinant(lhs) - determinant(rhs)) > TOLERANCE)
			return false;
		const osg::Vec3d lhsTrans = lhs.getTrans();
		const osg::Vec3d rhsTrans = rhs.getTrans();
		if (lhsTrans.x() != rhsTrans.x() || lhsTrans.y() != rhsTrans.y() || lhsTrans.z() != rhsTrans.z())
			return false;
		//if (lhsTrans != rhsTrans && (lhsTrans - rhsTrans).length() > TOLERANCE)
		//	return false;

		const osg::Quat lhsQuat = lhs.getRotate();
		const osg::Quat rhsQuat = rhs.getRotate();
		if (lhsQuat.x() != rhsQuat.x() || lhsQuat.y() != rhsQuat.y() || lhsQuat.z() != rhsQuat.z() || lhsQuat.w() != rhsQuat.w())
			return false;
		//if (lhsQuat != lhsQuat)
		//	return false;

		const osg::Vec3d lhsScale = lhs.getScale();
		const osg::Vec3d rhsScale = rhs.getScale();
		if (lhsScale.x() != rhsScale.x() || lhsScale.y() != rhsScale.y() || lhsScale.z() != rhsScale.z())
			return false;
		//if (lhsScale != rhsScale && (lhsScale - rhsScale).length() > TOLERANCE)
		//	return false;
		return true;
	}
};

void test2(const tinygltf::Model& model, const size_t index,osg::Matrixd& matrix)
{
	for (size_t i = 0; i < model.nodes.size(); ++i)
	{
		const tinygltf::Node& node = model.nodes[i];
		for (size_t j = 0; j < node.children.size(); ++j)
		{
			if (node.children[j] == index)
			{
				const osg::Matrixd mat = convertGltfNode2OsgMatrix(node);
				matrix.postMult(mat);
				test2(model, i, matrix);
				break;
			}
		}
	}
}
void test1(const tinygltf::Model& model, const size_t index, std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual>& matrixPrimitiveMap)
{
	osg::Matrixd matrix;
	for (size_t i=0;i<model.nodes.size();++i)
	{
		const tinygltf::Node& node = model.nodes[i];
		if (node.mesh == index)
		{
			test2(model, i, matrix);
			const tinygltf::Mesh& mesh = model.meshes[node.mesh];
			matrixPrimitiveMap[matrix].insert(matrixPrimitiveMap[matrix].end(), mesh.primitives.begin(), mesh.primitives.end());
			break;
		}
	}
}

void findMeshNode(tinygltf::Model& model, const size_t index, std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual>& matrixPrimitiveMap,osg::Matrixd& matrix)
{
	const tinygltf::Node& node = model.nodes[index];
	matrix.preMult(convertGltfNode2OsgMatrix(node));
	if (node.mesh == -1)
	{
		for (size_t i = 0; i < node.children.size(); ++i)
		{
			findMeshNode(model, node.children[i], matrixPrimitiveMap, osg::Matrixd(matrix));
		}
	}
	else
	{
		const tinygltf::Mesh& mesh = model.meshes[node.mesh];
		matrixPrimitiveMap[matrix].insert(matrixPrimitiveMap[matrix].end(), mesh.primitives.begin(), mesh.primitives.end());
	}
}

void mergeMeshes(tinygltf::Model& model)
{
	std::vector<tinygltf::Accessor> accessors;
	std::vector<tinygltf::BufferView> bufferViews;
	std::vector<tinygltf::Buffer> buffers;
	for (auto& image : model.images) {
		tinygltf::BufferView bufferView = model.bufferViews[image.bufferView];
		tinygltf::Buffer buffer = model.buffers[bufferView.buffer];

		bufferView.buffer = buffers.size();
		image.bufferView = bufferViews.size();
		bufferViews.push_back(bufferView);
		buffers.push_back(buffer);
	}

	std::vector<tinygltf::Mesh> meshes;

	std::unordered_map<osg::Matrixd, std::vector<tinygltf::Primitive>, MatrixHash, MatrixEqual> matrixPrimitiveMap;
	findMeshNode(model, model.scenes[0].nodes[0], matrixPrimitiveMap, osg::Matrixd::identity());
	//for (size_t i = 0; i < model.meshes.size(); ++i)
	//{
	//	test1(model, i, matrixPrimitiveMap);
	//}

	for (const auto& matrixPrimitiveItem : matrixPrimitiveMap) {
		const osg::Vec3d trans = matrixPrimitiveItem.first.getTrans();
		OSG_NOTICE << trans.x() << "," << trans.y() << "," << trans.z() << std::endl;

		std::vector<tinygltf::Primitive> primitives;

		std::map<int, std::vector<tinygltf::Primitive>> materialPrimitiveMap;
		for (auto& primitive : matrixPrimitiveItem.second) {
			materialPrimitiveMap[primitive.material].push_back(primitive);
		}

		for (const auto& materialPrimitiveItem : materialPrimitiveMap) {
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

			for (const auto& prim : materialPrimitiveItem.second) {
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
			for (const auto& prim : materialPrimitiveItem.second) {

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

		tinygltf::Mesh totalMesh;
		totalMesh.primitives = primitives;
		meshes.push_back(totalMesh);
	}

	model.buffers.clear();
	model.bufferViews.clear();
	model.accessors.clear();
	model.buffers = buffers;
	model.bufferViews = bufferViews;
	model.accessors = accessors;

	model.nodes.clear();
	model.meshes.clear();
	model.scenes[0].nodes.clear();
	size_t i = 0;
	for (auto it = matrixPrimitiveMap.begin(); it != matrixPrimitiveMap.end(); ++it, ++i)
	{
		model.scenes[0].nodes.push_back(model.nodes.size());
		model.nodes.emplace_back();
		tinygltf::Node& node = model.nodes.back();
		node.mesh = model.meshes.size();
		model.meshes.push_back(meshes[i]);

		const osg::Matrixd matrix = it->first;
		if (osg::absolute(determinant(matrix)-1) <= TOLERANCE)
			continue;

		const double* ptr = matrix.ptr();
		constexpr int size = 16;
		for (unsigned i = 0; i < size; ++i)
		{
			model.nodes.back().matrix.push_back(*ptr++);
		}
	}

}

