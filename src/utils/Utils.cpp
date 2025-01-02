#include "utils/Utils.h"

using namespace osgGISPlugins;

bool Utils::compareMatrix(const osg::Matrixd& lhs, const osg::Matrixd& rhs)
{
	const double* ptr1 = lhs.ptr();
	const double* ptr2 = rhs.ptr();

	for (size_t i = 0; i < 16; ++i) {
		if (osg::absolute((*ptr1++) - (*ptr2++)) > EPSILON) return false;
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

bool Utils::compareStateSet(osg::ref_ptr<osg::StateSet> stateSet1, osg::ref_ptr<osg::StateSet> stateSet2)
{
	if (stateSet1.get() == stateSet2.get())
		return true;

	if (!stateSet1.valid() || !stateSet2.valid())
		return false;

	if (stateSet1->getAttributeList().size() != stateSet2->getAttributeList().size())
		return false;

	if (stateSet1->getMode(GL_CULL_FACE) != stateSet2->getMode(GL_CULL_FACE))
		return false;
	if (stateSet1->getMode(GL_BLEND) != stateSet2->getMode(GL_BLEND))
		return false;

	const osg::ref_ptr<osg::Material> osgMaterial1 = dynamic_cast<osg::Material*>(stateSet1->getAttribute(osg::StateAttribute::MATERIAL));
	const osg::ref_ptr<osg::Material> osgMaterial2 = dynamic_cast<osg::Material*>(stateSet1->getAttribute(osg::StateAttribute::MATERIAL));
	if (osgMaterial1.valid() && osgMaterial2.valid())
	{
		const std::type_info& materialId1 = typeid(*osgMaterial1.get());
		const std::type_info& materialId2 = typeid(*osgMaterial2.get());
		if (materialId1 != materialId2)
			return false;
		osg::ref_ptr<GltfPbrMRMaterial> gltfPbrMRMaterial1 = dynamic_cast<GltfPbrMRMaterial*>(osgMaterial1.get());
		osg::ref_ptr<GltfPbrMRMaterial> gltfPbrMRMaterial2 = dynamic_cast<GltfPbrMRMaterial*>(osgMaterial2.get());
		if (gltfPbrMRMaterial1.valid())
		{
			if (gltfPbrMRMaterial1 != gltfPbrMRMaterial2)
				return false;
		}
		else
		{
			osg::ref_ptr<GltfPbrSGMaterial> gltfPbrSGMaterial1 = dynamic_cast<GltfPbrSGMaterial*>(osgMaterial1.get());
			osg::ref_ptr<GltfPbrSGMaterial> gltfPbrSGMaterial2 = dynamic_cast<GltfPbrSGMaterial*>(osgMaterial2.get());
			if (gltfPbrSGMaterial1 != gltfPbrSGMaterial2)
				return false;
		}
	}
	else if (osgMaterial1 != osgMaterial2)
		return false;
	osg::ref_ptr<osg::Texture2D> osgTexture1 = dynamic_cast<osg::Texture2D*>(stateSet1->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
	osg::ref_ptr<osg::Texture2D> osgTexture2 = dynamic_cast<osg::Texture2D*>(stateSet2->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
	if (!GltfMaterial::compareTexture2D(osgTexture1, osgTexture2))
		return false;

	return true;

}

bool Utils::comparePrimitiveSet(osg::ref_ptr<osg::PrimitiveSet> pSet1, osg::ref_ptr<osg::PrimitiveSet> pSet2)
{
	if (pSet1.get() == pSet2.get())
		return true;

	if (pSet1->getMode() != pSet2->getMode())
		return false;
	if (pSet1->getType() != pSet2->getType())
		return false;
	return true;
}

bool Utils::compareGeometry(osg::ref_ptr<osg::Geometry> geom1, osg::ref_ptr<osg::Geometry> geom2)
{
	if (geom1.get() == geom2.get())
		return true;

	if (!(geom1.valid() == geom2.valid() && geom1.valid()))
		return false;

	//compare positions
	osg::ref_ptr<osg::Vec3Array> positions1 = dynamic_cast<osg::Vec3Array*>(geom1->getVertexArray());
	osg::ref_ptr<osg::Vec3Array> positions2 = dynamic_cast<osg::Vec3Array*>(geom2->getVertexArray());
	if (!(positions1.valid() == positions2.valid() && positions1.valid()))
		return false;
	if (positions1->size() != positions2->size())
		return false;
	for (size_t i = 0; i < positions1->size(); ++i)
	{
		if ((positions1->at(i) - positions2->at(i)).length() > EPSILON)
			return false;
	}


	//compare texCoords
	osg::ref_ptr<osg::Array> texCoords1 = geom1->getTexCoordArray(0);
	osg::ref_ptr<osg::Array> texCoords2 = geom2->getTexCoordArray(0);
	if (!(texCoords1.valid() == texCoords2.valid()))
		return false;
	if (texCoords1.valid())
	{
		if (texCoords1->getElementSize() != texCoords2->getElementSize())
			return false;
	}

	//compare stateset
	osg::ref_ptr<osg::StateSet> stateSet1 = geom1->getStateSet();
	osg::ref_ptr<osg::StateSet> stateSet2 = geom2->getStateSet();
	if (!compareStateSet(stateSet1, stateSet2))
		return false;

	//compare colors
	osg::ref_ptr<osg::Array> colors1 = geom1->getColorArray();
	osg::ref_ptr<osg::Array> colors2 = geom2->getColorArray();
	if (!(colors1.valid() == colors2.valid()))
		return false;
	if (colors1.valid())
	{
		if (colors1->getElementSize() != colors2->getElementSize())
			return false;
	}

	//compare primitiveset
	if (geom1->getNumPrimitiveSets() != geom2->getNumPrimitiveSets())
		return false;
	for (size_t i = 0; i < geom1->getNumPrimitiveSets(); ++i)
	{
		if (!comparePrimitiveSet(geom1->getPrimitiveSet(i), geom2->getPrimitiveSet(i)))
			return false;
	}

	return true;
}

bool Utils::compareGeode(osg::Geode& geode1, osg::Geode& geode2)
{
	if (&geode1 == &geode2)
		return true;

	if (geode1.getNumDrawables() != geode2.getNumDrawables())
		return false;

	for (size_t i = 0; i < geode1.getNumDrawables(); ++i)
	{
		osg::ref_ptr<osg::Geometry> geom1 = geode1.getDrawable(i)->asGeometry();
		osg::ref_ptr<osg::Geometry> geom2 = geode2.getDrawable(i)->asGeometry();
		if (!compareGeometry(geom1, geom2))
			return false;
	}

	return true;
}

void CRenderingThread::stop()
{
	show_console_cursor(true);
	_bStop = true;
}

void CRenderingThread::run()
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

osgDB::ReaderWriter::ReadResult ProgressReportingFileReadCallback::readNode(const std::string& file, const osgDB::Options* option)
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

void TriangleCountVisitor::apply(osg::Drawable& drawable)
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

unsigned int TriangleCountVisitor::calculateTriangleCount(const GLenum mode, const unsigned int count)
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

void TextureCountVisitor::apply(osg::Drawable& drawable)
{
	osg::StateSet* stateSet = drawable.getStateSet();
	if (stateSet)
	{
		countTextures(stateSet);
	}
}

void TextureCountVisitor::countTextures(osg::StateSet* stateSet)
{
	for (unsigned int i = 0; i < stateSet->getTextureAttributeList().size(); ++i)
	{
		const osg::Texture* osgTexture = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
		if (osgTexture)
		{
			const osg::Image* img = osgTexture->getImage(0);
			const std::string name = img->getFileName();
			if (_names.find(name) == _names.end())
			{
				_names.insert(name);
				++count;
			}
		}
	}
}

void TextureMetricsVisitor::apply(osg::Transform& transform)
{
	osg::Matrix matrix;
	if (transform.computeLocalToWorldMatrix(matrix, this))
	{
		pushMatrix(matrix);
		traverse(transform);
		popMatrix();
	}
}

void TextureMetricsVisitor::apply(osg::Geode& geode)
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

void TextureMetricsVisitor::pushMatrix(const osg::Matrix& matrix)
{
	_matrixStack.push_back(_currentMatrix);
	_currentMatrix = _currentMatrix * matrix;
}

void TextureMetricsVisitor::popMatrix()
{
	if (!_matrixStack.empty())
	{
		_currentMatrix = _matrixStack.back();
		_matrixStack.pop_back();
	}
}

void MaxGeometryVisitor::apply(osg::Transform& transform)
{
	osg::Matrix matrix;
	if (transform.computeLocalToWorldMatrix(matrix, this))
	{
		pushMatrix(matrix);
		traverse(transform);
		popMatrix();
	}
}

void MaxGeometryVisitor::apply(osg::Geode& geode)
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

void MaxGeometryVisitor::pushMatrix(const osg::Matrix& matrix)
{
	_matrixStack.push_back(_currentMatrix);
	_currentMatrix = _currentMatrix * matrix;
}

void MaxGeometryVisitor::popMatrix()
{
	if (!_matrixStack.empty())
	{
		_currentMatrix = _matrixStack.back();
		_matrixStack.pop_back();
	}
}
