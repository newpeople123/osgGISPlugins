#include "osgdb_gltf/Osg2Gltf.h"
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/Geode>
#include <osgDB/FileNameUtils>
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <osgDB/ConvertUTF>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include "osgdb_gltf/material/GltfPbrSGMaterial.h"
using namespace osgGISPlugins;
int Osg2Gltf::getCurrentMaterial(tinygltf::Material &gltfMaterial)
{

	for (int i = 0; i < _model.materials.size(); ++i)
	{
		if (gltfMaterial == _model.materials.at(i))
		{
			return i;
		}
	}
	return -1;
}

void Osg2Gltf::apply(osg::Node &node)
{
	const bool isRoot = _model.scenes[_model.defaultScene].nodes.empty();
	if (isRoot)
	{
		// put a placeholder here just to prevent any other nodes
		// from thinking they are the root
		_model.scenes[_model.defaultScene].nodes.push_back(-1);
	}

	bool pushedStateSet = false;
	const osg::ref_ptr<osg::StateSet> ss = node.getStateSet();
	if (ss)
	{
		pushedStateSet = pushStateSet(ss.get());
	}

	traverse(node);

	if (ss && pushedStateSet)
	{
		popStateSet();
	}

	_model.nodes.emplace_back();
	tinygltf::Node &gnode = _model.nodes.back();
	const int id = _model.nodes.size() - 1;
	const std::string nodeName = node.getName();
	gnode.name = nodeName;
	_osgNodeSeqMap[&node] = id;
	if (isRoot)
	{
		// replace the placeholder with the actual root id.
		_model.scenes[_model.defaultScene].nodes.back() = id;
	}
}

void Osg2Gltf::apply(osg::Group &group)
{
	apply(static_cast<osg::Node &>(group));

	for (unsigned i = 0; i < group.getNumChildren(); ++i)
	{
		const int id = _osgNodeSeqMap[group.getChild(i)];
		if (_model.nodes.size() - 1 != id)
			_model.nodes.back().children.push_back(id);
	}
}

void Osg2Gltf::apply(osg::MatrixTransform &xform)
{
	apply(static_cast<osg::Group &>(xform));

	osg::Matrix matrix;
	xform.computeLocalToWorldMatrix(matrix, this);
	if (matrix != osg::Matrix::identity())
	{
		// 向gltf中写入translation、rotation、scale，与matrix互斥
		{
			osg::Vec3d translation;
			osg::Quat rotation;
			osg::Vec3d scale;
			osg::Quat so; // scale orientation, not used in glTF but required by decompose

			matrix.decompose(translation, rotation, scale, so);

			_model.nodes.back().translation.push_back(translation.x());
			_model.nodes.back().translation.push_back(translation.y());
			_model.nodes.back().translation.push_back(translation.z());

			_model.nodes.back().scale.push_back(scale.x());
			_model.nodes.back().scale.push_back(scale.y());
			_model.nodes.back().scale.push_back(scale.z());

			osg::Vec4 rotationVec = rotation.asVec4();
			_model.nodes.back().rotation.push_back(rotationVec.x());
			_model.nodes.back().rotation.push_back(rotationVec.y());
			_model.nodes.back().rotation.push_back(rotationVec.z());
			_model.nodes.back().rotation.push_back(rotationVec.w());
		}

		// 向gltf中写入matrix，与translation、rotation、scale互斥
		//{
		//     const double* ptr = matrix.ptr();
		//     constexpr int size = 16;
		//     for (unsigned i = 0; i < size; ++i)
		//     {
		//         _model.nodes.back().matrix.push_back(*ptr++);
		//     }
		// }
	}
}

void Osg2Gltf::apply(osg::Drawable &drawable)
{
	const osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
	if (geom.valid())
	{
		apply(static_cast<osg::Node &>(drawable));
		osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array *>(geom->getVertexArray());

		bool pushedStateSet = false;
		if (positions.valid() && positions->size())
		{
			const osg::ref_ptr<osg::StateSet> ss = drawable.getStateSet();
			if (ss.valid())
			{
				pushedStateSet = pushStateSet(ss.get());
			}
		}

		_model.meshes.emplace_back();
		tinygltf::Mesh &mesh = _model.meshes.back();
		mesh.name = _model.nodes.back().name;
		_model.nodes.back().mesh = _model.meshes.size() - 1;

		if (positions.valid())
		{
			getOrCreateBufferView(positions, GL_ARRAY_BUFFER_ARB);
		}

		osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array *>(geom->getNormalArray());
		if (normals.valid())
		{
			getOrCreateBufferView(normals, GL_ARRAY_BUFFER_ARB);
		}

		osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array *>(geom->getTexCoordArray(0));
		if (!texCoords.valid())
		{
			// See if we have 3d texture coordinates and convert them to vec2
			const osg::Vec3Array *texCoords3 = dynamic_cast<osg::Vec3Array *>(geom->getTexCoordArray(0));
			if (texCoords3)
			{
				texCoords = new osg::Vec2Array;
				for (unsigned int i = 0; i < texCoords3->size(); i++)
				{
					texCoords->push_back(osg::Vec2((*texCoords3)[i].x(), (*texCoords3)[i].y()));
				}
				// geom->setTexCoordArray(0, texCoords.get());
			}
		}

		osg::ref_ptr<osg::Vec4Array> colors = dynamic_cast<osg::Vec4Array *>(geom->getColorArray());
		if (colors.valid())
		{
			const osg::Geometry::AttributeBinding colorAttrBinding = geom->getColorBinding();
			if (colorAttrBinding & osg::Geometry::AttributeBinding::BIND_PER_VERTEX)
			{
				getOrCreateBufferView(colors, GL_ARRAY_BUFFER_ARB);
			}
			else if (colorAttrBinding & osg::Geometry::AttributeBinding::BIND_PER_PRIMITIVE_SET)
			{
				if (colors->size())
				{
					osg::ref_ptr<osg::Vec4Array> colorsPerVertex = new osg::Vec4Array(positions->size());
					std::fill(colorsPerVertex->begin(), colorsPerVertex->end(), colors->at(0));
					colors = colorsPerVertex;
				}
				getOrCreateBufferView(colors, GL_ARRAY_BUFFER_ARB);
			}
		}

		for (unsigned i = 0; i < geom->getNumPrimitiveSets(); ++i)
		{
			const osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(i);

			const GLenum mode = pset->getMode();
			switch (mode)
			{
			case osg::PrimitiveSet::Mode::QUADS:
				OSG_FATAL << "primitiveSet mode is osg::PrimitiveSet::Mode::QUADS,that is not supported!Please optimize the Geometry using osgUtil::Optimizer::TRISTRIP_GEOMETRY." << std::endl;
			case osg::PrimitiveSet::Mode::QUAD_STRIP:
				OSG_FATAL << "primitiveSet mode is osg::PrimitiveSet::Mode::QUAD_STRIP,that is not supported!Please optimize the Geometry using osgUtil::Optimizer::TRISTRIP_GEOMETRY." << std::endl;
			case osg::PrimitiveSet::Mode::POLYGON:
				OSG_FATAL << "primitiveSet mode is osg::PrimitiveSet::Mode::POLYGON,that is not supported!Please optimize the Geometry using osgUtil::Optimizer::TRISTRIP_GEOMETRY." << std::endl;
			case osg::PrimitiveSet::Mode::LINES_ADJACENCY:
				OSG_FATAL << "primitiveSet mode is osg::PrimitiveSet::Mode::LINES_ADJACENCY,that is not supported!" << std::endl;
			case osg::PrimitiveSet::Mode::LINE_STRIP_ADJACENCY:
				OSG_FATAL << "primitiveSet mode is osg::PrimitiveSet::Mode::LINE_STRIP_ADJACENCY,that is not supported!" << std::endl;
			case osg::PrimitiveSet::Mode::TRIANGLES_ADJACENCY:
				OSG_FATAL << "primitiveSet mode is osg::PrimitiveSet::Mode::TRIANGLES_ADJACENCY,that is not supported!Please optimize the Geometry using osgUtil::Optimizer::TRISTRIP_GEOMETRY." << std::endl;
			case osg::PrimitiveSet::Mode::TRIANGLE_STRIP_ADJACENCY:
				OSG_FATAL << "primitiveSet mode is osg::PrimitiveSet::Mode::TRIANGLE_STRIP_ADJACENCY,that is not supported!Please optimize the Geometry using osgUtil::Optimizer::TRISTRIP_GEOMETRY." << std::endl;
			case osg::PrimitiveSet::Mode::PATCHES:
				OSG_FATAL << "primitiveSet mode is osg::PrimitiveSet::Mode::PATCHES,that is not supported!Please optimize the Geometry using osgUtil::Optimizer::TRISTRIP_GEOMETRY." << std::endl;
			default:
				break;
			}

			mesh.primitives.emplace_back();
			tinygltf::Primitive &primitive = mesh.primitives.back();

			const int currentMaterial = getCurrentMaterial(); // 没有纹理坐标未必没有材质，可以是纯色的材质
			if (currentMaterial >= 0)
			{
				// Cesium may crash if using texture without texCoords
				// gltf_validator will report it as errors
				// ThreeJS seems to be fine though
				primitive.material = currentMaterial;
			}
			if (texCoords.valid() && texCoords->size() == positions->size())
			{
				getOrCreateBufferView(texCoords.get(), GL_ARRAY_BUFFER_ARB);

				const int texAccessorIndex = getOrCreateAccessor(texCoords.get(), pset, primitive, "TEXCOORD_0");
				if (texAccessorIndex > -1)
				{
					osg::Vec2f texMin(FLT_MAX, FLT_MAX);
					osg::Vec2f texMax(-FLT_MAX, -FLT_MAX);
					for (const osg::Vec2 &t : *texCoords)
					{
						if (!t.isNaN())
						{
							texMin.x() = osg::minimum(texMin.x(), t.x());
							texMin.y() = osg::minimum(texMin.y(), t.y());

							texMax.x() = osg::maximum(texMax.x(), t.x());
							texMax.y() = osg::maximum(texMax.y(), t.y());
						}
					}
					tinygltf::Accessor &texacc = _model.accessors[texAccessorIndex];
					texacc.minValues = {texMin.x(), texMin.y()};
					texacc.maxValues = {texMax.x(), texMax.y()};
				}
			}

			primitive.mode = mode;

			if (positions.valid())
			{
				const int posAccessorIndex = getOrCreateAccessor(positions, pset, primitive, "POSITION");
				if (posAccessorIndex > -1)
				{
					osg::Vec3f posMin(FLT_MAX, FLT_MAX, FLT_MAX);
					osg::Vec3f posMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
					for (const osg::Vec3 &v : *positions)
					{
						if (!v.isNaN())
						{
							posMin.x() = osg::minimum(posMin.x(), v.x());
							posMin.y() = osg::minimum(posMin.y(), v.y());
							posMin.z() = osg::minimum(posMin.z(), v.z());
							posMax.x() = osg::maximum(posMax.x(), v.x());
							posMax.y() = osg::maximum(posMax.y(), v.y());
							posMax.z() = osg::maximum(posMax.z(), v.z());
						}
					}
					// record min/max for position array (required):
					tinygltf::Accessor &posacc = _model.accessors[posAccessorIndex];
					posacc.minValues = {posMin.x(), posMin.y(), posMin.z()};
					posacc.maxValues = {posMax.x(), posMax.y(), posMax.z()};
				}
				if (normals.valid())
				{
					getOrCreateAccessor(normals, pset, primitive, "NORMAL");
				}
				if (colors.valid())
				{
					getOrCreateAccessor(colors, pset, primitive, "COLOR_0");
				}

				const osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray *>(geom->getVertexAttribArray(0));
				if (batchIds.valid())
				{

					if (batchIds->size() == positions->size())
					{
						getOrCreateBufferView(batchIds, GL_ARRAY_BUFFER_ARB);
						getOrCreateAccessor(batchIds, pset, primitive, "_BATCHID");
					}
				}
			}
		}

		if (pushedStateSet)
		{
			popStateSet();
		}
	}
}

void Osg2Gltf::flipGltfTextureYAxis(KHR_texture_transform &texture_transform_extension)
{
	std::array<double, 2> offsets = texture_transform_extension.getOffset();
	double offsetX = offsets[0], offsetY = offsets[1];
	std::array<double, 2> scales = texture_transform_extension.getScale();
	double scaleX = scales[0], scaleY = scales[1];

	offsetY = 1 - offsetY;
	scaleY = -scaleY;
	texture_transform_extension.setOffset({offsetX, offsetY});
	texture_transform_extension.setScale({scaleX, scaleY});
}

Osg2Gltf::Osg2Gltf() : NodeVisitor(TRAVERSE_ALL_CHILDREN)
{
	setNodeMaskOverride(~0);
	_model.asset.version = "2.0";
	_model.scenes.emplace_back();
	_model.defaultScene = 0;
}

tinygltf::Model Osg2Gltf::getGltfModel()
{
	std::sort(_model.extensionsRequired.begin(), _model.extensionsRequired.end());
	_model.extensionsRequired.erase(std::unique(_model.extensionsRequired.begin(), _model.extensionsRequired.end()), _model.extensionsRequired.end());
	std::sort(_model.extensionsUsed.begin(), _model.extensionsUsed.end());
	_model.extensionsUsed.erase(std::unique(_model.extensionsUsed.begin(), _model.extensionsUsed.end()), _model.extensionsUsed.end());
	if (!_model.meshes.size())
	{
		tinygltf::Model model;
		model.asset.version = "2.0";
		return model;
	}

	return _model;
}

Osg2Gltf::~Osg2Gltf()
{
}

void Osg2Gltf::push(tinygltf::Node &gnode)
{
	_gltfNodeStack.push(&gnode);
}

void Osg2Gltf::pop()
{
	_gltfNodeStack.pop();
}

bool Osg2Gltf::pushStateSet(osg::StateSet *stateSet)
{
	const osg::Texture *osgTexture = dynamic_cast<osg::Texture *>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
	osg::StateSet::AttributeList list = stateSet->getAttributeList();
	const osg::Material *material = dynamic_cast<osg::Material *>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
	if (!osgTexture && !material)
	{
		return false;
	}

	_ssStack.emplace_back(stateSet);
	return true;
}

void Osg2Gltf::popStateSet()
{
	_ssStack.pop_back();
}

unsigned Osg2Gltf::getBytesInDataType(const GLenum dataType)
{
	return dataType == GL_BYTE || dataType == GL_UNSIGNED_BYTE ? 1 : dataType == GL_SHORT || dataType == GL_UNSIGNED_SHORT					 ? 2
																 : dataType == GL_INT || dataType == GL_UNSIGNED_INT || dataType == GL_FLOAT ? 4
																																			 : 0;
}

unsigned Osg2Gltf::getBytesPerElement(const osg::Array *data)
{
	return data->getDataSize() * getBytesInDataType(data->getDataType());
}

unsigned Osg2Gltf::getBytesPerElement(const osg::DrawElements *data)
{
	return dynamic_cast<const osg::DrawElementsUByte *>(data) ? 1 : dynamic_cast<const osg::DrawElementsUShort *>(data) ? 2
																														: 4;
}

int Osg2Gltf::getOrCreateBuffer(const osg::BufferData *data)
{
	const auto a = _buffers.find(data);
	if (a != _buffers.end())
		return a->second;

	_model.buffers.emplace_back();
	tinygltf::Buffer &buffer = _model.buffers.back();
	const int id = _model.buffers.size() - 1;
	_buffers[data] = id;

	buffer.data.resize(data->getTotalDataSize());

	// TODO: account for endianess
	const unsigned char *ptr = reinterpret_cast<const unsigned char *>(data->getDataPointer());
	std::copy(ptr, ptr + data->getTotalDataSize(), buffer.data.begin());

	return id;
}

int Osg2Gltf::getOrCreateBufferView(const osg::BufferData *data, const GLenum target)
{
	try
	{
		const auto a = _bufferViews.find(data);
		if (a != _bufferViews.end())
			return a->second;

		int bufferId = -1;
		const auto buffersIter = _buffers.find(data);
		if (buffersIter != _buffers.end())
			bufferId = buffersIter->second;
		else
			bufferId = getOrCreateBuffer(data);

		_model.bufferViews.emplace_back();
		tinygltf::BufferView &bv = _model.bufferViews.back();

		const int id = _model.bufferViews.size() - 1;
		_bufferViews[data] = id;

		bv.buffer = bufferId;
		bv.byteLength = data->getTotalDataSize();
		bv.byteOffset = 0;
		bv.target = target;
		return id;
	}
	catch (const std::exception &e)
	{
		OSG_FATAL << e.what() << std::endl;
		return -1;
	}
}

int Osg2Gltf::getOrCreateAccessor(const osg::Array *data, osg::PrimitiveSet *pset, tinygltf::Primitive &prim, const std::string &attr)
{
	const auto a = _accessors.find(data);
	if (a != _accessors.end())
		return a->second;

	const auto bv = _bufferViews.find(data);
	if (bv == _bufferViews.end())
		return -1;

	_model.accessors.emplace_back();
	tinygltf::Accessor &accessor = _model.accessors.back();
	const int accessorId = _model.accessors.size() - 1;
	prim.attributes[attr] = accessorId;

	accessor.type =
		data->getDataSize() == 1 ? TINYGLTF_TYPE_SCALAR : data->getDataSize() == 2 ? TINYGLTF_TYPE_VEC2
													  : data->getDataSize() == 3   ? TINYGLTF_TYPE_VEC3
													  : data->getDataSize() == 4   ? TINYGLTF_TYPE_VEC4
																				   : TINYGLTF_TYPE_SCALAR;

	accessor.bufferView = bv->second;
	accessor.byteOffset = 0;
	accessor.componentType = data->getDataType();
	accessor.count = data->getNumElements();
	accessor.normalized = data->getNormalize();

	if (attr == "POSITION")
	{
		setPositionAccessor(data, pset, prim, accessor);
	}
	return accessorId;
}

void Osg2Gltf::setPositionAccessor(const osg::Array *data, osg::PrimitiveSet *pset, tinygltf::Primitive &prim, tinygltf::Accessor &accessor)
{
	const osg::PrimitiveSet::Type type = pset->getType();
	if (type == osg::PrimitiveSet::DrawArraysPrimitiveType)
	{
		const osg::DrawArrays *da = dynamic_cast<const osg::DrawArrays *>(pset);
		if (da)
		{
			const GLint first = da->getFirst();
			unsigned int bytes = getBytesPerElement(data);
			accessor.byteOffset = static_cast<unsigned int>(first) * static_cast<size_t>(bytes);

			accessor.count = da->getCount();
		}
	}
	else if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType ||
			 type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType ||
			 type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType)
	{
		const osg::DrawElements *de = dynamic_cast<osg::DrawElements *>(pset);
		if (de)
		{
			prim.indices = _model.accessors.size();
			_model.accessors.emplace_back();
			tinygltf::Accessor &idxAccessor = _model.accessors.back();

			idxAccessor.type = TINYGLTF_TYPE_SCALAR;
			idxAccessor.byteOffset = 0;
			if (type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType)
				idxAccessor.componentType = 5121;
			else if (type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType)
				idxAccessor.componentType = 5123;
			else if (type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType)
				idxAccessor.componentType = 5125;

			idxAccessor.count = de->getNumIndices();

			getOrCreateBuffer(de);
			const int idxBV = getOrCreateBufferView(de, GL_ELEMENT_ARRAY_BUFFER_ARB);

			idxAccessor.bufferView = idxBV;
		}
	}
	else
	{
		OSG_FATAL << "primitiveSet type is " << type << ",that is not supported!Please optimize the Geometry using osgUtil::Optimizer::INDEX_MESH." << std::endl;
	}
}

int Osg2Gltf::getCurrentMaterial()
{
	if (!_ssStack.empty())
	{
		const osg::ref_ptr<osg::StateSet> stateSet = _ssStack.back();
		tinygltf::Material gltfMaterial;
		gltfMaterial.doubleSided = ((stateSet->getMode(GL_CULL_FACE) & osg::StateAttribute::ON) == 0);
		if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON)
		{
			gltfMaterial.alphaMode = "BLEND";
		}
		else
		{
			// gltf规范中alphaMode的默认值为OPAQUE
			gltfMaterial.alphaMode = "OPAQUE";

			// gltfMaterial.alphaMode = "MASK";
			// gltfMaterial.alphaCutoff = 0.5;
		}

		const osg::ref_ptr<osg::Material> osgMaterial = dynamic_cast<osg::Material *>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
		if (osgMaterial.valid())
		{

			const std::type_info &materialId = typeid(*osgMaterial.get());
			if (materialId == typeid(GltfMaterial) || materialId == typeid(GltfPbrMRMaterial) || materialId == typeid(GltfPbrSGMaterial))
			{
				return getOsgMaterial2Material(gltfMaterial, dynamic_cast<GltfMaterial *>(osgMaterial.get()));
			}
			else
			{
				const osg::Vec4 baseColor = osgMaterial->getDiffuse(osg::Material::FRONT_AND_BACK);
				gltfMaterial.pbrMetallicRoughness.baseColorFactor = {baseColor.x(), baseColor.y(), baseColor.z(), baseColor.w()};
				gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
				const osg::ref_ptr<osg::Texture> osgTexture = dynamic_cast<osg::Texture *>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
				if (osgTexture.valid())
					return getOsgTexture2Material(gltfMaterial, osgTexture);
			}
		}
		else
		{
			const osg::ref_ptr<osg::Texture> osgTexture = dynamic_cast<osg::Texture *>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
			if (osgTexture.valid())
				return getOsgTexture2Material(gltfMaterial, osgTexture);
		}
	}
	return -1;
}

int Osg2Gltf::getOrCreateTexture(const osg::ref_ptr<osg::Texture> &osgTexture)
{
	if (!osgTexture.valid())
	{
		return -1;
	}
	if (osgTexture->getNumImages() < 0)
	{
		return -1;
	}
	osg::ref_ptr<osg::Image> osgImage = osgTexture->getImage(0);
	if (!osgImage.valid())
	{
		return -1;
	}
	std::string filename;
	osgTexture->getUserValue(BASECOLOR_TEXTURE_FILENAME, filename);
	if (filename.empty())
	{
		filename = osgImage->getFileName();
	}

	if (!osgDB::fileExists(filename))
	{
		OSG_WARN << "image file " << filename << " not found!" << std::endl;
		return -1;
	}
	std::ifstream file(osgDB::convertStringFromUTF8toCurrentCodePage(filename), std::ios::binary);
	if (!file.is_open())
	{
		OSG_FATAL << "Texture file \"" << filename << "\" exists,but failed to read.";
		return -1;
	}
	for (unsigned int i = 0; i < _textures.size(); i++)
	{
		const osg::ref_ptr<osg::Texture> existTexture = _textures[i].get();
		std::string existPathName;
		existTexture->getUserValue(BASECOLOR_TEXTURE_FILENAME, existPathName);
		if (existPathName.empty())
		{
			existPathName = existTexture->getImage(0)->getFileName();
		}
		osg::Texture::WrapMode existWrapS = existTexture->getWrap(osg::Texture::WRAP_S);
		osg::Texture::WrapMode existWrapT = existTexture->getWrap(osg::Texture::WRAP_T);
		osg::Texture::WrapMode existWrapR = existTexture->getWrap(osg::Texture::WRAP_R);
		osg::Texture::FilterMode existMinFilter = existTexture->getFilter(osg::Texture::MIN_FILTER);
		osg::Texture::FilterMode existMaxFilter = existTexture->getFilter(osg::Texture::MAG_FILTER);

		osg::Texture::WrapMode newWrapS = osgTexture->getWrap(osg::Texture::WRAP_S);
		osg::Texture::WrapMode newWrapT = osgTexture->getWrap(osg::Texture::WRAP_T);
		osg::Texture::WrapMode newWrapR = osgTexture->getWrap(osg::Texture::WRAP_R);
		osg::Texture::FilterMode newMinFilter = osgTexture->getFilter(osg::Texture::MIN_FILTER);
		osg::Texture::FilterMode newMaxFilter = osgTexture->getFilter(osg::Texture::MAG_FILTER);
		if (existPathName == filename && existWrapS == newWrapS && existWrapT == newWrapT && existWrapR == newWrapR && existMinFilter == newMinFilter && existMaxFilter == newMaxFilter)
		{
			return i;
		}
	}
	int index = _model.textures.size();
	_textures.emplace_back(osgTexture);

	const std::string ext = osgDB::getLowerCaseFileExtension(filename);
	std::string mimeType;

	if (ext == "ktx")
	{
		mimeType = "image/ktx";
	}
	else if (ext == "ktx2")
	{
		mimeType = "image/ktx2";
	}
	else if (ext == "png")
	{
		mimeType = "image/png";
	}
	else if (ext == "jpeg" || ext == "jpg")
	{
		mimeType = "image/jpeg";
	}
	else if (ext == "webp")
	{
		mimeType = "image/webp";
	}
	else
	{
		OSG_FATAL << "Error:texture's extension is: " << ext << " ,that is not supported!" << std::endl;
		return -1;
	}

	int imageIndex = _model.images.size();

	tinygltf::Image image;
	tinygltf::BufferView gltfBufferView;
	int bufferViewIndex = -1;
	bufferViewIndex = static_cast<int>(_model.bufferViews.size());
	gltfBufferView.buffer = _model.buffers.size();
	gltfBufferView.byteOffset = 0;

	std::vector<unsigned char> imageData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	gltfBufferView.byteLength = static_cast<int>(imageData.size());
	tinygltf::Buffer gltfBuffer;
	gltfBuffer.data = imageData;
	_model.buffers.push_back(gltfBuffer);
	file.close();

	gltfBufferView.target = TINYGLTF_TEXTURE_TARGET_TEXTURE2D;
	gltfBufferView.name = filename;
	_model.bufferViews.push_back(gltfBufferView);

	image.name = filename;
	image.mimeType = mimeType;
	image.bufferView = bufferViewIndex; //_model.bufferViews.size() the only bufferView in this _model
	_model.images.push_back(image);

	// Add the sampler
	tinygltf::Sampler sampler;
	osg::Texture::WrapMode wrapS = osgTexture->getWrap(osg::Texture::WRAP_S);
	osg::Texture::WrapMode wrapT = osgTexture->getWrap(osg::Texture::WRAP_T);
	osg::Texture::WrapMode wrapR = osgTexture->getWrap(osg::Texture::WRAP_R);

	// Validate the clamp mode to be compatible with webgl
	if ((wrapS == osg::Texture::CLAMP) || (wrapS == osg::Texture::CLAMP_TO_BORDER))
	{
		wrapS = osg::Texture::CLAMP_TO_EDGE;
	}
	if ((wrapT == osg::Texture::CLAMP) || (wrapT == osg::Texture::CLAMP_TO_BORDER))
	{
		wrapT = osg::Texture::CLAMP_TO_EDGE;
	}
	sampler.wrapS = wrapS;
	sampler.wrapT = wrapT;

	if (ext == "ktx2" || ext == "ktx")
	{
		sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
	}
	else
	{
		sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR; // osgTexture->getFilter(osg::Texture::MIN_FILTER);
	}
	sampler.magFilter = osgTexture->getFilter(osg::Texture::MAG_FILTER);
	int samplerIndex = -1;
	for (int i = 0; i < _model.samplers.size(); ++i)
	{
		const tinygltf::Sampler existSampler = _model.samplers.at(i);
		if (existSampler.wrapR == sampler.wrapR && existSampler.wrapT == sampler.wrapT && existSampler.minFilter == sampler.minFilter && existSampler.magFilter == sampler.magFilter)
		{
			samplerIndex = i;
		}
	}
	if (samplerIndex == -1)
	{
		samplerIndex = _model.samplers.size();
		_model.samplers.push_back(sampler);
	}
	// Add the texture
	tinygltf::Texture texture;
	if (ext == "ktx2" || ext == "ktx")
	{
		KHR_texture_basisu texture_basisu_extension;
		_model.extensionsRequired.emplace_back(texture_basisu_extension.name);
		_model.extensionsUsed.emplace_back(texture_basisu_extension.name);
		texture.source = -1;
		texture_basisu_extension.setSource(imageIndex);
		texture.extensions[texture_basisu_extension.name] = texture_basisu_extension.GetValue();
	}
	else if (ext == "webp")
	{
		EXT_texture_webp texture_webp_extension;
		_model.extensionsRequired.emplace_back(texture_webp_extension.name);
		_model.extensionsUsed.emplace_back(texture_webp_extension.name);
		texture.source = -1;
		texture_webp_extension.setSource(imageIndex);
		texture.extensions[texture_webp_extension.name] = texture_webp_extension.GetValue();
	}
	else
	{
		texture.source = imageIndex;
	}
	texture.sampler = samplerIndex;
	_model.textures.push_back(texture);
	return index;
}

int Osg2Gltf::getOsgTexture2Material(tinygltf::Material &gltfMaterial, const osg::ref_ptr<osg::Texture> &osgTexture)
{
	int index = -1;
	gltfMaterial.pbrMetallicRoughness.baseColorTexture.index = getOrCreateTexture(osgTexture);
	if (osgTexture.valid())
	{
		KHR_texture_transform texture_transform_extension;
		bool enableExtension = false;
		osgTexture->getUserValue(TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME, enableExtension);
		if (enableExtension)
		{
			double offsetX = 0.0, offsetY = 0.0, scaleX = 1.0, scaleY = 1.0;
			int texCoord = 0;
			osgTexture->getUserValue(TEX_TRANSFORM_BASECOLOR_OFFSET_X, offsetX);
			osgTexture->getUserValue(TEX_TRANSFORM_BASECOLOR_OFFSET_Y, offsetY);
			osgTexture->getUserValue(TEX_TRANSFORM_BASECOLOR_SCALE_X, scaleX);
			osgTexture->getUserValue(TEX_TRANSFORM_BASECOLOR_SCALE_Y, scaleY);
			osgTexture->getUserValue(TEX_TRANSFORM_BASECOLOR_TEXCOORD, texCoord);
			texture_transform_extension.setOffset({offsetX, offsetY});
			texture_transform_extension.setScale({scaleX, scaleY});
			texture_transform_extension.setTexCoord(texCoord);
			gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = texCoord;
		}
		flipGltfTextureYAxis(texture_transform_extension);
		_model.extensionsRequired.emplace_back(texture_transform_extension.name);
		_model.extensionsUsed.emplace_back(texture_transform_extension.name);
		gltfMaterial.pbrMetallicRoughness.baseColorTexture.extensions[texture_transform_extension.name] = texture_transform_extension.GetValue();
	}

	index = getCurrentMaterial(gltfMaterial);
	if (index == -1)
	{
		index = _model.materials.size();
		_model.materials.push_back(gltfMaterial);
	}

	return index;
}

int Osg2Gltf::getOsgMaterial2Material(tinygltf::Material &gltfMaterial, const osg::ref_ptr<GltfMaterial> &osgGltfMaterial)
{
	int index = -1;

	const osg::ref_ptr<osg::Texture2D> normalTexture = osgGltfMaterial->normalTexture;
	if (normalTexture.valid())
	{
		gltfMaterial.normalTexture.index = getOrCreateTexture(normalTexture);
		gltfMaterial.normalTexture.texCoord = 0;

		KHR_texture_transform texture_transform_extension;
		bool enableExtension = false;
		osgGltfMaterial->getUserValue(TEX_TRANSFORM_NORMAL_TEXTURE_NAME, enableExtension);
		if (enableExtension)
		{
			double offsetX = 0.0, offsetY = 0.0, scaleX = 1.0, scaleY = 1.0;
			int texCoord = 0;
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_NORMAL_OFFSET_X, offsetX);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_NORMAL_OFFSET_Y, offsetY);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_NORMAL_SCALE_X, scaleX);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_NORMAL_SCALE_Y, scaleY);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_NORMAL_TEXCOORD, texCoord);
			texture_transform_extension.setOffset({offsetX, offsetY});
			texture_transform_extension.setScale({scaleX, scaleY});
			texture_transform_extension.setTexCoord(texCoord);
			gltfMaterial.normalTexture.texCoord = texCoord;
		}
		flipGltfTextureYAxis(texture_transform_extension);
		_model.extensionsRequired.emplace_back(texture_transform_extension.name);
		_model.extensionsUsed.emplace_back(texture_transform_extension.name);
		gltfMaterial.normalTexture.extensions[texture_transform_extension.name] = texture_transform_extension.GetValue();
	}
	const osg::ref_ptr<osg::Texture2D> occlusionTexture = osgGltfMaterial->occlusionTexture;
	if (occlusionTexture.valid())
	{
		gltfMaterial.occlusionTexture.index = getOrCreateTexture(occlusionTexture);
		gltfMaterial.occlusionTexture.texCoord = 0;

		KHR_texture_transform texture_transform_extension;
		bool enableExtension = false;
		osgGltfMaterial->getUserValue(TEX_TRANSFORM_OCCLUSION_TEXTURE_NAME, enableExtension);
		if (enableExtension)
		{
			double offsetX = 0.0, offsetY = 0.0, scaleX = 1.0, scaleY = 1.0;
			int texCoord = 0;
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_OCCLUSION_OFFSET_X, offsetX);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_OCCLUSION_OFFSET_Y, offsetY);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_OCCLUSION_SCALE_X, scaleX);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_OCCLUSION_SCALE_Y, scaleY);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_OCCLUSION_TEXCOORD, texCoord);
			texture_transform_extension.setOffset({offsetX, offsetY});
			texture_transform_extension.setScale({scaleX, scaleY});
			texture_transform_extension.setTexCoord(texCoord);
			gltfMaterial.occlusionTexture.texCoord = texCoord;
		}
		flipGltfTextureYAxis(texture_transform_extension);
		_model.extensionsRequired.emplace_back(texture_transform_extension.name);
		_model.extensionsUsed.emplace_back(texture_transform_extension.name);
		gltfMaterial.occlusionTexture.extensions[texture_transform_extension.name] = texture_transform_extension.GetValue();
	}
	const osg::ref_ptr<osg::Texture2D> emissiveTexture = osgGltfMaterial->emissiveTexture;
	if (emissiveTexture.valid())
	{
		gltfMaterial.emissiveTexture.index = getOrCreateTexture(emissiveTexture);
		gltfMaterial.emissiveTexture.texCoord = 0;

		KHR_texture_transform texture_transform_extension;
		bool enableExtension = false;
		osgGltfMaterial->getUserValue(TEX_TRANSFORM_EMISSIVE_TEXTURE_NAME, enableExtension);
		if (enableExtension)
		{
			double offsetX = 0.0, offsetY = 0.0, scaleX = 1.0, scaleY = 1.0;
			int texCoord = 0;
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_EMISSIVE_OFFSET_X, offsetX);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_EMISSIVE_OFFSET_Y, offsetY);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_EMISSIVE_SCALE_X, scaleX);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_EMISSIVE_SCALE_Y, scaleY);
			osgGltfMaterial->getUserValue(TEX_TRANSFORM_EMISSIVE_TEXCOORD, texCoord);
			texture_transform_extension.setOffset({offsetX, offsetY});
			texture_transform_extension.setScale({scaleX, scaleY});
			texture_transform_extension.setTexCoord(texCoord);
			gltfMaterial.emissiveTexture.texCoord = texCoord;
		}
		flipGltfTextureYAxis(texture_transform_extension);
		_model.extensionsRequired.emplace_back(texture_transform_extension.name);
		_model.extensionsUsed.emplace_back(texture_transform_extension.name);
		gltfMaterial.emissiveTexture.extensions[texture_transform_extension.name] = texture_transform_extension.GetValue();
	}
	gltfMaterial.emissiveFactor = {osgGltfMaterial->emissiveFactor[0], osgGltfMaterial->emissiveFactor[1], osgGltfMaterial->emissiveFactor[2]};

	if (typeid(*osgGltfMaterial.get()) == typeid(GltfPbrMRMaterial))
	{
		const osg::ref_ptr<GltfPbrMRMaterial> osgGltfMRMaterial = dynamic_cast<GltfPbrMRMaterial *>(osgGltfMaterial.get());
		if (osgGltfMRMaterial->metallicRoughnessTexture.valid())
		{
			gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index = getOrCreateTexture(osgGltfMRMaterial->metallicRoughnessTexture);
			gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = 0;

			KHR_texture_transform texture_transform_extension;
			bool enableExtension = false;
			osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_MR_TEXTURE_NAME, enableExtension);
			if (enableExtension)
			{
				double offsetX = 0.0, offsetY = 0.0, scaleX = 1.0, scaleY = 1.0;
				int texCoord = 0;
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_MR_OFFSET_X, offsetX);
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_MR_OFFSET_Y, offsetY);
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_MR_SCALE_X, scaleX);
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_MR_SCALE_Y, scaleY);
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_MR_TEXCOORD, texCoord);
				texture_transform_extension.setOffset({offsetX, offsetY});
				texture_transform_extension.setScale({scaleX, scaleY});
				texture_transform_extension.setTexCoord(texCoord);
				gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = texCoord;
			}
			flipGltfTextureYAxis(texture_transform_extension);
			_model.extensionsRequired.emplace_back(texture_transform_extension.name);
			_model.extensionsUsed.emplace_back(texture_transform_extension.name);
			gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.extensions[texture_transform_extension.name] = texture_transform_extension.GetValue();
		}
		if (osgGltfMRMaterial->baseColorTexture.valid())
		{

			gltfMaterial.pbrMetallicRoughness.baseColorTexture.index = getOrCreateTexture(osgGltfMRMaterial->baseColorTexture);
			gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = 0;

			KHR_texture_transform texture_transform_extension;
			bool enableExtension = false;
			osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME, enableExtension);
			if (enableExtension)
			{
				double offsetX = 0.0, offsetY = 0.0, scaleX = 1.0, scaleY = 1.0;
				int texCoord = 0;
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_BASECOLOR_OFFSET_X, offsetX);
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_BASECOLOR_OFFSET_Y, offsetY);
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_BASECOLOR_SCALE_X, scaleX);
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_BASECOLOR_SCALE_Y, scaleY);
				osgGltfMRMaterial->getUserValue(TEX_TRANSFORM_BASECOLOR_TEXCOORD, texCoord);
				texture_transform_extension.setOffset({offsetX, offsetY});
				texture_transform_extension.setScale({scaleX, scaleY});
				texture_transform_extension.setTexCoord(texCoord);
				gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = texCoord;
			}
			flipGltfTextureYAxis(texture_transform_extension);
			_model.extensionsRequired.emplace_back(texture_transform_extension.name);
			_model.extensionsUsed.emplace_back(texture_transform_extension.name);
			gltfMaterial.pbrMetallicRoughness.baseColorTexture.extensions[texture_transform_extension.name] = texture_transform_extension.GetValue();
		}
		gltfMaterial.pbrMetallicRoughness.baseColorFactor = {
			osgGltfMRMaterial->baseColorFactor[0],
			osgGltfMRMaterial->baseColorFactor[1],
			osgGltfMRMaterial->baseColorFactor[2],
			osgGltfMRMaterial->baseColorFactor[3]};
		gltfMaterial.pbrMetallicRoughness.metallicFactor = osgGltfMRMaterial->metallicFactor;
		gltfMaterial.pbrMetallicRoughness.roughnessFactor = osgGltfMRMaterial->roughnessFactor;
	}
	for (size_t i = 0; i < osgGltfMaterial->materialExtensionsByCesiumSupport.size(); ++i)
	{
		GltfExtension *extension = osgGltfMaterial->materialExtensionsByCesiumSupport.at(i);
		if (typeid(*extension) == typeid(KHR_materials_pbrSpecularGlossiness))
		{
			KHR_materials_pbrSpecularGlossiness *pbrSpecularGlossiness_extension = dynamic_cast<KHR_materials_pbrSpecularGlossiness *>(extension);
			if (pbrSpecularGlossiness_extension->osgDiffuseTexture.valid())
			{
				pbrSpecularGlossiness_extension->diffuseTexture.texCoord = 0;
				pbrSpecularGlossiness_extension->diffuseTexture.index = getOrCreateTexture(pbrSpecularGlossiness_extension->osgDiffuseTexture);

				KHR_texture_transform texture_transform_extension;
				bool enableExtension = false;
				osgGltfMaterial->getUserValue(TEX_TRANSFORM_DIFFUSE_TEXTURE_NAME, enableExtension);
				if (enableExtension)
				{
					double offsetX = 0.0, offsetY = 0.0, scaleX = 1.0, scaleY = 1.0;
					int texCoord = 0;
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_DIFFUSE_OFFSET_X, offsetX);
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_DIFFUSE_OFFSET_Y, offsetY);
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_DIFFUSE_SCALE_X, scaleX);
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_DIFFUSE_SCALE_Y, scaleY);
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_DIFFUSE_TEXCOORD, texCoord);
					texture_transform_extension.setOffset({offsetX, offsetY});
					texture_transform_extension.setScale({scaleX, scaleY});
					texture_transform_extension.setTexCoord(texCoord);
					pbrSpecularGlossiness_extension->diffuseTexture.texCoord = texCoord;
				}
				flipGltfTextureYAxis(texture_transform_extension);
				_model.extensionsRequired.emplace_back(texture_transform_extension.name);
				_model.extensionsUsed.emplace_back(texture_transform_extension.name);
				pbrSpecularGlossiness_extension->diffuseTexture.extensions[texture_transform_extension.name] = texture_transform_extension.GetValue();
			}
			if (pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture.valid())
			{
				pbrSpecularGlossiness_extension->specularGlossinessTexture.texCoord = 0;
				pbrSpecularGlossiness_extension->specularGlossinessTexture.index = getOrCreateTexture(pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture);

				KHR_texture_transform texture_transform_extension;
				bool enableExtension = false;
				osgGltfMaterial->getUserValue(TEX_TRANSFORM_SG_TEXTURE_NAME, enableExtension);
				if (enableExtension)
				{
					double offsetX = 0.0, offsetY = 0.0, scaleX = 1.0, scaleY = 1.0;
					int texCoord = 0;
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_SG_OFFSET_X, offsetX);
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_SG_OFFSET_Y, offsetY);
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_SG_SCALE_X, scaleX);
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_SG_SCALE_Y, scaleY);
					osgGltfMaterial->getUserValue(TEX_TRANSFORM_SG_TEXCOORD, texCoord);
					texture_transform_extension.setOffset({offsetX, offsetY});
					texture_transform_extension.setScale({scaleX, scaleY});
					texture_transform_extension.setTexCoord(texCoord);
					pbrSpecularGlossiness_extension->specularGlossinessTexture.texCoord = texCoord;
				}
				flipGltfTextureYAxis(texture_transform_extension);
				_model.extensionsRequired.emplace_back(texture_transform_extension.name);
				_model.extensionsUsed.emplace_back(texture_transform_extension.name);
				pbrSpecularGlossiness_extension->specularGlossinessTexture.extensions[texture_transform_extension.name] = texture_transform_extension.GetValue();
			}
		}

		_model.extensionsRequired.emplace_back(extension->name);
		_model.extensionsUsed.emplace_back(extension->name);
		gltfMaterial.extensions[extension->name] = extension->GetValue();
	}

	index = getCurrentMaterial(gltfMaterial);
	if (index == -1)
	{
		index = _model.materials.size();
		_model.materials.push_back(gltfMaterial);
	}
	return index;
}
