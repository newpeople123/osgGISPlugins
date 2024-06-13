#include "gltf/Osgb2Gltf.h"
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Node>
void Osgb2Gltf::apply(osg::Node& node)
{
	const bool isRoot = model.scenes[model.defaultScene].nodes.empty();
	if (isRoot)
	{
		// put a placeholder here just to prevent any other nodes
		// from thinking they are the root
		model.scenes[model.defaultScene].nodes.push_back(-1);
	}

	bool pushedStateSet = false;
	const osg::ref_ptr< osg::StateSet > ss = node.getStateSet();
	if (ss)
	{
		pushedStateSet = pushStateSet(ss.get());
	}

	traverse(node);

	if (ss && pushedStateSet)
	{
		popStateSet();
	}

	model.nodes.emplace_back();
	tinygltf::Node& gnode = model.nodes.back();
	const int id = model.nodes.size() - 1;
	const std::string nodeName = node.getName();
	gnode.name = nodeName;
	_osgNodeSeqMap[&node] = id;
	if (isRoot)
	{
		// replace the placeholder with the actual root id.
		model.scenes[model.defaultScene].nodes.back() = id;
	}
}

void Osgb2Gltf::apply(osg::Group& group)
{
	apply(static_cast<osg::Node&>(group));

	for (unsigned i = 0; i < group.getNumChildren(); ++i)
	{
		int id = _osgNodeSeqMap[group.getChild(i)];
		model.nodes.back().children.push_back(id);
	}
}

void Osgb2Gltf::apply(osg::MatrixTransform& xform)
{
	apply(static_cast<osg::Group&>(xform));

	osg::Matrix matrix;
	xform.computeLocalToWorldMatrix(matrix, this);
	if (matrix != osg::Matrix::identity()) {
		const double* ptr = matrix.ptr();
		constexpr int size = 16;
		for (unsigned i = 0; i < size; ++i)
		{
			model.nodes.back().matrix.push_back(*ptr++);
		}
	}
}

void Osgb2Gltf::apply(osg::Drawable& drawable)
{
	const osg::ref_ptr<osg::Geometry> geom = drawable.asGeometry();
	if (geom.valid())
	{
		if (geom->getNumPrimitiveSets() == 0)
		{
			return;
		}
		apply(static_cast<osg::Node&>(drawable));

		const osg::ref_ptr< osg::StateSet > ss = drawable.getStateSet();
		bool pushedStateSet = false;
		if (ss.valid())
		{
			pushedStateSet = pushStateSet(ss.get());
		}

		model.meshes.emplace_back();
		tinygltf::Mesh& mesh = model.meshes.back();
		model.nodes.back().mesh = model.meshes.size() - 1;

		osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
		osg::Vec3f posMin(FLT_MAX, FLT_MAX, FLT_MAX);
		osg::Vec3f posMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		if (positions.valid())
		{
			getOrCreateBufferView(positions, GL_ARRAY_BUFFER_ARB);
			for (unsigned i = 0; i < positions->size(); ++i)
			{
				const osg::Vec3f& v = (*positions)[i];
				if (!v.isNaN()) {
					posMin.x() = osg::minimum(posMin.x(), v.x());
					posMin.y() = osg::minimum(posMin.y(), v.y());
					posMin.z() = osg::minimum(posMin.z(), v.z());
					posMax.x() = osg::maximum(posMax.x(), v.x());
					posMax.y() = osg::maximum(posMax.y(), v.y());
					posMax.z() = osg::maximum(posMax.z(), v.z());
				}
			}
		}

		osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
		if (normals.valid())
		{
			getOrCreateBufferView(normals, GL_ARRAY_BUFFER_ARB);
		}

		osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
		if (!texCoords.valid())
		{
			// See if we have 3d texture coordinates and convert them to vec2
			const auto texCoords3 = dynamic_cast<osg::Vec3Array*>(geom->getTexCoordArray(0));
			if (texCoords3)
			{
				texCoords = new osg::Vec2Array;
				for (unsigned int i = 0; i < texCoords3->size(); i++)
				{
					texCoords->push_back(osg::Vec2((*texCoords3)[i].x(), (*texCoords3)[i].y()));
				}
				//geom->setTexCoordArray(0, texCoords.get());
			}
		}

		const osg::ref_ptr<osg::Vec4Array> colors = dynamic_cast<osg::Vec4Array*>(geom->getColorArray());
		if (colors.valid())
		{
			getOrCreateBufferView(colors, GL_ARRAY_BUFFER_ARB);
		}


		for (unsigned i = 0; i < geom->getNumPrimitiveSets(); ++i)
		{

			osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(i);

			mesh.primitives.emplace_back();
			tinygltf::Primitive& primitive = mesh.primitives.back();

			const int currentMaterial = getCurrentMaterial();
			if (currentMaterial >= 0)
			{
				// Cesium may crash if using texture without texCoords
				// gltf_validator will report it as errors
				// ThreeJS seems to be fine though
				primitive.material = currentMaterial;
				if (texCoords.valid())
					getOrCreateBufferView(texCoords.get(), GL_ARRAY_BUFFER_ARB);

			}

			primitive.mode = pset->getMode();
			if (positions.valid()) {
				const int a = getOrCreateAccessor(positions, pset, primitive, "POSITION");
				if (a > -1) {
					// record min/max for position array (required):
					tinygltf::Accessor& posacc = model.accessors[a];
					posacc.minValues.push_back(posMin.x());
					posacc.minValues.push_back(posMin.y());
					posacc.minValues.push_back(posMin.z());
					posacc.maxValues.push_back(posMax.x());
					posacc.maxValues.push_back(posMax.y());
					posacc.maxValues.push_back(posMax.z());
				}
				if (normals.valid()) {
					getOrCreateAccessor(normals, pset, primitive, "NORMAL");
				}
				if (colors.valid()) {
					getOrCreateAccessor(colors, pset, primitive, "COLOR_0");
				}
				if (texCoords.valid() && currentMaterial >= 0) {
					getOrCreateAccessor(texCoords.get(), pset, primitive, "TEXCOORD_0");
				}

				const osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geom->getVertexAttribArray(0));
				if (batchIds.valid()) {
					if (batchIds->size() == positions->size()) {
						getOrCreateBufferView(batchIds, GL_ARRAY_BUFFER_ARB);
						getOrCreateAccessor(batchIds, pset, primitive, "_BATCHID");
					}
					else
					{
						if (batchIds)
							std::cerr << "Osgb2Gltf error:batchid's length is not equal position's length!" << '\n';
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

void Osgb2Gltf::push(tinygltf::Node& gnode)
{
	_gltfNodeStack.push(&gnode);
}

void Osgb2Gltf::pop()
{
	_gltfNodeStack.pop();
}

bool Osgb2Gltf::pushStateSet(osg::StateSet* stateSet)
{
	const osg::Texture* osgTexture = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
	osg::StateSet::AttributeList list = stateSet->getAttributeList();
	const osg::Material* material = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
	if (!osgTexture && !material)
	{
		return false;
	}

	_ssStack.emplace_back(stateSet);
	return true;
}

void Osgb2Gltf::popStateSet()
{
	_ssStack.pop_back();
}

unsigned Osgb2Gltf::getBytesInDataType(const GLenum dataType)
{
	return
		dataType == GL_BYTE || dataType == GL_UNSIGNED_BYTE ? 1 :
		dataType == GL_SHORT || dataType == GL_UNSIGNED_SHORT ? 2 :
		dataType == GL_INT || dataType == GL_UNSIGNED_INT || dataType == GL_FLOAT ? 4 :
		0;
}

unsigned Osgb2Gltf::getBytesPerElement(const osg::Array* data)
{
	return data->getDataSize() * getBytesInDataType(data->getDataType());
}

unsigned Osgb2Gltf::getBytesPerElement(const osg::DrawElements* data)
{
	return
		dynamic_cast<const osg::DrawElementsUByte*>(data) ? 1 :
		dynamic_cast<const osg::DrawElementsUShort*>(data) ? 2 :
		4;
}

int Osgb2Gltf::getOrCreateBuffer(const osg::BufferData* data)
{
	const auto a = _buffers.find(data);
	if (a != _buffers.end())
		return a->second;

	model.buffers.emplace_back();
	tinygltf::Buffer& buffer = model.buffers.back();
	const int id = model.buffers.size() - 1;
	_buffers[data] = id;

	buffer.data.resize(data->getTotalDataSize());

	//TODO: account for endianess
	const unsigned char* ptr = (unsigned char*)(data->getDataPointer());
	for (unsigned i = 0; i < data->getTotalDataSize(); ++i)
		buffer.data[i] = *ptr++;

	return id;
}

int Osgb2Gltf::getOrCreateBufferView(const osg::BufferData* data, const GLenum target)
{
	try {
		const auto a = _bufferViews.find(data);
		if (a != _bufferViews.end())
			return a->second;

		int bufferId = -1;
		const auto buffersIter = _buffers.find(data);
		if (buffersIter != _buffers.end())
			bufferId = buffersIter->second;
		else
			bufferId = getOrCreateBuffer(data);

		model.bufferViews.emplace_back();
		tinygltf::BufferView& bv = model.bufferViews.back();

		const int id = model.bufferViews.size() - 1;
		_bufferViews[data] = id;

		bv.buffer = bufferId;
		bv.byteLength = data->getTotalDataSize();
		bv.byteOffset = 0;
		bv.target = target;
		return id;
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << '\n';
		return -1;
	}
}

int Osgb2Gltf::getOrCreateAccessor(const osg::Array* data, osg::PrimitiveSet* pset, tinygltf::Primitive& prim, const std::string& attr)
{
	const auto a = _accessors.find(data);
	if (a != _accessors.end())
		return a->second;

	const auto bv = _bufferViews.find(data);
	if (bv == _bufferViews.end())
		return -1;

	model.accessors.emplace_back();
	tinygltf::Accessor& accessor = model.accessors.back();
	const int accessorId = model.accessors.size() - 1;
	prim.attributes[attr] = accessorId;

	accessor.type =
		data->getDataSize() == 1 ? TINYGLTF_TYPE_SCALAR :
		data->getDataSize() == 2 ? TINYGLTF_TYPE_VEC2 :
		data->getDataSize() == 3 ? TINYGLTF_TYPE_VEC3 :
		data->getDataSize() == 4 ? TINYGLTF_TYPE_VEC4 :
		TINYGLTF_TYPE_SCALAR;

	accessor.bufferView = bv->second;
	accessor.byteOffset = 0;
	accessor.componentType = data->getDataType();
	accessor.count = data->getNumElements();
	accessor.normalized = data->getNormalize();


	if (attr == "POSITION") {
		const auto da = dynamic_cast<const osg::DrawArrays*>(pset);
		if (da)
		{
			accessor.byteOffset = da->getFirst() * getBytesPerElement(data);
			accessor.count = da->getCount();
		}
		//TODO: indexed elements
		const auto de = dynamic_cast<osg::DrawElements*>(pset);
		if (de)
		{
			model.accessors.emplace_back();
			tinygltf::Accessor& idxAccessor = model.accessors.back();
			prim.indices = model.accessors.size() - 1;

			idxAccessor.type = TINYGLTF_TYPE_SCALAR;
			idxAccessor.byteOffset = 0;
			const int componentType = de->getDataType();
			idxAccessor.componentType = componentType;
			idxAccessor.count = de->getNumIndices();

			getOrCreateBuffer(de);
			const int idxBV = getOrCreateBufferView(de, GL_ELEMENT_ARRAY_BUFFER_ARB);

			idxAccessor.bufferView = idxBV;
		}
	}
	return accessorId;
}

int Osgb2Gltf::getCurrentMaterial()
{
	if (!_ssStack.empty())
	{
		const osg::ref_ptr<osg::StateSet> stateSet = _ssStack.back();
		// Try to get the current texture
		auto osgTexture = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

		const auto osgMatrial = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
		auto setGltfMaterial = [&]()->int {
			const GltfPbrMetallicRoughnessMaterial* pbrMRMaterial = dynamic_cast<GltfPbrMetallicRoughnessMaterial*>(osgMatrial);
			const GltfPbrSpecularGlossinessMaterial* pbrSGMaterial = dynamic_cast<GltfPbrSpecularGlossinessMaterial*>(osgMatrial);
			if (pbrMRMaterial || pbrSGMaterial) {
				return _gltfUtils->textureCompression(_textureType, stateSet);
			}
			else if (osgTexture) {
				//same as osgearth
				return _gltfUtils->textureCompression(_textureType, stateSet, osgTexture);
			}
			else {
				return _gltfUtils->textureCompression(_textureType, stateSet, osgMatrial);
			}
			};
		if (osgMatrial)
		{
			const int index = setGltfMaterial();
			if (index != -1) {
				return index;
			}
		}
		if (osgTexture) {
			const osg::Image* osgImage = osgTexture->getImage(0);
			if (osgImage) {
				// Try to find the existing texture, which corresponds to a material index
				for (unsigned int i = 0; i < _textures.size(); i++)
				{
					const osg::Texture* existTexture = _textures[i].get();
					const std::string existPathName = existTexture->getImage(0)->getFileName();
					const osg::Texture::WrapMode existWrapS = existTexture->getWrap(osg::Texture::WRAP_S);
					const osg::Texture::WrapMode existWrapT = existTexture->getWrap(osg::Texture::WRAP_T);
					const osg::Texture::WrapMode existWrapR = existTexture->getWrap(osg::Texture::WRAP_R);
					const osg::Texture::FilterMode existMinFilter = existTexture->getFilter(osg::Texture::MIN_FILTER);
					const osg::Texture::FilterMode existMaxFilter = existTexture->getFilter(osg::Texture::MAG_FILTER);

					const std::string newPathName = osgImage->getFileName();
					const osg::Texture::WrapMode newWrapS = osgTexture->getWrap(osg::Texture::WRAP_S);
					const osg::Texture::WrapMode newWrapT = osgTexture->getWrap(osg::Texture::WRAP_T);
					const osg::Texture::WrapMode newWrapR = osgTexture->getWrap(osg::Texture::WRAP_R);
					const osg::Texture::FilterMode newMinFilter = osgTexture->getFilter(osg::Texture::MIN_FILTER);
					const osg::Texture::FilterMode newMaxFilter = osgTexture->getFilter(osg::Texture::MAG_FILTER);
					if (existPathName == newPathName
						&& existWrapS == newWrapS
						&& existWrapT == newWrapT
						&& existWrapR == newWrapR
						&& existMinFilter == newMinFilter
						&& existMaxFilter == newMaxFilter
						)
					{
						return i;
					}
				}
			}
			const int index = setGltfMaterial();
			if (index != -1) {
				_textures.emplace_back(osgTexture);
				return index;
			}
		}
	}
	return -1;
}
