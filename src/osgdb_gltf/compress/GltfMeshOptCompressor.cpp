#include "osgdb_gltf/compress/GltfMeshOptCompressor.h"
#include <meshoptimizer.h>
#include <iostream>
void GltfMeshOptCompressor::encodeQuat(osg::Vec4s v, osg::Vec4 a, int bits)
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

void GltfMeshOptCompressor::encodeOct(int& fu, int& fv, float nx, float ny, float nz, int bits)
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

void GltfMeshOptCompressor::encodeExpShared(osg::Vec4ui v, osg::Vec4 a, int bits)
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

unsigned int GltfMeshOptCompressor::encodeExpOne(float v, int bits)
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

osg::ref_ptr<osg::Vec3uiArray> GltfMeshOptCompressor::encodeExpParallel(std::vector<float> vertex, int bits)
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

int GltfMeshOptCompressor::quantizeColor(float v, int bytebits, int bits)
{
	int result = meshopt_quantizeUnorm(v, bytebits);

	const int mask = (1 << (bytebits - bits)) - 1;

	return (result & ~mask) | (mask & -(result >> (bytebits - 1)));
}

void GltfMeshOptCompressor::quantizeMesh(tinygltf::Mesh& mesh, const double minVX, const double minVY, const double minVZ, const double scaleV)
{
	const int positionBit = 14;
	const int normalBit = 8;
	const int texBit = 12;
	const int colorBit = 12;
	const bool compressmore = true;
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
						//顶点属性必须是4字节对齐的，所以这里是vec4us
						osg::ref_ptr<osg::Vec4usArray> newBufferData = new osg::Vec4usArray(accessor.count);
						for (size_t i = 0; i < accessor.count; ++i)
						{
							const unsigned short x = meshopt_quantizeUnorm((bufferData[i * 3 + 0] - minVX) / scaleV, positionBit);
							const unsigned short y = meshopt_quantizeUnorm((bufferData[i * 3 + 1] - minVY) / scaleV, positionBit);
							const unsigned short z = meshopt_quantizeUnorm((bufferData[i * 3 + 2] - minVZ) / scaleV, positionBit);
							newBufferData->at(i) = osg::Vec4us(x, y, z, 0);
						}
						accessor.minValues[0] = meshopt_quantizeUnorm((accessor.minValues[0] - minVX) / scaleV, positionBit);
						accessor.minValues[1] = meshopt_quantizeUnorm((accessor.minValues[1] - minVY) / scaleV, positionBit);
						accessor.minValues[2] = meshopt_quantizeUnorm((accessor.minValues[2] - minVZ) / scaleV, positionBit);
						accessor.maxValues[0] = meshopt_quantizeUnorm((accessor.maxValues[0] - minVX) / scaleV, positionBit);
						accessor.maxValues[1] = meshopt_quantizeUnorm((accessor.maxValues[1] - minVY) / scaleV, positionBit);
						accessor.maxValues[2] = meshopt_quantizeUnorm((accessor.maxValues[2] - minVZ) / scaleV, positionBit);
						accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;

						buffer.data.resize(newBufferData->getTotalDataSize());

						//TODO: account for endianess
						const unsigned char* ptr = (unsigned char*)(newBufferData->getDataPointer());
						for (size_t i = 0; i < newBufferData->getTotalDataSize(); ++i)
							buffer.data[i] = *ptr++;
						bufferView.byteLength = buffer.data.size();
						bufferView.byteStride = 2 * 4;

					}
					else if (pair.first == "NORMAL")
					{

						if (normalBit < 9) {
							const bool oct = compressmore && bufferView.target == TINYGLTF_TARGET_ARRAY_BUFFER;
							osg::ref_ptr<osg::Vec4bArray> newBufferData = new osg::Vec4bArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								if (oct) {
									int fu, fv;
									encodeOct(fu, fv, bufferData[i * 3 + 0], bufferData[i * 3 + 1], bufferData[i * 3 + 2], normalBit);
									const signed char x = fu;
									const signed char y = fv;
									const signed char z = meshopt_quantizeSnorm(1.f, normalBit);
									newBufferData->at(i) = osg::Vec4b(x, y, z, 0);
								}
								else {
									const signed char x = meshopt_quantizeSnorm(bufferData[i * 3 + 0], normalBit);
									const signed char y = meshopt_quantizeSnorm(bufferData[i * 3 + 1], normalBit);
									const signed char z = meshopt_quantizeSnorm(bufferData[i * 3 + 2], normalBit);
									newBufferData->at(i) = osg::Vec4b(x, y, z, 0);
								}
							}
							accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

							buffer.data.resize(newBufferData->getTotalDataSize());

							//TODO: account for endianess
							const unsigned char* ptr = (unsigned char*)(newBufferData->getDataPointer());
							for (size_t i = 0; i < newBufferData->getTotalDataSize(); ++i)
								buffer.data[i] = *ptr++;
							bufferView.byteLength = buffer.data.size();
							bufferView.byteStride = 1 * 4;
						}
						else
						{
							osg::ref_ptr<osg::Vec4sArray> newBufferData = new osg::Vec4sArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								const short x = meshopt_quantizeSnorm(bufferData[i * 3 + 0], normalBit);
								const short y = meshopt_quantizeSnorm(bufferData[i * 3 + 1], normalBit);
								const short z = meshopt_quantizeSnorm(bufferData[i * 3 + 2], normalBit);
								newBufferData->at(i) = osg::Vec4s(x, y, z, 0);

							}
							accessor.componentType = TINYGLTF_COMPONENT_TYPE_SHORT;
							buffer.data.resize(newBufferData->getTotalDataSize());

							//TODO: account for endianess
							const unsigned char* ptr = (unsigned char*)(newBufferData->getDataPointer());
							for (size_t i = 0; i < newBufferData->getTotalDataSize(); ++i)
								buffer.data[i] = *ptr++;
							bufferView.byteLength = buffer.data.size();
							bufferView.byteStride = 2 * 4;
						}
					}

				}
				else if (accessor.type == TINYGLTF_TYPE_VEC2)
				{
					if (pair.first == "TEXCOORD_0")
					{
						std::vector<tinygltf::Accessor> test1;
						for (tinygltf::Mesh& m : _model.meshes) {
							for (const tinygltf::Primitive& pri : m.primitives) {
								if (pri.material == primitive.material) {
									test1.push_back(_model.accessors[pri.attributes.find("TEXCOORD_0")->second]);
								}
							}
						}

						double minTx = accessor.minValues[0], minTy = accessor.minValues[1], maxTx = accessor.maxValues[0], maxTy = accessor.maxValues[1];
						for (auto item : test1) {
							minTx = std::min(item.minValues[0], minTx);
							minTy = std::min(item.minValues[1], minTy);

							maxTx = std::max(item.maxValues[0], maxTx);
							maxTy = std::max(item.maxValues[1], maxTy);
						}

						osg::ref_ptr<osg::Vec2usArray> newBufferData = new osg::Vec2usArray(accessor.count);
						for (size_t i = 0; i < accessor.count; ++i)
						{
							const unsigned short x = meshopt_quantizeUnorm((bufferData[i * 2 + 0] - minTx) / (maxTx - minTx), texBit);
							const unsigned short y = meshopt_quantizeUnorm((bufferData[i * 2 + 1] - minTy) / (maxTy - minTy), texBit);
							newBufferData->at(i) = osg::Vec2us(x, y);
						}
						accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;

						buffer.data.resize(newBufferData->getTotalDataSize());
						//if (!((maxTx - minTx) > 1.0 || (maxTy - minTy) > 1.0))
							accessor.normalized = true;
						//TODO: account for endianess
						const unsigned char* ptr = (unsigned char*)(newBufferData->getDataPointer());
						for (size_t i = 0; i < newBufferData->getTotalDataSize(); ++i)
							buffer.data[i] = *ptr++;
						bufferView.byteLength = buffer.data.size();
						bufferView.byteStride = 2 * 2;

						KHR_texture_transform texture_transform_extension;
						auto& baseColorTextureTransformExtension = _model.materials[primitive.material].pbrMetallicRoughness.baseColorTexture.extensions.find(texture_transform_extension.name);
						texture_transform_extension.value = baseColorTextureTransformExtension->second.Get<tinygltf::Value::Object>();
						std::array<double, 2> offsets = texture_transform_extension.getOffset();
						offsets[0] += minTx;
						offsets[1] += minTy;
						texture_transform_extension.setOffset(offsets);

						std::array<double, 2> scales = texture_transform_extension.getScale();
						scales[0] *= (maxTx - minTx) / float((1 << texBit) - 1) * (accessor.normalized ? 65535.f : 1.f);
						scales[1] *= (maxTy - minTy) / float((1 << texBit) - 1) * (accessor.normalized ? 65535.f : 1.f);
						texture_transform_extension.setScale(scales);

						baseColorTextureTransformExtension->second = tinygltf::Value(texture_transform_extension.value);
					}
				}
				else {
					if (pair.first == "TANGENT")
					{
						const int bits = normalBit > 8 ? 8 : normalBit;
						const bool oct = compressmore && bufferView.target == TINYGLTF_TARGET_ARRAY_BUFFER;
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

						buffer.data.resize(newBufferData->getTotalDataSize());

						//TODO: account for endianess
						const unsigned char* ptr = (unsigned char*)(newBufferData->getDataPointer());
						for (size_t i = 0; i < newBufferData->getTotalDataSize(); ++i)
							buffer.data[i] = *ptr++;
						bufferView.byteLength = buffer.data.size();
						bufferView.byteStride = 1 * 4;
					}
					else if (pair.first == "COLOR_0")
					{
						if (colorBit < 9)
						{
							osg::ref_ptr<osg::Vec4bArray> newBufferData = new osg::Vec4bArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								const signed char x = quantizeColor(bufferData[i * 4 + 0], 8, colorBit);
								const signed char y = quantizeColor(bufferData[i * 4 + 1], 8, colorBit);
								const signed char z = quantizeColor(bufferData[i * 4 + 2], 8, colorBit);
								const signed char w = quantizeColor(bufferData[i * 4 + 3], 8, colorBit);

								newBufferData->at(i) = osg::Vec4b(x, y, z, w);
							}
							accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

							buffer.data.resize(newBufferData->getTotalDataSize());

							//TODO: account for endianess
							const unsigned char* ptr = (unsigned char*)(newBufferData->getDataPointer());
							for (size_t i = 0; i < newBufferData->getTotalDataSize(); ++i)
								buffer.data[i] = *ptr++;
							bufferView.byteLength = buffer.data.size();
							bufferView.byteStride = 1 * 4;
						}
						else
						{
							osg::ref_ptr<osg::Vec4sArray> newBufferData = new osg::Vec4sArray(accessor.count);
							for (size_t i = 0; i < accessor.count; ++i)
							{
								const short x = quantizeColor(bufferData[i * 4 + 0], 16, colorBit);
								const short y = quantizeColor(bufferData[i * 4 + 1], 16, colorBit);
								const short z = quantizeColor(bufferData[i * 4 + 2], 16, colorBit);
								const short w = quantizeColor(bufferData[i * 4 + 3], 16, colorBit);

								newBufferData->at(i) = osg::Vec4s(x, y, z, w);

							}
							accessor.componentType = TINYGLTF_COMPONENT_TYPE_SHORT;
							buffer.data.resize(newBufferData->getTotalDataSize());

							//TODO: account for endianess
							const unsigned char* ptr = (unsigned char*)(newBufferData->getDataPointer());
							for (size_t i = 0; i < newBufferData->getTotalDataSize(); ++i)
								buffer.data[i] = *ptr++;
							bufferView.byteLength = buffer.data.size();
							bufferView.byteStride = 2 * 4;
						}
					}
				}
			}
		}
	}
}
