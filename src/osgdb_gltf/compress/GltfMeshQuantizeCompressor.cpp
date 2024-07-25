#include "osgdb_gltf/compress/GltfMeshQuantizeCompressor.h"
#include <meshoptimizer.h>
void GltfMeshQuantizeCompressor::restoreBuffer(tinygltf::Buffer& buffer, tinygltf::BufferView& bufferView, osg::ref_ptr<osg::Array> newBufferData)
{
	buffer.data.resize(newBufferData->getTotalDataSize());
	const unsigned char* ptr = (unsigned char*)(newBufferData->getDataPointer());
	for (size_t i = 0; i < newBufferData->getTotalDataSize(); ++i)
		buffer.data[i] = *ptr++;
	bufferView.byteLength = buffer.data.size();
}

void GltfMeshQuantizeCompressor::recomputeTextureTransform(tinygltf::ExtensionMap& extensionMap, tinygltf::Accessor& accessor, const double minTx, const double minTy, const double scaleTx, const double scaleTy)
{
	KHR_texture_transform texture_transform_extension;
	auto& findResult = extensionMap.find(texture_transform_extension.name);
	if (findResult != extensionMap.end()) {
		texture_transform_extension.SetValue(findResult->second.Get<tinygltf::Value::Object>());
		std::array<double, 2> offsets = texture_transform_extension.getOffset();
		offsets[0] += minTx;
		offsets[1] += minTy;
		texture_transform_extension.setOffset(offsets);
		std::array<double, 2> scales = texture_transform_extension.getScale();
		scales[0] *= scaleTx / float((1 << _texBit) - 1) * (accessor.normalized ? 65535.f : 1.f);
		scales[1] *= scaleTy / float((1 << _texBit) - 1) * (accessor.normalized ? 65535.f : 1.f);
		texture_transform_extension.setScale(scales);

		findResult->second = texture_transform_extension.GetValue();
	}
}

void GltfMeshQuantizeCompressor::processMaterial(const tinygltf::Primitive primitive, tinygltf::Accessor& accessor, const double minTx, const double minTy, const double scaleTx, const double scaleTy)
{
	if (std::count(_materialIndexes.begin(), _materialIndexes.end(), primitive.material))
		return;
	_materialIndexes.push_back(primitive.material);
	tinygltf::Material& material = _model.materials[primitive.material];
	recomputeTextureTransform(material.pbrMetallicRoughness.baseColorTexture.extensions, accessor, minTx, minTy, scaleTx, scaleTy);
	recomputeTextureTransform(material.normalTexture.extensions, accessor, minTx, minTy, scaleTx, scaleTy);
	recomputeTextureTransform(material.occlusionTexture.extensions, accessor, minTx, minTy, scaleTx, scaleTy);
	recomputeTextureTransform(material.emissiveTexture.extensions, accessor, minTx, minTy, scaleTx, scaleTy);
	recomputeTextureTransform(material.pbrMetallicRoughness.metallicRoughnessTexture.extensions, accessor, minTx, minTy, scaleTx, scaleTy);
	KHR_materials_pbrSpecularGlossiness pbrSGExtension;
	if (material.extensions.find(pbrSGExtension.name) != material.extensions.end()) {
		tinygltf::Value& val = material.extensions[pbrSGExtension.name];
		if (val.IsObject()) {
			pbrSGExtension.SetValue(val.Get<tinygltf::Value::Object>());
			recomputeTextureTransform(pbrSGExtension.specularGlossinessTexture.extensions, accessor, minTx, minTy, scaleTx, scaleTy);
			recomputeTextureTransform(pbrSGExtension.diffuseTexture.extensions, accessor, minTx, minTy, scaleTx, scaleTy);
		}
	}
}

std::tuple<double, double, double, double> GltfMeshQuantizeCompressor::getTexcoordBounds(const tinygltf::Primitive& primitive, const tinygltf::Accessor& accessor)
{
	std::vector<tinygltf::Accessor> texcoordAccessors;
	for (const tinygltf::Mesh& mesh : _model.meshes) {
		for (const tinygltf::Primitive& pri : mesh.primitives) {
			if (pri.material == primitive.material) {
				auto it = pri.attributes.find("TEXCOORD_0");
				if (it != pri.attributes.end()) {
					texcoordAccessors.push_back(_model.accessors[it->second]);
				}
			}
		}
	}

	double minTx = accessor.minValues[0], minTy = accessor.minValues[1];
	double maxTx = accessor.maxValues[0], maxTy = accessor.maxValues[1];
	for (const auto& item : texcoordAccessors) {
		minTx = std::min(item.minValues[0], minTx);
		minTy = std::min(item.minValues[1], minTy);

		maxTx = std::max(item.maxValues[0], maxTx);
		maxTy = std::max(item.maxValues[1], maxTy);
	}

	double scaleTx = maxTx - minTx;
	double scaleTy = maxTy - minTy;

	return std::make_tuple(minTx, minTy, scaleTx, scaleTy);
}

std::tuple<double, double, double, double> GltfMeshQuantizeCompressor::getPositionBounds()
{
	double minVX = FLT_MAX, minVY = FLT_MAX, minVZ = FLT_MAX, scaleV = -FLT_MAX, maxVX = -FLT_MAX, maxVY = -FLT_MAX, maxVZ = -FLT_MAX;
	for (auto& mesh : _model.meshes)
	{
		for (const tinygltf::Primitive& primitive : mesh.primitives)
		{
			for (const auto& pair : primitive.attributes)
			{
				tinygltf::Accessor& accessor = _model.accessors[pair.second];
				if (accessor.type == TINYGLTF_TYPE_VEC3 && pair.first == "POSITION")
				{
					minVX = std::min(double(accessor.minValues[0]), minVX);
					minVY = std::min(double(accessor.minValues[1]), minVY);
					minVZ = std::min(double(accessor.minValues[2]), minVZ);

					maxVX = std::max(double(accessor.maxValues[0]), maxVX);
					maxVY = std::max(double(accessor.maxValues[1]), maxVY);
					maxVZ = std::max(double(accessor.maxValues[2]), maxVZ);
				}
			}

		}
	}
	scaleV = std::max(std::max(maxVX - minVX, maxVY - minVY), maxVZ - minVZ);

	return std::make_tuple(minVX, minVY, minVZ, scaleV);
}

void GltfMeshQuantizeCompressor::encodeQuat(osg::Vec4s v, osg::Vec4 a, int bits)
{
	const float scaler = sqrtf(2.f);

	int qc = 0;
	qc = fabsf(a[1]) > fabsf(a[qc]) ? 1 : qc;
	qc = fabsf(a[2]) > fabsf(a[qc]) ? 2 : qc;
	qc = fabsf(a[3]) > fabsf(a[qc]) ? 3 : qc;

	float sign = a[qc] < 0.f ? -1.f : 1.f;

	v[0] = meshopt_quantizeSnorm(a[(qc + 1) & 3] * scaler * sign, bits);
	v[1] = meshopt_quantizeSnorm(a[(qc + 2) & 3] * scaler * sign, bits);
	v[2] = meshopt_quantizeSnorm(a[(qc + 3) & 3] * scaler * sign, bits);
	v[3] = (meshopt_quantizeSnorm(1.f, bits) & ~3) | qc;
}

void GltfMeshQuantizeCompressor::encodeOct(int& fu, int& fv, float nx, float ny, float nz, int bits)
{
	float nl = fabsf(nx) + fabsf(ny) + fabsf(nz);
	float ns = nl == 0.f ? 0.f : 1.f / nl;

	nx *= ns;
	ny *= ns;

	float u = (nz >= 0.f) ? nx : (1 - fabsf(ny)) * (nx >= 0.f ? 1.f : -1.f);
	float v = (nz >= 0.f) ? ny : (1 - fabsf(nx)) * (ny >= 0.f ? 1.f : -1.f);

	fu = meshopt_quantizeSnorm(u, bits);
	fv = meshopt_quantizeSnorm(v, bits);
}

void GltfMeshQuantizeCompressor::encodeExpShared(osg::Vec4ui v, osg::Vec4 a, int bits)
{
	// get exponents from all components
	int ex, ey, ez;
	frexp(a[0], &ex);
	frexp(a[1], &ey);
	frexp(a[2], &ez);

	// use maximum exponent to encode values; this guarantees that mantissa is [-1, 1]
	// note that we additionally scale the mantissa to make it a K-bit signed integer (K-1 bits for magnitude)
	int exp = std::max(ex, std::max(ey, ez)) - (bits - 1);

	// compute renormalized rounded mantissas for each component
	int mx = int(ldexp(a[0], -exp) + (a[0] >= 0 ? 0.5f : -0.5f));
	int my = int(ldexp(a[1], -exp) + (a[1] >= 0 ? 0.5f : -0.5f));
	int mz = int(ldexp(a[2], -exp) + (a[2] >= 0 ? 0.5f : -0.5f));

	int mmask = (1 << 24) - 1;

	// encode exponent & mantissa into each resulting value
	v[0] = (mx & mmask) | (unsigned(exp) << 24);
	v[1] = (my & mmask) | (unsigned(exp) << 24);
	v[2] = (mz & mmask) | (unsigned(exp) << 24);
}

unsigned int GltfMeshQuantizeCompressor::encodeExpOne(float v, int bits)
{
	// extract exponent
	int e;
	frexp(v, &e);

	// scale the mantissa to make it a K-bit signed integer (K-1 bits for magnitude)
	int exp = e - (bits - 1);

	// compute renormalized rounded mantissa
	int m = int(ldexp(v, -exp) + (v >= 0 ? 0.5f : -0.5f));

	int mmask = (1 << 24) - 1;

	// encode exponent & mantissa
	return (m & mmask) | (unsigned(exp) << 24);
}

osg::ref_ptr<osg::Vec3uiArray> GltfMeshQuantizeCompressor::encodeExpParallel(std::vector<float> vertex, int bits)
{
	const unsigned int size = vertex.size() / 3;
	osg::ref_ptr<osg::Vec3uiArray> result = new osg::Vec3uiArray(size);

	int expx = -128, expy = -128, expz = -128;
	for (size_t i = 0; i < size; ++i)
	{
		int ex, ey, ez;
		frexp(vertex[3 * i + 0], &ex);
		frexp(vertex[3 * i + 1], &ey);
		frexp(vertex[3 * i + 2], &ez);

		expx = std::max(expx, ex);
		expy = std::max(expy, ey);
		expz = std::max(expz, ez);
	}

	expx -= (bits - 1);
	expy -= (bits - 1);
	expz -= (bits - 1);

	for (size_t i = 0; i < size; ++i)
	{

		// compute renormalized rounded mantissas
		int mx = int(ldexp(vertex[3 * i + 0], -expx) + (vertex[3 * i + 0] >= 0 ? 0.5f : -0.5f));
		int my = int(ldexp(vertex[3 * i + 1], -expy) + (vertex[3 * i + 1] >= 0 ? 0.5f : -0.5f));
		int mz = int(ldexp(vertex[3 * i + 2], -expz) + (vertex[3 * i + 2] >= 0 ? 0.5f : -0.5f));

		int mmask = (1 << 24) - 1;

		// encode exponent & mantissa
		osg::Vec3ui v;
		v[0] = (mx & mmask) | (unsigned(expx) << 24);
		v[1] = (my & mmask) | (unsigned(expy) << 24);
		v[2] = (mz & mmask) | (unsigned(expz) << 24);
		result->at(i) = v;
	}

	return result;
}

int GltfMeshQuantizeCompressor::quantizeColor(float v, int bytebits, int bits)
{
	int result = meshopt_quantizeUnorm(v, bytebits);

	const int mask = (1 << (bytebits - bits)) - 1;

	return (result & ~mask) | (mask & -(result >> (bytebits - 1)));
}

void GltfMeshQuantizeCompressor::quantizeMesh(tinygltf::Mesh& mesh, const double minVX, const double minVY, const double minVZ, const double scaleV)
{
	for (const tinygltf::Primitive& primitive : mesh.primitives)
	{
		for (const auto& pair : primitive.attributes) {
			tinygltf::Accessor& accessor = _model.accessors[pair.second];
			if (accessor.type == TINYGLTF_TYPE_VEC2 || accessor.type == TINYGLTF_TYPE_VEC3 || accessor.type == TINYGLTF_TYPE_VEC4)
			{
				tinygltf::BufferView& bufferView = _model.bufferViews[accessor.bufferView];
				tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
				std::vector<float> bufferData = getBufferData<float>(accessor);

				if (accessor.type == TINYGLTF_TYPE_VEC3)
				{
					if (pair.first == "POSITION") {

						if (_posFloat) {
							osg::ref_ptr<osg::Vec3Array> newBufferData = new osg::Vec3Array(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								const float x = meshopt_quantizeFloat(bufferData[i * 3 + 0], _positionBit + 1);
								const float y = meshopt_quantizeFloat(bufferData[i * 3 + 1], _positionBit + 1);
								const float z = meshopt_quantizeFloat(bufferData[i * 3 + 2], _positionBit + 1);
								newBufferData->at(i) = osg::Vec3(x, y, z);
							}
							for (int k = 0; k < 3; ++k)
							{
								accessor.minValues[k] = meshopt_quantizeFloat(accessor.minValues[k], _positionBit);
								accessor.maxValues[k] = meshopt_quantizeFloat(accessor.maxValues[k], _positionBit);
							}


							accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
							bufferView.byteStride = 4 * 3;
							restoreBuffer(buffer, bufferView, newBufferData);
						}
						else
						{
							//顶点属性必须是4字节对齐的，所以这里是vec4us
							osg::ref_ptr<osg::Vec4usArray> newBufferData = new osg::Vec4usArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								const unsigned short x = meshopt_quantizeUnorm((bufferData[i * 3 + 0] - minVX) / scaleV, _positionBit);
								const unsigned short y = meshopt_quantizeUnorm((bufferData[i * 3 + 1] - minVY) / scaleV, _positionBit);
								const unsigned short z = meshopt_quantizeUnorm((bufferData[i * 3 + 2] - minVZ) / scaleV, _positionBit);
								newBufferData->at(i) = osg::Vec4us(x, y, z, 0);
							}
							accessor.minValues[0] = meshopt_quantizeUnorm((accessor.minValues[0] - minVX) / scaleV, _positionBit);
							accessor.minValues[1] = meshopt_quantizeUnorm((accessor.minValues[1] - minVY) / scaleV, _positionBit);
							accessor.minValues[2] = meshopt_quantizeUnorm((accessor.minValues[2] - minVZ) / scaleV, _positionBit);
							accessor.maxValues[0] = meshopt_quantizeUnorm((accessor.maxValues[0] - minVX) / scaleV, _positionBit);
							accessor.maxValues[1] = meshopt_quantizeUnorm((accessor.maxValues[1] - minVY) / scaleV, _positionBit);
							accessor.maxValues[2] = meshopt_quantizeUnorm((accessor.maxValues[2] - minVZ) / scaleV, _positionBit);

							accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
							bufferView.byteStride = 2 * 4;
							restoreBuffer(buffer, bufferView, newBufferData);
						}
					}
					else if (pair.first == "NORMAL")
					{

						if (_normalBit < 9) {
							const bool oct = _compressmore && bufferView.target == TINYGLTF_TARGET_ARRAY_BUFFER;
							osg::ref_ptr<osg::Vec4bArray> newBufferData = new osg::Vec4bArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								if (oct) {
									int fu, fv;
									encodeOct(fu, fv, bufferData[i * 3 + 0], bufferData[i * 3 + 1], bufferData[i * 3 + 2], _normalBit);
									const signed char x = fu;
									const signed char y = fv;
									const signed char z = meshopt_quantizeSnorm(1.f, _normalBit);
									newBufferData->at(i) = osg::Vec4b(x, y, z, 0);
								}
								else {
									const signed char x = meshopt_quantizeSnorm(bufferData[i * 3 + 0], _normalBit);
									const signed char y = meshopt_quantizeSnorm(bufferData[i * 3 + 1], _normalBit);
									const signed char z = meshopt_quantizeSnorm(bufferData[i * 3 + 2], _normalBit);
									newBufferData->at(i) = osg::Vec4b(x, y, z, 0);
								}
							}
							accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

							restoreBuffer(buffer, bufferView, newBufferData);
							bufferView.byteStride = 1 * 4;
						}
						else
						{
							osg::ref_ptr<osg::Vec4sArray> newBufferData = new osg::Vec4sArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								const short x = meshopt_quantizeSnorm(bufferData[i * 3 + 0], _normalBit);
								const short y = meshopt_quantizeSnorm(bufferData[i * 3 + 1], _normalBit);
								const short z = meshopt_quantizeSnorm(bufferData[i * 3 + 2], _normalBit);
								newBufferData->at(i) = osg::Vec4s(x, y, z, 0);

							}
							accessor.componentType = TINYGLTF_COMPONENT_TYPE_SHORT;
							restoreBuffer(buffer, bufferView, newBufferData);
							bufferView.byteStride = 2 * 4;
						}
					}

				}
				else if (accessor.type == TINYGLTF_TYPE_VEC2)
				{
					if (pair.first == "TEXCOORD_0")
					{
						std::tuple<double, double, double, double> result = getTexcoordBounds(primitive, accessor);
						const double minTx = std::get<0>(result);
						const double minTy = std::get<1>(result);
						const double scaleTx = std::get<2>(result);
						const double scaleTy = std::get<3>(result);

						osg::ref_ptr<osg::Vec2usArray> newBufferData = new osg::Vec2usArray(accessor.count);
						for (size_t i = 0; i < accessor.count; ++i)
						{
							const unsigned short x = meshopt_quantizeUnorm((bufferData[i * 2 + 0] - minTx) / scaleTx, _texBit);
							const unsigned short y = meshopt_quantizeUnorm((bufferData[i * 2 + 1] - minTy) / scaleTy, _texBit);
							newBufferData->at(i) = osg::Vec2us(x, y);
						}
						accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
						accessor.normalized = true;
						restoreBuffer(buffer, bufferView, newBufferData);
						bufferView.byteStride = 2 * 2;

						processMaterial(primitive, accessor, minTx, minTy, scaleTx, scaleTy);
					}
				}
				else {
					if (pair.first == "TANGENT")
					{
						const int bits = _normalBit > 8 ? 8 : _normalBit;
						const bool oct = _compressmore && bufferView.target == TINYGLTF_TARGET_ARRAY_BUFFER;
						osg::ref_ptr<osg::Vec4bArray> newBufferData = new osg::Vec4bArray(accessor.count);
						for (size_t i = 0; i < accessor.count; ++i)
						{
							if (oct) {
								int fu, fv;
								encodeOct(fu, fv, bufferData[i * 4 + 0], bufferData[i * 4 + 1], bufferData[i * 4 + 2], bits);
								const signed char x = fu;
								const signed char y = fv;
								const signed char z = meshopt_quantizeSnorm(1.f, bits);
								const signed char w = meshopt_quantizeSnorm(bufferData[i * 4 + 3], bits);
								newBufferData->at(i) = osg::Vec4b(x, y, z, w);
							}
							else {
								const signed char x = meshopt_quantizeSnorm(bufferData[i * 4 + 0], bits);
								const signed char y = meshopt_quantizeSnorm(bufferData[i * 4 + 1], bits);
								const signed char z = meshopt_quantizeSnorm(bufferData[i * 4 + 2], bits);
								const signed char w = meshopt_quantizeSnorm(bufferData[i * 4 + 3], bits);
								newBufferData->at(i) = osg::Vec4b(x, y, z, w);
							}
						}
						accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

						restoreBuffer(buffer, bufferView, newBufferData);
						bufferView.byteStride = 1 * 4;
					}
					else if (pair.first == "COLOR_0")
					{
						if (_colorBit < 9)
						{
							osg::ref_ptr<osg::Vec4bArray> newBufferData = new osg::Vec4bArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								const signed char x = quantizeColor(bufferData[i * 4 + 0], 8, _colorBit);
								const signed char y = quantizeColor(bufferData[i * 4 + 1], 8, _colorBit);
								const signed char z = quantizeColor(bufferData[i * 4 + 2], 8, _colorBit);
								const signed char w = quantizeColor(bufferData[i * 4 + 3], 8, _colorBit);

								newBufferData->at(i) = osg::Vec4b(x, y, z, w);
							}
							accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

							restoreBuffer(buffer, bufferView, newBufferData);
							bufferView.byteStride = 1 * 4;
						}
						else
						{
							osg::ref_ptr<osg::Vec4sArray> newBufferData = new osg::Vec4sArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								const short x = quantizeColor(bufferData[i * 4 + 0], 16, _colorBit);
								const short y = quantizeColor(bufferData[i * 4 + 1], 16, _colorBit);
								const short z = quantizeColor(bufferData[i * 4 + 2], 16, _colorBit);
								const short w = quantizeColor(bufferData[i * 4 + 3], 16, _colorBit);

								newBufferData->at(i) = osg::Vec4s(x, y, z, w);

							}
							accessor.componentType = TINYGLTF_COMPONENT_TYPE_SHORT;
							restoreBuffer(buffer, bufferView, newBufferData);
							bufferView.byteStride = 2 * 4;
						}
					}
				}
			}
		}
	}
}
