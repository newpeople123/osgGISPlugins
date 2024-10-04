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
