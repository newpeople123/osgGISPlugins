#ifndef OSGDB_UTILS_OSG2GLTF
#define OSGDB_UTILS_OSG2GLTF 1
#include <utils/GltfUtils.h>
#include <osg/Node>
#include <osg/Geometry>
#include <osgDB/FileNameUtils>
#include <osgDB/ReaderWriter>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include <osgDB/PluginQuery>
#include <osg/MatrixTransform>
#include <stack>
#include <osg/ComputeBoundsVisitor>
#include <osg/Image>
#include <meshoptimizer.h>
#include <osgUtil/SmoothingVisitor>
#include <osgUtil/Optimizer>
class OsgToGltf :public osg::NodeVisitor {
private:
	typedef std::map<osg::ref_ptr< const osg::Node >, int> OsgNodeSequenceMap;
	typedef std::map<osg::ref_ptr<const osg::BufferData>, int> ArraySequenceMap;
	typedef std::map< osg::ref_ptr<const osg::Array>, int> AccessorSequenceMap;
	typedef std::vector< osg::ref_ptr< osg::StateSet > > StateSetStack;

	std::vector< osg::ref_ptr< osg::Texture > > _textures;

	tinygltf::Model _model;
	std::stack<tinygltf::Node*> _gltfNodeStack;
	OsgNodeSequenceMap _osgNodeSeqMap;
	ArraySequenceMap _buffers;
	ArraySequenceMap _bufferViews;
	ArraySequenceMap _accessors;
	StateSetStack _ssStack;
	osg::ref_ptr<osg::Geode> _cacheGeode = NULL;
	CompressionType _compresssionType = CompressionType::NONE;
	int _vco = 1;
	TextureType _textureType = TextureType::PNG;
	GltfUtils* _gltfUtils;
public:
	OsgToGltf(TextureType textureType, CompressionType compresstionType, int vco) :_textureType(textureType), _compresssionType(compresstionType), _vco(vco) {
		_model.asset.version = "2.0";
		_gltfUtils = new GltfUtils(_model);
		setTraversalMode(TRAVERSE_ALL_CHILDREN);
		setNodeMaskOverride(~0);

		_model.scenes.push_back(tinygltf::Scene());
		tinygltf::Scene& scene = _model.scenes.back();
		_model.defaultScene = 0;
	}
	tinygltf::Model getGltf() {
		std::sort(_model.extensionsRequired.begin(), _model.extensionsRequired.end());
		_model.extensionsRequired.erase(std::unique(_model.extensionsRequired.begin(), _model.extensionsRequired.end()), _model.extensionsRequired.end());
		std::sort(_model.extensionsUsed.begin(), _model.extensionsUsed.end());
		_model.extensionsUsed.erase(std::unique(_model.extensionsUsed.begin(), _model.extensionsUsed.end()), _model.extensionsUsed.end());
		_gltfUtils->geometryCompresstion(_compresssionType, _vco);
		return _model;
	}
private:
	void push(tinygltf::Node& gnode) {
		_gltfNodeStack.push(&gnode);
	}
	void pop() {
		_gltfNodeStack.pop();
	}
	bool pushStateSet(osg::StateSet* stateSet) {
		osg::Texture* osgTexture = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
		if (!osgTexture)
		{
			return false;
		}

		_ssStack.push_back(stateSet);
		return true;
	}
	void popStateSet() {
		_ssStack.pop_back();
	}
	unsigned getBytesInDataType(GLenum dataType)
	{
		return
			dataType == GL_BYTE || dataType == GL_UNSIGNED_BYTE ? 1 :
			dataType == GL_SHORT || dataType == GL_UNSIGNED_SHORT ? 2 :
			dataType == GL_INT || dataType == GL_UNSIGNED_INT || dataType == GL_FLOAT ? 4 :
			0;
	}
	unsigned getBytesPerElement(const osg::Array* data)
	{
		return data->getDataSize() * getBytesInDataType(data->getDataType());
	}
	unsigned getBytesPerElement(const osg::DrawElements* data)
	{
		return
			dynamic_cast<const osg::DrawElementsUByte*>(data) ? 1 :
			dynamic_cast<const osg::DrawElementsUShort*>(data) ? 2 :
			4;
	}
	int getOrCreateBuffer(const osg::BufferData* data)
	{
		ArraySequenceMap::iterator a = _buffers.find(data);
		if (a != _buffers.end())
			return a->second;

		_model.buffers.push_back(tinygltf::Buffer());
		tinygltf::Buffer& buffer = _model.buffers.back();
		int id = _model.buffers.size() - 1;
		_buffers[data] = id;

		buffer.data.resize(data->getTotalDataSize());

		//TODO: account for endianess
		unsigned char* ptr = (unsigned char*)(data->getDataPointer());
		for (unsigned i = 0; i < data->getTotalDataSize(); ++i)
			buffer.data[i] = *ptr++;

		return id;
	}
	int getOrCreateBufferView(const osg::BufferData* data, GLenum target)
	{
		try {
			ArraySequenceMap::iterator a = _bufferViews.find(data);
			if (a != _bufferViews.end())
				return a->second;

			int bufferId = -1;
			ArraySequenceMap::iterator buffersIter = _buffers.find(data);
			if (buffersIter != _buffers.end())
				bufferId = buffersIter->second;
			else
				bufferId = getOrCreateBuffer(data);

			_model.bufferViews.push_back(tinygltf::BufferView());
			tinygltf::BufferView& bv = _model.bufferViews.back();

			int id = _model.bufferViews.size() - 1;
			_bufferViews[data] = id;

			bv.buffer = bufferId;
			bv.byteLength = data->getTotalDataSize();
			bv.byteOffset = 0;
			bv.target = target;
			return id;
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
			return -1;
		}
	}

	int getOrCreateAccessor(osg::Array* data, osg::PrimitiveSet* pset, tinygltf::Primitive& prim, const std::string& attr)
	{
		ArraySequenceMap::iterator a = _accessors.find(data);
		if (a != _accessors.end())
			return a->second;

		ArraySequenceMap::iterator bv = _bufferViews.find(data);
		if (bv == _bufferViews.end())
			return -1;

		_model.accessors.push_back(tinygltf::Accessor());
		tinygltf::Accessor& accessor = _model.accessors.back();
		int accessorId = _model.accessors.size() - 1;
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
			const osg::DrawArrays* da = dynamic_cast<const osg::DrawArrays*>(pset);
			if (da)
			{
				accessor.byteOffset = da->getFirst() * getBytesPerElement(data);
				accessor.count = da->getCount();
			}
			//TODO: indexed elements
			osg::DrawElements* de = dynamic_cast<osg::DrawElements*>(pset);
			if (de)
			{
				_model.accessors.push_back(tinygltf::Accessor());
				tinygltf::Accessor& idxAccessor = _model.accessors.back();
				prim.indices = _model.accessors.size() - 1;

				idxAccessor.type = TINYGLTF_TYPE_SCALAR;
				idxAccessor.byteOffset = 0;
				const int componentType = de->getDataType();
				idxAccessor.componentType = componentType;
				idxAccessor.count = de->getNumIndices();

				getOrCreateBuffer(de);
				int idxBV = getOrCreateBufferView(de, GL_ELEMENT_ARRAY_BUFFER_ARB);

				idxAccessor.bufferView = idxBV;
			}
		}
		return accessorId;
	}

	int getCurrentMaterial()
	{
		if (_ssStack.size() > 0)
		{
			osg::ref_ptr<osg::StateSet> stateSet = _ssStack.back();
			// Try to get the current texture
			osg::Texture* osgTexture = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

			osg::Material* osgMatrial = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
			auto setGltfMaterial = [&]()->int {
				GltfPbrMetallicRoughnessMaterial* pbrMRMaterial = dynamic_cast<GltfPbrMetallicRoughnessMaterial*>(osgMatrial);
				GltfPbrSpecularGlossinessMaterial* pbrSGMaterial = dynamic_cast<GltfPbrSpecularGlossinessMaterial*>(osgMatrial);
				if (pbrMRMaterial || pbrSGMaterial) {
					return _gltfUtils->textureCompression(_textureType, stateSet);
				}
				else {
					//same as osgearth
					return _gltfUtils->textureCompression(_textureType, stateSet, osgTexture);
				}
				};
			if (osgMatrial)
			{
				int index = setGltfMaterial();
				if (index != -1) {
					return index;
				}
			}
			if (osgTexture) {
				osg::Image* osgImage = osgTexture->getImage(0);
				if (osgImage) {
					// Try to find the existing texture, which corresponds to a material index
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
				}
				int index = setGltfMaterial();
				if (index != -1) {
					_textures.push_back(osgTexture);
					return index;
				}
			}
		}
		return -1;
	}
	void apply(osg::Node& node) {

		bool isRoot = _model.scenes[_model.defaultScene].nodes.empty();
		if (isRoot)
		{
			// put a placeholder here just to prevent any other nodes
			// from thinking they are the root
			_model.scenes[_model.defaultScene].nodes.push_back(-1);
		}

		bool pushedStateSet = false;
		osg::ref_ptr< osg::StateSet > ss = node.getStateSet();
		if (ss)
		{
			pushedStateSet = pushStateSet(ss.get());
		}

		traverse(node);

		if (ss && pushedStateSet)
		{
			popStateSet();
		}

		_model.nodes.push_back(tinygltf::Node());
		tinygltf::Node& gnode = _model.nodes.back();
		int id = _model.nodes.size() - 1;
		const std::string nodeName = node.getName();
		gnode.name = nodeName;
		_osgNodeSeqMap[&node] = id;
		if (isRoot)
		{
			// replace the placeholder with the actual root id.
			_model.scenes[_model.defaultScene].nodes.back() = id;
		}
	}
	void apply(osg::Group& group)
	{
		apply(static_cast<osg::Node&>(group));

		for (unsigned i = 0; i < group.getNumChildren(); ++i)
		{
			int id = _osgNodeSeqMap[group.getChild(i)];
			_model.nodes.back().children.push_back(id);
		}
	}
	void apply(osg::MatrixTransform& xform)
	{
		//std::cout << xform << std::endl;
		apply(static_cast<osg::Group&>(xform));

		osg::Matrix matrix;
		xform.computeLocalToWorldMatrix(matrix, this);
		if (matrix != osg::Matrix::identity()) {
			const double* ptr = matrix.ptr();
			const int size = 16;
			for (unsigned i = 0; i < size; ++i)
			{
				_model.nodes.back().matrix.push_back(*ptr++);
			}
		}
	}
	void apply(osg::Drawable& drawable)
	{
		if (drawable.asGeometry())
		{
			apply(static_cast<osg::Node&>(drawable));

			osg::ref_ptr< osg::StateSet > ss = drawable.getStateSet();
			bool pushedStateSet = false;
			if (ss.valid())
			{
				pushedStateSet = pushStateSet(ss.get());
			}

			osg::Geometry* geom = drawable.asGeometry();

			_model.meshes.push_back(tinygltf::Mesh());
			tinygltf::Mesh& mesh = _model.meshes.back();
			_model.nodes.back().mesh = _model.meshes.size() - 1;

			osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
			osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
			osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
			if (!texCoords.valid())
			{
				// See if we have 3d texture coordinates and convert them to vec2
				osg::Vec3Array* texCoords3 = dynamic_cast<osg::Vec3Array*>(geom->getTexCoordArray(0));
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
			//1
			reindexMesh(geom, positions, normals, texCoords);
			//2
			mergePrimitives(geom);
			//3

			osg::Vec3f posMin(FLT_MAX, FLT_MAX, FLT_MAX);
			osg::Vec3f posMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
			normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
			texCoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
			osg::ref_ptr<osg::FloatArray> batchIds = static_cast<osg::FloatArray*>(geom->getVertexAttribArray(0));
			batchIds->resize(positions->size());
			if (!texCoords.valid())
			{
				// See if we have 3d texture coordinates and convert them to vec2
				osg::Vec3Array* texCoords3 = dynamic_cast<osg::Vec3Array*>(geom->getTexCoordArray(0));
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
					}
					if (!v.isNaN()) {
						posMax.x() = osg::maximum(posMax.x(), v.x());
						posMax.y() = osg::maximum(posMax.y(), v.y());
						posMax.z() = osg::maximum(posMax.z(), v.z());
					}
				}
			}
			if (normals.valid())
			{
				getOrCreateBufferView(normals, GL_ARRAY_BUFFER_ARB);
			}

			osg::ref_ptr<osg::Vec4Array> colors = dynamic_cast<osg::Vec4Array*>(geom->getColorArray());
			if (colors.valid())
			{
				getOrCreateBufferView(colors, GL_ARRAY_BUFFER_ARB);
			}


			for (unsigned i = 0; i < geom->getNumPrimitiveSets(); ++i)
			{

				osg::ref_ptr<osg::PrimitiveSet> pset = geom->getPrimitiveSet(i);

				mesh.primitives.push_back(tinygltf::Primitive());
				tinygltf::Primitive& primitive = mesh.primitives.back();

				int currentMaterial = getCurrentMaterial();
				if (currentMaterial >= 0)
				{
					// Cesium may crash if using texture without texCoords
					// gltf_validator will report it as errors
					// ThreeJS seems to be fine though
					// TODO: check if the material actually has any texture in it
					// TODO: the material should not be added if not used anywhere
					if (texCoords.valid()) {
						primitive.material = currentMaterial;
						getOrCreateBufferView(texCoords.get(), GL_ARRAY_BUFFER_ARB);
					}
				}

				primitive.mode = pset->getMode();
				if (positions.valid()) {
					int a = getOrCreateAccessor(positions, pset, primitive, "POSITION");
					if (a > -1) {
						// record min/max for position array (required):
						tinygltf::Accessor& posacc = _model.accessors[a];
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
					if (batchIds) {
						getOrCreateBufferView(batchIds, GL_ARRAY_BUFFER_ARB);
						getOrCreateAccessor(batchIds, pset, primitive, "_BATCHID");
					}
				}
			}

			if (pushedStateSet)
			{
				popStateSet();
			}
		}
	}
	void mergePrimitives(osg::Geometry* geom) {
		osgUtil::Optimizer optimizer;
		optimizer.optimize(geom, osgUtil::Optimizer::INDEX_MESH);
		osg::ref_ptr<osg::PrimitiveSet> mergePrimitiveset = NULL;
		const unsigned int numPrimitiveSets = geom->getNumPrimitiveSets();
		for (unsigned i = 0; i < numPrimitiveSets; ++i) {
			osg::PrimitiveSet* pset = geom->getPrimitiveSet(i);
			const GLenum mode = pset->getMode();
			osg::PrimitiveSet::Type type = pset->getType();
			switch (type)
			{
			case osg::PrimitiveSet::PrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArraysPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArrayLengthsPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUBytePrimitiveType:
				if (mergePrimitiveset == NULL) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = osg::clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					osg::DrawElementsUByte* primitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(pset);
					osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					if (mergeType == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
						osg::DrawElementsUByte* mergePrimitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(mergePrimitiveset.get());
						mergePrimitiveUByte->insert(mergePrimitiveUByte->end(), primitiveUByte->begin(), primitiveUByte->end());
					}
					else {

						osg::DrawElementsUShort* mergePrimitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(mergePrimitiveset.get());
						if (mergeType == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {

							for (unsigned int k = 0; k < primitiveUByte->getNumIndices(); ++k) {
								unsigned short index = primitiveUByte->at(k);
								mergePrimitiveUShort->push_back(index);
							}
						}
						else if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
							osg::DrawElementsUInt* mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
							for (unsigned int k = 0; k < primitiveUByte->getNumIndices(); ++k) {
								unsigned int index = primitiveUByte->at(k);
								mergePrimitiveUInt->push_back(index);
							}
						}
					}
				}

				break;
			case osg::PrimitiveSet::DrawElementsUShortPrimitiveType:
				if (mergePrimitiveset == NULL) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = osg::clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					osg::ref_ptr<osg::DrawElementsUShort> primitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(pset);
					osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					osg::ref_ptr<osg::DrawElementsUShort> mergePrimitiveUShort = dynamic_cast<osg::DrawElementsUShort*>(mergePrimitiveset.get());
					if (mergeType == osg::PrimitiveSet::DrawElementsUShortPrimitiveType) {
						mergePrimitiveUShort->insert(mergePrimitiveUShort->end(), primitiveUShort->begin(), primitiveUShort->end());
						primitiveUShort.release();
					}
					else {
						if (mergeType == osg::PrimitiveSet::DrawElementsUBytePrimitiveType) {
							osg::DrawElementsUByte* mergePrimitiveUByte = dynamic_cast<osg::DrawElementsUByte*>(mergePrimitiveset.get());
							osg::DrawElementsUShort* newMergePrimitvieUShort = new osg::DrawElementsUShort;
							for (unsigned int k = 0; k < mergePrimitiveUByte->getNumIndices(); ++k) {
								unsigned short index = mergePrimitiveUByte->at(k);
								newMergePrimitvieUShort->push_back(index);
							}
							newMergePrimitvieUShort->insert(newMergePrimitvieUShort->end(), primitiveUShort->begin(), primitiveUShort->end());
							primitiveUShort.release();
							mergePrimitiveset.release();
							mergePrimitiveset = newMergePrimitvieUShort;
						}
						else if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
							osg::DrawElementsUInt* mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
							for (unsigned int k = 0; k < primitiveUShort->getNumIndices(); ++k) {
								unsigned int index = primitiveUShort->at(k);
								mergePrimitiveUInt->push_back(index);
							}
							primitiveUShort.release();
						}
					}

				}
				break;
			case osg::PrimitiveSet::DrawElementsUIntPrimitiveType:
				if (mergePrimitiveset == NULL) {
					if (geom->getNumPrimitiveSets() > 1)
						mergePrimitiveset = osg::clone(pset, osg::CopyOp::DEEP_COPY_ALL);
				}
				else {
					osg::ref_ptr<osg::DrawElementsUInt> primitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(pset);
					osg::PrimitiveSet::Type mergeType = mergePrimitiveset->getType();
					if (mergeType == osg::PrimitiveSet::DrawElementsUIntPrimitiveType) {
						osg::DrawElementsUInt* mergePrimitiveUInt = dynamic_cast<osg::DrawElementsUInt*>(mergePrimitiveset.get());
						mergePrimitiveUInt->insert(mergePrimitiveUInt->end(), primitiveUInt->begin(), primitiveUInt->end());
						primitiveUInt.release();
					}
					else {
						osg::DrawElements* mergePrimitive = dynamic_cast<osg::DrawElements*>(mergePrimitiveset.get());
						osg::DrawElementsUInt* newMergePrimitvieUInt = new osg::DrawElementsUInt;
						for (unsigned int k = 0; k < mergePrimitive->getNumIndices(); ++k) {
							unsigned int index = mergePrimitive->getElement(k);
							newMergePrimitvieUInt->push_back(index);
						}
						newMergePrimitvieUInt->insert(newMergePrimitvieUInt->end(), primitiveUInt->begin(), primitiveUInt->end());
						primitiveUInt.release();
						mergePrimitiveset.release();
						mergePrimitiveset = newMergePrimitvieUInt;
					}
				}
				break;
			case osg::PrimitiveSet::MultiDrawArraysPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawArraysIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUByteIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUShortIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::DrawElementsUIntIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawArraysIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUByteIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUShortIndirectPrimitiveType:
				break;
			case osg::PrimitiveSet::MultiDrawElementsUIntIndirectPrimitiveType:
				break;
			default:
				break;
			}
		}
		if (mergePrimitiveset != NULL) {
			for (unsigned i = 0; i < numPrimitiveSets; ++i) {
				geom->removePrimitiveSet(0);
			}
			geom->addPrimitiveSet(mergePrimitiveset);
		}
	}
	void reindexMesh(osg::Geometry* geom, osg::ref_ptr<osg::Vec3Array>& positions, osg::ref_ptr<osg::Vec3Array>& normals, osg::ref_ptr<osg::Vec2Array>& texCoords) {
		//reindexmesh
		const unsigned int num = geom->getNumPrimitiveSets();
		if (num>0) {
			for (unsigned int kk = 0; kk < num; ++kk) {
				osg::ref_ptr<osg::DrawElements> drawElements = dynamic_cast<osg::DrawElements*>(geom->getPrimitiveSet(kk));
				if (drawElements.valid() && positions.valid()) {
					const unsigned int numIndices = drawElements->getNumIndices();
					std::vector<meshopt_Stream> streams;
					struct Attr
					{
						float f[4];
					};
					const size_t count = positions->size();
					std::vector<Attr> vertexData, normalData, texCoordData;
					for (size_t i = 0; i < count; ++i)
					{
						const osg::Vec3& vertex = positions->at(i);
						Attr v;
						v.f[0] = vertex.x();
						v.f[1] = vertex.y();
						v.f[2] = vertex.z();
						v.f[3] = 0.0;

						vertexData.push_back(v);

						if (normals.valid()) {
							const osg::Vec3& normal = normals->at(i);
							Attr n;
							n.f[0] = normal.x();
							n.f[1] = normal.y();
							n.f[2] = normal.z();
							n.f[3] = 0.0;
							normalData.push_back(n);
						}
						if (texCoords.valid()) {
							const osg::Vec2& texCoord = texCoords->at(i);
							Attr t;
							t.f[0] = texCoord.x();
							t.f[1] = texCoord.y();
							t.f[2] = 0.0;
							t.f[3] = 0.0;
							texCoordData.push_back(t);
						}
					}
					meshopt_Stream vertexStream = { &vertexData[0], sizeof(Attr), sizeof(Attr) };
					streams.push_back(vertexStream);
					if (normals.valid()) {
						meshopt_Stream normalStream = { &normalData[0], sizeof(Attr), sizeof(Attr) };
						streams.push_back(normalStream);
					}
					if (texCoords.valid()) {
						meshopt_Stream texCoordStream = { &texCoordData[0], sizeof(Attr), sizeof(Attr) };
						streams.push_back(texCoordStream);
					}

					osg::ref_ptr<osg::DrawElementsUShort> drawElementsUShort = dynamic_cast<osg::DrawElementsUShort*>(geom->getPrimitiveSet(kk));
					if (drawElementsUShort.valid()) {
						osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray;
						for (unsigned int i = 0; i < numIndices; ++i)
						{
							unsigned int index = drawElementsUShort->at(i);
							indices->push_back(index);
						}
						std::vector<unsigned int> remap(positions->size());
						size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(&remap[0], &(*indices)[0], indices->size(), positions->size(), &streams[0], streams.size());

						//size_t uniqueVertexCount = meshopt_generateVertexRemap(&remap[0], &(*indices)[0], indices->size(), &(*positions)[0].x(), positions->size(), sizeof(osg::Vec3));
						osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(uniqueVertexCount);
						osg::ref_ptr<osg::UShortArray> optimizedIndices = new osg::UShortArray(indices->size());
						meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), &remap[0]);
						meshopt_remapVertexBuffer(&vertexData[0], &vertexData[0], positions->size(), sizeof(Attr), &remap[0]);
						vertexData.resize(uniqueVertexCount);
						if (normals.valid()) {
							meshopt_remapVertexBuffer(&normalData[0], &normalData[0], normals->size(), sizeof(Attr), &remap[0]);
							normalData.resize(uniqueVertexCount);
						}
						if (texCoords.valid()) {
							meshopt_remapVertexBuffer(&texCoordData[0], &texCoordData[0], texCoords->size(), sizeof(Attr), &remap[0]);
							texCoordData.resize(uniqueVertexCount);
						}

						for (size_t i = 0; i < uniqueVertexCount; ++i) {
							optimizedVertices->at(i) = osg::Vec3(vertexData[i].f[0], vertexData[i].f[1], vertexData[i].f[2]);
							if (normals.valid())
							{
								osg::Vec3 n(normalData[i].f[0], normalData[i].f[1], normalData[i].f[2]);
								n.normalize();
								optimizedNormals->at(i) = n;
							}
							if (texCoords.valid())
								optimizedTexCoords->at(i) = osg::Vec2(texCoordData[i].f[0], texCoordData[i].f[1]);
						}
						geom->setVertexArray(optimizedVertices);
						if (normals.valid())
							geom->setNormalArray(optimizedNormals);
						if (texCoords.valid())
							geom->setTexCoordArray(0, optimizedTexCoords);
#pragma region filterTriangles
						size_t newNumIndices = 0;

						for (size_t i = 0; i < numIndices; i += 3) {
							unsigned short a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);

							if (a != b && a != c && b != c)
							{
								optimizedIndices->at(newNumIndices) = a;
								optimizedIndices->at(newNumIndices + 1) = b;
								optimizedIndices->at(newNumIndices + 2) = c;
								newNumIndices += 3;
							}
						}
						optimizedIndices->resize(newNumIndices);
#pragma endregion

						geom->setPrimitiveSet(kk, new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));

					}
					else {
						osg::ref_ptr<osg::DrawElementsUInt> drawElementsUInt = dynamic_cast<osg::DrawElementsUInt*>(geom->getPrimitiveSet(kk));
						osg::ref_ptr<osg::UIntArray> indices = new osg::UIntArray;
						for (unsigned int i = 0; i < numIndices; ++i)
						{
							indices->push_back(drawElementsUInt->at(i));
						}
						std::vector<unsigned int> remap(positions->size());
						size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(&remap[0], &(*indices)[0], indices->size(), positions->size(), &streams[0], streams.size());

						//size_t uniqueVertexCount = meshopt_generateVertexRemap(&remap[0], &(*indices)[0], indices->size(), &(*positions)[0].x(), positions->size(), sizeof(osg::Vec3));
						osg::ref_ptr<osg::Vec3Array> optimizedVertices = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec3Array> optimizedNormals = new osg::Vec3Array(uniqueVertexCount);
						osg::ref_ptr<osg::Vec2Array> optimizedTexCoords = new osg::Vec2Array(uniqueVertexCount);
						osg::ref_ptr<osg::UIntArray> optimizedIndices = new osg::UIntArray(indices->size());
						meshopt_remapIndexBuffer(&(*optimizedIndices)[0], &(*indices)[0], indices->size(), &remap[0]);
						meshopt_remapVertexBuffer(&vertexData[0], &vertexData[0], positions->size(), sizeof(Attr), &remap[0]);
						vertexData.resize(uniqueVertexCount);
						if (normals.valid()) {
							meshopt_remapVertexBuffer(&normalData[0], &normalData[0], normals->size(), sizeof(Attr), &remap[0]);
							normalData.resize(uniqueVertexCount);
						}
						if (texCoords.valid()) {
							meshopt_remapVertexBuffer(&texCoordData[0], &texCoordData[0], texCoords->size(), sizeof(Attr), &remap[0]);
							texCoordData.resize(uniqueVertexCount);
						}
						for (size_t i = 0; i < uniqueVertexCount; ++i) {
							optimizedVertices->at(i) = osg::Vec3(vertexData[i].f[0], vertexData[i].f[1], vertexData[i].f[2]);
							if (normals.valid()) {
								osg::Vec3 n(normalData[i].f[0], normalData[i].f[1], normalData[i].f[2]);
								n.normalize();
								optimizedNormals->at(i) = osg::Vec3(normalData[i].f[0], normalData[i].f[1], normalData[i].f[2]);
							}
							if (texCoords.valid())
								optimizedTexCoords->at(i) = osg::Vec2(texCoordData[i].f[0], texCoordData[i].f[1]);
						}
						geom->setVertexArray(optimizedVertices);
						if (normals.valid())
							geom->setNormalArray(optimizedNormals);
						if (texCoords.valid())
							geom->setTexCoordArray(0, optimizedTexCoords);
#pragma region filterTriangles
						size_t newNumIndices = 0;
						for (size_t i = 0; i < numIndices; i += 3) {
							unsigned int a = optimizedIndices->at(i), b = optimizedIndices->at(i + 1), c = optimizedIndices->at(i + 2);

							if (a != b && a != c && b != c)
							{
								optimizedIndices->at(newNumIndices) = a;
								optimizedIndices->at(newNumIndices + 1) = b;
								optimizedIndices->at(newNumIndices + 2) = c;
								newNumIndices += 3;
							}
						}
						optimizedIndices->resize(newNumIndices);
#pragma endregion
						geom->setPrimitiveSet(kk, new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, optimizedIndices->size(), &(*optimizedIndices)[0]));
					}

				}
			}
		}
	}
};
#endif // !OSGDB_UTILS_OSG2GLTF
