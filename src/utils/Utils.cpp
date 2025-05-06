#include "utils/Utils.h"

using namespace osgGISPlugins;

void Utils::pushStateSet2UniqueStateSets(osg::ref_ptr<osg::StateSet> stateSet, std::vector<osg::ref_ptr<osg::StateSet>>& uniqueStateSets)
{
	int index = -1;
	for (size_t j = 0; j < uniqueStateSets.size(); j++)
	{
		const osg::ref_ptr<osg::StateSet> key = uniqueStateSets.at(j);
		if (Utils::compareStateSet(key, stateSet))
		{
			index = j;
			break;
		}

	}
	if (index == -1)
		uniqueStateSets.push_back(stateSet);
}

bool Utils::isRepeatTexCoords(osg::ref_ptr<osg::Vec2Array> texCoords)
{
	for (osg::Vec2 texCoord : *texCoords.get())
	{
		const float texCoordX = osg::absolute(texCoord.x());
		const float texCoordY = osg::absolute(texCoord.y());

		if (texCoordX > 1.0 || texCoordY > 1.0)
		{
			return true;
		}
	}
	return false;
}

bool Utils::compareMatrix(const osg::Matrixd& lhs, const osg::Matrixd& rhs)
{
	for (size_t i = 0; i < 4; ++i) {
		for (size_t j = 0; j < 4; ++j) {
			if (!osg::equivalent(lhs(i, j), rhs(i, j)))
				return false;
		}
	}
	return true;
}

bool Utils::compareMatrixs(const std::vector<osg::Matrixd>& lhses, const std::vector<osg::Matrixd>& rhses)
{
	if (lhses.size() != rhses.size()) {
		return false;
	}
	for (size_t i = 0; i < lhses.size(); ++i) {
		if (!Utils::compareMatrix(lhses[i], rhses[i])) {
			return false;
		}
	}
	return true;
}

bool Utils::compareStateSet(const osg::ref_ptr<osg::StateSet>& ss1, const osg::ref_ptr<osg::StateSet>& ss2)
{
	// 快速路径
	if (ss1.get() == ss2.get()) return true;
	if (!ss1.valid() || !ss2.valid()) return false;

	// 比较渲染状态
	static const GLenum modes[] = {
		GL_BLEND,
		GL_LIGHTING,
		GL_DEPTH_TEST,
		GL_CULL_FACE,
		GL_ALPHA_TEST,
		GL_POLYGON_OFFSET_FILL,
		GL_POLYGON_OFFSET_LINE,
		GL_POLYGON_OFFSET_POINT,
		GL_LINE_SMOOTH,
		GL_POINT_SMOOTH,
		GL_POLYGON_SMOOTH
	};

	for (GLenum mode : modes) {
		if (ss1->getMode(mode) != ss2->getMode(mode))
			return false;
	}

	// 比较属性列表
	const osg::StateSet::AttributeList& attr1 = ss1->getAttributeList();
	const osg::StateSet::AttributeList& attr2 = ss2->getAttributeList();

	if (attr1.size() != attr2.size()) return false;

	// 比较材质
	const osg::Material* mat1 = dynamic_cast<const osg::Material*>(
		ss1->getAttribute(osg::StateAttribute::MATERIAL));
	const osg::Material* mat2 = dynamic_cast<const osg::Material*>(
		ss2->getAttribute(osg::StateAttribute::MATERIAL));

	if (mat1 && mat2) {
		// 检查是否是 GltfMaterial
		const GltfMaterial* gltfMat1 = dynamic_cast<const GltfMaterial*>(mat1);
		const GltfMaterial* gltfMat2 = dynamic_cast<const GltfMaterial*>(mat2);

		if (gltfMat1 && gltfMat2) {
			if (!gltfMat1->compare(*gltfMat2)) return false;
		}
		else {
			if (!mat1->compare(*mat2)) return false;
		}
	}
	else if (mat1 || mat2) {
		return false;
	}

	// 比较所有纹理单元
	const osg::StateSet::TextureAttributeList& tal1 = ss1->getTextureAttributeList();
	const osg::StateSet::TextureAttributeList& tal2 = ss2->getTextureAttributeList();
	if (tal1.size() != tal2.size()) return false;

	for (size_t unit = 0; unit < tal1.size(); ++unit) {
		const osg::StateSet::AttributeList& texAttr1 = tal1[unit];
		const osg::StateSet::AttributeList& texAttr2 = tal2[unit];

		if (texAttr1.size() != texAttr2.size()) return false;

		// 比较每个纹理属性
		for (const auto& pair : texAttr1) {
			auto it = texAttr2.find(pair.first);
			if (it == texAttr2.end()) return false;

			const osg::StateAttribute* sa1 = pair.second.first.get();
			const osg::StateAttribute* sa2 = it->second.first.get();

			// 特别处理 Texture2D
			const osg::Texture2D* tex1 = dynamic_cast<const osg::Texture2D*>(sa1);
			const osg::Texture2D* tex2 = dynamic_cast<const osg::Texture2D*>(sa2);
			if (tex1 && tex2) {
				if (!GltfMaterial::compareTexture2D(
					const_cast<osg::Texture2D*>(tex1),
					const_cast<osg::Texture2D*>(tex2)))
					return false;
			}
			else {
				if (sa1->compare(*sa2) != 0) return false;
			}
		}
	}

	// 比较 uniform 变量
	const osg::StateSet::UniformList& uniforms1 = ss1->getUniformList();
	const osg::StateSet::UniformList& uniforms2 = ss2->getUniformList();

	if (uniforms1.size() != uniforms2.size()) return false;

	for (const auto& pair : uniforms1) {
		auto it = uniforms2.find(pair.first);
		if (it == uniforms2.end()) return false;

		const osg::Uniform* u1 = pair.second.first.get();
		const osg::Uniform* u2 = it->second.first.get();

		if (u1->getType() != u2->getType()) return false;
		if (u1->getNumElements() != u2->getNumElements()) return false;

		// 比较 uniform 值
		if (!compareUniformValue(*u1, *u2)) return false;
	}

	return true;

}

bool Utils::comparePrimitiveSet(const osg::ref_ptr<osg::PrimitiveSet>& pSet1, const osg::ref_ptr<osg::PrimitiveSet>& pSet2)
{
	// 快速路径：相同指针
	if (pSet1.get() == pSet2.get()) return true;
	if (!pSet1.valid() || !pSet2.valid()) return false;

	// 基本属性比较
	if (pSet1->getMode() != pSet2->getMode()) return false;
	if (pSet1->getType() != pSet2->getType()) return false;
	if (pSet1->getNumIndices() != pSet2->getNumIndices()) return false;

	// 根据具体类型比较
	if (osg::DrawElements* de1 = dynamic_cast<osg::DrawElements*>(pSet1.get()))
	{
		osg::DrawElements* de2 = dynamic_cast<osg::DrawElements*>(pSet2.get());
		if (!de2) return false;
		return compareDrawElements(de1, de2);
	}
	else if (osg::DrawArrays* da1 = dynamic_cast<osg::DrawArrays*>(pSet1.get()))
	{
		osg::DrawArrays* da2 = dynamic_cast<osg::DrawArrays*>(pSet2.get());
		if (!da2) return false;
		return compareDrawArrays(da1, da2);
	}

	return false;
}

bool Utils::compareDrawElements(const osg::DrawElements* de1, const osg::DrawElements* de2)
{
	if (!de1 || !de2) return false;
	if (de1->getType() != de2->getType()) return false;

	switch (de1->getType())
	{
	case osg::PrimitiveSet::DrawElementsUBytePrimitiveType:
		return compareDrawElementsTemplate<GLubyte, osg::DrawElementsUByte>(
			static_cast<const osg::DrawElementsUByte*>(de1),
			static_cast<const osg::DrawElementsUByte*>(de2));
	case osg::PrimitiveSet::DrawElementsUShortPrimitiveType:
		return compareDrawElementsTemplate<GLushort, osg::DrawElementsUShort>(
			static_cast<const osg::DrawElementsUShort*>(de1),
			static_cast<const osg::DrawElementsUShort*>(de2));
	case osg::PrimitiveSet::DrawElementsUIntPrimitiveType:
		return compareDrawElementsTemplate<GLuint, osg::DrawElementsUInt>(
			static_cast<const osg::DrawElementsUInt*>(de1),
			static_cast<const osg::DrawElementsUInt*>(de2));
	default:
		return false;
	}
}

bool Utils::compareGeometry(const osg::ref_ptr<osg::Geometry>& geom1, const osg::ref_ptr<osg::Geometry>& geom2)
{
	// 快速路径
	if (geom1.get() == geom2.get()) return true;
	if (!geom1.valid() || !geom2.valid()) return false;

	// 基本属性比较
	if (geom1->getNumPrimitiveSets() != geom2->getNumPrimitiveSets()) return false;

	// 顶点数组比较（最重要的）
	if (!compareArray(geom1->getVertexArray(), geom2->getVertexArray()))
		return false;

	// 法线数组比较
	if (!compareArray(geom1->getNormalArray(), geom2->getNormalArray()))
		return false;

	// 颜色数组比较
	if (!compareArray(geom1->getColorArray(), geom2->getColorArray()))
		return false;

	// 纹理坐标数组比较
	for (unsigned int i = 0; i < geom1->getNumTexCoordArrays(); ++i) {
		if (!compareArray(geom1->getTexCoordArray(i), geom2->getTexCoordArray(i)))
			return false;
	}

	// 绑定方式比较
	if (geom1->getNormalBinding() != geom2->getNormalBinding()) return false;
	if (geom1->getColorBinding() != geom2->getColorBinding()) return false;

	// StateSet比较
	if (!compareStateSet(geom1->getStateSet(), geom2->getStateSet()))
		return false;

	// PrimitiveSets比较
	for (size_t i = 0; i < geom1->getNumPrimitiveSets(); ++i)
	{
		if (!comparePrimitiveSet(geom1->getPrimitiveSet(i), geom2->getPrimitiveSet(i)))
			return false;
	}

	return true;
}

bool Utils::compareGeode(const osg::ref_ptr<osg::Geode>& geode1, const osg::ref_ptr<osg::Geode>& geode2)
{
	// 快速路径
	if (geode1.get() == geode2.get()) return true;
	if (!geode1.valid() || !geode2.valid()) return false;

	// 基本属性比较
	if (geode1->getNumDrawables() != geode2->getNumDrawables())
		return false;

	// StateSet比较
	if (!compareStateSet(geode1->getStateSet(), geode2->getStateSet()))
		return false;

	// 比较所有Drawable
	for (size_t i = 0; i < geode1->getNumDrawables(); ++i)
	{
		const osg::Drawable* drawable1 = geode1->getDrawable(i);
		const osg::Drawable* drawable2 = geode2->getDrawable(i);

		// Geometry特殊处理
		const osg::Geometry* geom1 = drawable1->asGeometry();
		const osg::Geometry* geom2 = drawable2->asGeometry();
		if (geom1 && geom2)
		{
			if (!compareGeometry(osg::ref_ptr<osg::Geometry>(const_cast<osg::Geometry*>(geom1)),
				osg::ref_ptr<osg::Geometry>(const_cast<osg::Geometry*>(geom2))))
				return false;
		}
		else if (geom1 || geom2)  // 一个是Geometry另一个不是
		{
			return false;
		}
	}

	return true;
}

bool Utils::compareArray(const osg::Array* array1, const osg::Array* array2)
{
	// 空指针检查
	if (array1 == array2) return true;
	if (!array1 || !array2) return false;

	// 基本属性检查
	if (array1->getType() != array2->getType()) return false;
	if (array1->getDataType() != array2->getDataType()) return false;
	if (array1->getNumElements() != array2->getNumElements()) return false;

	// 根据数组类型进行具体比较
	switch (array1->getType())
	{
	case osg::Array::Vec2ArrayType:
		return compareVecArray<osg::Vec2Array>(
			static_cast<const osg::Vec2Array*>(array1),
			static_cast<const osg::Vec2Array*>(array2));

	case osg::Array::Vec3ArrayType:
		return compareVecArray<osg::Vec3Array>(
			static_cast<const osg::Vec3Array*>(array1),
			static_cast<const osg::Vec3Array*>(array2));

	case osg::Array::Vec4ArrayType:
		return compareVecArray<osg::Vec4Array>(
			static_cast<const osg::Vec4Array*>(array1),
			static_cast<const osg::Vec4Array*>(array2));

	case osg::Array::Vec4ubArrayType:
		return compareVecArray<osg::Vec4ubArray>(
			static_cast<const osg::Vec4ubArray*>(array1),
			static_cast<const osg::Vec4ubArray*>(array2));

	case osg::Array::FloatArrayType:
		return compareVecArray<osg::FloatArray>(
			static_cast<const osg::FloatArray*>(array1),
			static_cast<const osg::FloatArray*>(array2));

	case osg::Array::Vec2bArrayType:
		return compareVecArray<osg::Vec2bArray>(
			static_cast<const osg::Vec2bArray*>(array1),
			static_cast<const osg::Vec2bArray*>(array2));

	case osg::Array::Vec3bArrayType:
		return compareVecArray<osg::Vec3bArray>(
			static_cast<const osg::Vec3bArray*>(array1),
			static_cast<const osg::Vec3bArray*>(array2));

	case osg::Array::IntArrayType:
		return compareVecArray<osg::IntArray>(
			static_cast<const osg::IntArray*>(array1),
			static_cast<const osg::IntArray*>(array2));

	default:
		OSG_WARN << "Unsupported array type in comparison: " << array1->getType() << std::endl;
		return false;
	}
}

bool Utils::compareDrawArrays(const osg::DrawArrays* da1, const osg::DrawArrays* da2)
{
	return da1->getFirst() == da2->getFirst() &&
		da1->getCount() == da2->getCount();
}

bool Utils::compareUniformValue(const osg::Uniform& u1, const osg::Uniform& u2)
{
	switch (u1.getType()) {
	case osg::Uniform::FLOAT:
	{
		float v1, v2;
		u1.get(v1); u2.get(v2);
		return osg::equivalent(v1, v2);
	}
	case osg::Uniform::FLOAT_VEC2:
	{
		osg::Vec2f v1, v2;
		u1.get(v1); u2.get(v2);
		return osg::equivalent(v1.x(), v2.x()) && osg::equivalent(v1.y(), v2.y());
	}
	case osg::Uniform::FLOAT_VEC3:
	{
		osg::Vec3d v1, v2;
		u1.get(v1); u2.get(v2);
		return osg::equivalent(v1.x(), v2.x()) && osg::equivalent(v1.y(), v2.y()) && osg::equivalent(v1.z(), v2.z());
	}
	case osg::Uniform::FLOAT_VEC4:
	{
		osg::Vec4f v1, v2;
		u1.get(v1); u2.get(v2);
		return osg::equivalent(v1.x(), v2.x()) && osg::equivalent(v1.y(), v2.y()) && osg::equivalent(v1.z(), v2.z()) && osg::equivalent(v1.w(), v2.w());
	}
	// ... 其他类型的比较
	default:
		return false;
	}
}

void Utils::initConsole()
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#else
	setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
}

void Utils::registerFileAliases()
{
	osgDB::Registry* instance = osgDB::Registry::instance();
	instance->addFileExtensionAlias("glb", "gltf");
	instance->addFileExtensionAlias("b3dm", "gltf");
	instance->addFileExtensionAlias("i3dm", "gltf");
	instance->addFileExtensionAlias("ktx2", "ktx");
	instance->setReadFileCallback(new Utils::ProgressReportingFileReadCallback);
}

void Utils::CRenderingThread::stop()
{
	show_console_cursor(true);
	_bStop = true;
}

void Utils::CRenderingThread::run()
{
	if (!_bStop)
	{
		int pos = _fin->tellg();
		while (pos < _length && !_bStop && !bar.is_completed())
		{
			pos = _fin->tellg();
			double percent = 100.0 * pos / _length;

			// 当百分比大于等于 1% 时打印
			if (percent >= 1)
			{
				bar.set_progress(percent);  // 设置当前进度
				OpenThreads::Thread::microSleep(100);
			}
		}
		show_console_cursor(true);
	}
}

osgDB::ReaderWriter::ReadResult Utils::ProgressReportingFileReadCallback::readNode(const std::string& file, const osgDB::Options* option)
{
	std::string ext = osgDB::getLowerCaseFileExtension(file);
	osgDB::ReaderWriter::ReadResult rr;
	osgDB::ReaderWriter* rw = osgDB::Registry::instance()->getReaderWriterForExtension(ext);

	if (rw)
	{
		osgDB::ifstream istream(file.c_str(), std::ios::in | std::ios::binary);
		crt = new CRenderingThread(&istream);
		crt->startThread();
		if (istream)
		{

			rr = rw->readNode(istream, option);
			if (rr.status() != osgDB::ReaderWriter::ReadResult::ReadStatus::FILE_LOADED)
			{
				crt->stop();
				crt->cancel();
				rr = osgDB::Registry::instance()->readNodeImplementation(file, option);
			}
			else
			{
				crt->join();
			}
		}
	}
	else
	{
		rr = osgDB::Registry::instance()->readNodeImplementation(file, option);
	}

	return rr;
}

void Utils::TriangleCounterVisitor::apply(osg::Drawable& drawable)
{
	osg::Geometry* geom = drawable.asGeometry();
	if (geom)
	{
		for (unsigned int i = 0; i < geom->getNumPrimitiveSets(); ++i)
		{
			osg::PrimitiveSet* primSet = geom->getPrimitiveSet(i);

			if (osg::DrawArrays* drawArrays = dynamic_cast<osg::DrawArrays*>(primSet))
			{
				count += calculateTriangleCount(drawArrays->getMode(), drawArrays->getCount());
			}
			else if (osg::DrawElements* drawElements = dynamic_cast<osg::DrawElements*>(primSet))
			{
				count += drawElements->getNumIndices() / 3;
			}
		}
	}
}

unsigned int Utils::TriangleCounterVisitor::calculateTriangleCount(const GLenum mode, const unsigned int count)
{
	switch (mode)
	{
	case GL_TRIANGLES:
		return count / 3;
	case GL_TRIANGLE_STRIP:
	case GL_TRIANGLE_FAN:
	case GL_POLYGON:
		return count - 2;
	case GL_QUADS:
		return (count / 4) * 2;
	case GL_QUAD_STRIP:
		return count;
	default:
		return 0;
	}
}

void Utils::TextureCounterVisitor::apply(osg::Drawable& drawable)
{
	osg::ref_ptr<osg::StateSet> stateSet = drawable.getStateSet();
	pushStateSet2UniqueStateSets(stateSet, _stateSets);
}

void Utils::TextureMetricsVisitor::apply(osg::Transform& transform)
{
	osg::Matrix matrix;
	if (transform.computeLocalToWorldMatrix(matrix, this))
	{
		pushMatrix(matrix);
		traverse(transform);
		popMatrix();
	}
}

void Utils::TextureMetricsVisitor::apply(osg::Geode& geode)
{
	for (unsigned int i = 0; i < geode.getNumDrawables(); ++i)
	{
		osg::Vec2 texMin, texMax;
		double totalArea = 0.0f;

		texMin.set(FLT_MAX, FLT_MAX);
		texMax.set(-FLT_MAX, -FLT_MAX);
		osg::Geometry* geometry = geode.getDrawable(i)->asGeometry();
		if (!geometry) continue;
		osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
		if (!stateSet.valid()) continue;
		osg::ref_ptr<osg::Texture2D> osgTexture = dynamic_cast<osg::Texture2D*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
		if (!osgTexture.valid()) continue;
		if (!osgTexture->getNumImages()) continue;

		osg::Image* image = osgTexture->getImage();
		if (!image) continue;

		// 获取顶点数组和纹理坐标
		osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
		osg::Vec2Array* texCoords = dynamic_cast<osg::Vec2Array*>(geometry->getTexCoordArray(0));
		if (!vertices || !texCoords) continue;

		// 更新纹理坐标范围
		for (const auto& texCoord : *texCoords)
		{
			texMin.x() = osg::minimum(texMin.x(), texCoord.x());
			texMin.y() = osg::minimum(texMin.y(), texCoord.y());
			texMax.x() = osg::maximum(texMax.x(), texCoord.x());
			texMax.y() = osg::maximum(texMax.y(), texCoord.y());
		}

		// 计算几何体表面积
		for (unsigned int j = 0; j < geometry->getNumPrimitiveSets(); ++j)
		{
			osg::PrimitiveSet* primSet = geometry->getPrimitiveSet(j);
			if (primSet->getMode() != osg::PrimitiveSet::TRIANGLES) continue;

			for (unsigned int k = 0; k < primSet->getNumIndices(); k += 3)
			{
				osg::Vec3 v1 = (*vertices)[primSet->index(k)] * _currentMatrix;
				osg::Vec3 v2 = (*vertices)[primSet->index(k + 1)] * _currentMatrix;
				osg::Vec3 v3 = (*vertices)[primSet->index(k + 2)] * _currentMatrix;

				osg::Vec3 cross = (v2 - v1) ^ (v3 - v1);
				totalArea += cross.length() * 0.5f; // 三角形面积
			}
		}

		// 计算 UV 面积
		double uvArea = (texMax.x() - texMin.x()) * (texMax.y() - texMin.y());
		if (uvArea <= 0.0) continue; // 如果 UV 面积无效，跳过

		// 计算纹理总像素数
		int s = osg::Image::computeNearestPowerOfTwo(image->s());
		s = s > _resolution ? _resolution : s;
		int t = osg::Image::computeNearestPowerOfTwo(image->t());
		t = t > _resolution ? _resolution : t;
		const int texturePixelCount = s * t;

		// 计算纹素密度
		if (totalArea > 0.0 && uvArea > 0.0)
		{
			double texelDensity = texturePixelCount / (uvArea * totalArea);
			if (texelDensity < _minTexelDensity)
			{
				_minTexelDensity = texelDensity;
				diagonalLength = geometry->getBound().radius();
			}
		}
	}
}

void Utils::TextureMetricsVisitor::pushMatrix(const osg::Matrix& matrix)
{
	_matrixStack.push_back(_currentMatrix);
	_currentMatrix = _currentMatrix * matrix;
}

void Utils::TextureMetricsVisitor::popMatrix()
{
	if (!_matrixStack.empty())
	{
		_currentMatrix = _matrixStack.back();
		_matrixStack.pop_back();
	}
}

void Utils::MaxGeometryVisitor::apply(osg::Transform& transform)
{
	osg::Matrix matrix;
	if (transform.computeLocalToWorldMatrix(matrix, this))
	{
		pushMatrix(matrix);
		traverse(transform);
		popMatrix();
	}
}

void Utils::MaxGeometryVisitor::apply(osg::Geode& geode)
{

	for (unsigned int i = 0; i < geode.getNumDrawables(); ++i)
	{
		osg::BoundingBox boundingBox;

		osg::Geometry* geometry = geode.getDrawable(i)->asGeometry();
		if (!geometry) continue;

		// 获取顶点数组和纹理坐标
		osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());

		// 计算几何体的面积和体积
		for (unsigned int j = 0; j < geometry->getNumPrimitiveSets(); ++j)
		{
			osg::PrimitiveSet* primSet = geometry->getPrimitiveSet(j);
			if (primSet->getMode() != osg::PrimitiveSet::TRIANGLES) continue;

			for (unsigned int k = 0; k < primSet->getNumIndices(); k += 3)
			{
				// 获取变换后的顶点
				osg::Vec3 v1 = (*vertices)[primSet->index(k)] * _currentMatrix;
				osg::Vec3 v2 = (*vertices)[primSet->index(k + 1)] * _currentMatrix;
				osg::Vec3 v3 = (*vertices)[primSet->index(k + 2)] * _currentMatrix;


				// 更新总体包围盒
				boundingBox.expandBy(v1);
				boundingBox.expandBy(v2);
				boundingBox.expandBy(v3);
			}
		}
		if (!maxBB.valid())
		{
			maxBB = boundingBox;
			geom = geometry;
		}
		else
		{
			if ((boundingBox._max - boundingBox._min).length() > (maxBB._max - maxBB._min).length())
			{
				maxBB = boundingBox;
				geom = geometry;
			}
		}
	}

}

void Utils::MaxGeometryVisitor::pushMatrix(const osg::Matrix& matrix)
{
	_matrixStack.push_back(_currentMatrix);
	_currentMatrix = _currentMatrix * matrix;
}

void Utils::MaxGeometryVisitor::popMatrix()
{
	if (!_matrixStack.empty())
	{
		_currentMatrix = _matrixStack.back();
		_matrixStack.pop_back();
	}
}

void Utils::DrawcallCommandCounterVisitor::apply(osg::Transform& transform)
{
	osg::Matrix matrix;
	if (transform.computeLocalToWorldMatrix(matrix, this))
	{
		pushMatrix(matrix);
		traverse(transform);
		popMatrix();
	}
}

void Utils::DrawcallCommandCounterVisitor::apply(osg::Drawable& drawable)
{
	_matrixGeometryMap[_currentMatrix].push_back(drawable.asGeometry());
}

unsigned int Utils::DrawcallCommandCounterVisitor::getCount() const
{
	unsigned int count = 0;
	for (auto it= _matrixGeometryMap.begin();it!= _matrixGeometryMap.end();++it)
	{
		if (it->second.size() == 1)
			count++;
		else
		{
			std::vector<osg::ref_ptr<osg::StateSet>> stateSets;
			std::vector<osg::ref_ptr<osg::StateSet>> repeatStateSets;

			for (size_t i = 0; i < it->second.size(); i++)
			{
				osg::ref_ptr<osg::Geometry> geometry = it->second.at(i);
				osg::ref_ptr<osg::StateSet> stateSet = geometry->getStateSet();
				osg::ref_ptr<osg::Vec2Array> texCoords = dynamic_cast<osg::Vec2Array*>(geometry->getTexCoordArray(0));
				if (texCoords.valid())
				{
					if(isRepeatTexCoords(texCoords))
						pushStateSet2UniqueStateSets(stateSet, repeatStateSets);
					else
						pushStateSet2UniqueStateSets(stateSet, stateSets);
				}
				else
					pushStateSet2UniqueStateSets(stateSet, stateSets);
			}

			count += repeatStateSets.size() + stateSets.size();
		}
	}

	return count;
}

void Utils::DrawcallCommandCounterVisitor::pushMatrix(const osg::Matrix& matrix)
{
	_matrixStack.push_back(_currentMatrix);
	_currentMatrix = _currentMatrix * matrix;
}

void Utils::DrawcallCommandCounterVisitor::popMatrix()
{
	if (!_matrixStack.empty())
	{
		_currentMatrix = _matrixStack.back();
		_matrixStack.pop_back();
	}
}

// int 特化
template<>
bool Utils::compareVec<osg::IntArray>(const int& v1, const int& v2)
{
	return v1 == v2;
}

// float 特化
template<>
bool Utils::compareVec<osg::FloatArray>(const float& v1, const float& v2)
{
	return osg::equivalent(v1, v2);
}

// Vec2f 特化
template<>
bool Utils::compareVec<osg::Vec2Array>(const osg::Vec2f& v1, const osg::Vec2f& v2)
{
	return osg::equivalent(v1.x(), v2.x()) &&
		osg::equivalent(v1.y(), v2.y());
}

// Vec2b 特化
template<>
bool Utils::compareVec<osg::Vec2bArray>(const osg::Vec2b& v1, const osg::Vec2b& v2)
{
	return v1 == v2;
}

// Vec3b 特化
template<>
bool Utils::compareVec<osg::Vec3bArray>(const osg::Vec3b& v1, const osg::Vec3b& v2)
{
	return v1 == v2;
}

// Vec3f 特化
template<>
bool Utils::compareVec<osg::Vec3Array>(const osg::Vec3f& v1, const osg::Vec3f& v2)
{
	return osg::equivalent(v1.x(), v2.x()) &&
		osg::equivalent(v1.y(), v2.y()) &&
		osg::equivalent(v1.z(), v2.z());
}

// Vec4f 特化
template<>
bool Utils::compareVec<osg::Vec4Array>(const osg::Vec4f& v1, const osg::Vec4f& v2)
{
	return osg::equivalent(v1.x(), v2.x()) &&
		osg::equivalent(v1.y(), v2.y()) &&
		osg::equivalent(v1.z(), v2.z()) &&
		osg::equivalent(v1.w(), v2.w());
}

// Vec4ub 特化
template<>
bool Utils::compareVec<osg::Vec4ubArray>(const osg::Vec4ub& v1, const osg::Vec4ub& v2)
{
	return v1 == v2;
}
