#include "gltf/Osgb2Gltf.h"
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osgDB/FileNameUtils>
int Osgb2Gltf::getCurrentMaterial(tinygltf::Material& gltfMaterial)
{
	json matJson;
	tinygltf::SerializeGltfMaterial(gltfMaterial, matJson);
	for (int i = 0; i < model.materials.size(); ++i) {
		json existMatJson;
		SerializeGltfMaterial(model.materials.at(i), existMatJson);
		if (matJson == existMatJson) {
			return i;
		}
	}
	return -1;
}

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

Osgb2Gltf::Osgb2Gltf(GltfComporessor* gltfComporessor):NodeVisitor(TRAVERSE_ALL_CHILDREN),_gltfComporessor(gltfComporessor)
{
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
		tinygltf::Material gltfMaterial;
		gltfMaterial.doubleSided = ((stateSet->getMode(GL_CULL_FACE) & osg::StateAttribute::ON) == 0);
		if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON) {
			gltfMaterial.alphaMode = "BLEND";
		}

		const osg::ref_ptr<osg::Material> osgMatrial = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
		if (osgMatrial.valid()) {
			return convertOsgMaterial2Material(gltfMaterial, osgMatrial);
		}
		else {
			const osg::ref_ptr<osg::Texture> osgTexture = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
			if(osgTexture.valid())
				return convertOsgTexture2Material(gltfMaterial, osgTexture);
		}
	}
	return -1;
}

int Osgb2Gltf::getOrCreateTexture(const osg::ref_ptr<osg::Texture>& osgTexture)
{
	if (!osgTexture.valid()) {
		return -1;
	}
	if (osgTexture->getNumImages() < 1) {
		return -1;
	}
	osg::ref_ptr<osg::Image> osgImage = osgTexture->getImage(0);
	if (!osgImage.valid()) {
		return -1;
	}
	for (unsigned int i = 0; i < _textures.size(); i++)
	{
		const osg::Texture* existTexture = _textures[i].get();
		const std::string existPathName = existTexture->getImage(0)->getFileName();
		osg::Texture::WrapMode existWrapS = existTexture->getWrap(osg::Texture::WRAP_S);
		osg::Texture::WrapMode existWrapT = existTexture->getWrap(osg::Texture::WRAP_T);
		osg::Texture::WrapMode existWrapR = existTexture->getWrap(osg::Texture::WRAP_R);
		osg::Texture::FilterMode existMinFilter = existTexture->getFilter(osg::Texture::MIN_FILTER);
		osg::Texture::FilterMode existMaxFilter = existTexture->getFilter(osg::Texture::MAG_FILTER);

		const std::string newPathName = osgImage->getFileName();
		osg::Texture::WrapMode newWrapS = osgTexture->getWrap(osg::Texture::WRAP_S);
		osg::Texture::WrapMode newWrapT = osgTexture->getWrap(osg::Texture::WRAP_T);
		osg::Texture::WrapMode newWrapR = osgTexture->getWrap(osg::Texture::WRAP_R);
		osg::Texture::FilterMode newMinFilter = osgTexture->getFilter(osg::Texture::MIN_FILTER);
		osg::Texture::FilterMode newMaxFilter = osgTexture->getFilter(osg::Texture::MAG_FILTER);
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
	int index = model.textures.size();
	_textures.emplace_back(osgTexture);
	const std::string filename = osgImage->getFileName();
	const std::string ext = osgDB::getLowerCaseFileExtension(filename);
	std::string mimeType;
	
	if (ext == "ktx") {
		mimeType = "image/ktx";
	}
	else if (ext == "ktx2") {
		mimeType = "image/ktx2";
	}
	else if (ext == "png") {
		mimeType = "image/png";
	}
	else if (ext == "jpeg"||ext=="jpg") {
		mimeType = "image/jpeg";
	}
	else if (ext == "webp") {
		mimeType = "image/webp";
	}

	tinygltf::Image image;

	tinygltf::BufferView gltfBufferView;
	int bufferViewIndex = -1;
	bufferViewIndex = static_cast<int>(model.bufferViews.size());
	gltfBufferView.buffer = model.buffers.size();
	gltfBufferView.byteOffset = 0;
	{
		std::ifstream file(filename, std::ios::binary);
		std::vector<unsigned char> imageData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		gltfBufferView.byteLength = static_cast<int>(imageData.size());
		tinygltf::Buffer gltfBuffer;
		gltfBuffer.data = imageData;
		model.buffers.push_back(gltfBuffer);
		file.close();
	}
	gltfBufferView.target = TINYGLTF_TEXTURE_TARGET_TEXTURE2D;
	gltfBufferView.name = filename;
	model.bufferViews.push_back(gltfBufferView);

	image.mimeType = mimeType;
	image.bufferView = bufferViewIndex; //model.bufferViews.size() the only bufferView in this model
	model.images.push_back(image);

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

	if (ext == "ktx2" || ext == "ktx") {
		sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
	}
	else
	{
		sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR; //osgTexture->getFilter(osg::Texture::MIN_FILTER);
	}
	sampler.magFilter = osgTexture->getFilter(osg::Texture::MAG_FILTER);
	int samplerIndex = -1;
	for (int i = 0; i < model.samplers.size(); ++i) {
		const tinygltf::Sampler existSampler = model.samplers.at(i);
		if (existSampler.wrapR == sampler.wrapR && existSampler.wrapT == sampler.wrapT && existSampler.minFilter == sampler.minFilter && existSampler.magFilter == sampler.magFilter) {
			samplerIndex = i;
		}
	}
	if (samplerIndex == -1) {
		samplerIndex = model.samplers.size();
		model.samplers.push_back(sampler);
	}
	// Add the texture
	tinygltf::Texture texture;
	if (ext == "ktx2" || ext == "ktx")
	{
		KHR_texture_basisu texture_basisu_extension;
		model.extensionsRequired.emplace_back(texture_basisu_extension.name);
		model.extensionsUsed.emplace_back(texture_basisu_extension.name);
		texture.source = -1;
		texture_basisu_extension.setSource(index);
		texture.extensions.insert(std::make_pair(texture_basisu_extension.name, texture_basisu_extension.value));
	}
	else if (ext == "webp") {
		EXT_texture_webp texture_webp_extension;
		model.extensionsRequired.emplace_back(texture_webp_extension.name);
		model.extensionsUsed.emplace_back(texture_webp_extension.name);
		texture.source = -1;
		texture_webp_extension.setSource(index);
		texture.extensions.insert(std::make_pair(texture_webp_extension.name, texture_webp_extension.value));
	}
	else {
		texture.source = index;
	}
	texture.sampler = samplerIndex;
	model.textures.push_back(texture);
	return index;
}

int Osgb2Gltf::getOsgTexture2Material(tinygltf::Material& gltfMaterial, const osg::ref_ptr<osg::Texture>& osgTexture)
{
	int index = -1;
	gltfMaterial.pbrMetallicRoughness.baseColorTexture.index = getOrCreateTexture(osgTexture);
	gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
	gltfMaterial.pbrMetallicRoughness.baseColorFactor = { 1.0,1.0,1.0,1.0 };
	gltfMaterial.pbrMetallicRoughness.metallicFactor = 0.0;
	gltfMaterial.pbrMetallicRoughness.roughnessFactor = 1.0;

	index = getCurrentMaterial(gltfMaterial);
	if (index != -1) {
		index = model.materials.size();
		model.materials.push_back(gltfMaterial);
	}

	return index;
}

int Osgb2Gltf::getOsgMaterial2Material(tinygltf::Material& gltfMaterial, const osg::ref_ptr<osg::Material>& osgMaterial)
{
	return 0;
}

