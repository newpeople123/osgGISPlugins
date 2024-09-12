#include "3dtiles/hlod/TreeBuilder.h"
#include <osg/Group>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osgViewer/Viewer>
#include <osgdb_gltf/material/GltfPbrMRMaterial.h>
#include <osgdb_gltf/material/GltfPbrSGMaterial.h>
#include <osgDB/WriteFile>
using namespace osgGISPlugins;
osg::ref_ptr<Tile> TreeBuilder::build()
{
	osg::ref_ptr<osg::Group> node = new osg::Group;
	double min = FLT_MAX;
	for (auto& item : _groupsToDivideList)
	{
		osg::BoundingBox bb;
		bb.expandBy(item->getBound());
		const double xLength = bb.xMax() - bb.xMin();
		const double yLength = bb.yMax() - bb.yMin();
		const double zLength = bb.zMax() - bb.zMin();
		min = osg::minimum(osg::minimum(osg::minimum(min, xLength), yLength), zLength);
		node->addChild(item);
	}
	for (auto& item : _geodesToDivideList)
	{
		osg::BoundingBox bb;
		bb.expandBy(item->getBound());
		const double xLength = bb.xMax() - bb.xMin();
		const double yLength = bb.yMax() - bb.yMin();
		const double zLength = bb.zMax() - bb.zMin();
		min = osg::minimum(osg::minimum(osg::minimum(min, xLength), yLength), zLength);
		node->addChild(item);
	}

	osg::BoundingBox rootBox;
	rootBox.expandBy(node->getBound());
	const double xLength = rootBox.xMax() - rootBox.xMin();
	const double yLength = rootBox.yMax() - rootBox.yMin();
	const double zLength = rootBox.zMax() - rootBox.zMin();
	const double max = osg::maximum(osg::maximum(xLength, yLength), zLength);
	_maxLevel = std::log2(max / min);
	OSG_NOTICE << test.size() << " " << _groupsToDivideList.size() << " " << _geodesToDivideList.size() << std::endl;
	std::map<osg::Geode*, std::vector<osg::Group*>> test2;
	int index = 0;
	for (auto item = test.begin(); item != test.end(); ++item)
	{
		if (item->second.size() == 33)
		{
			OSG_NOTICE << item->second.size() << std::endl;
			test2.insert(std::make_pair(item->first,item->second));
			osg::ref_ptr<osg::Group> group = new osg::Group;
			for (size_t i = 0; i < item->second.size(); ++i)
			{
				group->addChild(item->second.at(i));
			}
			
			osgDB::writeNodeFile(*group.get(), R"(D:\nginx-1.22.1\html\3dtiles\tet5\test)" + std::to_string(index) + ".i3dm");
			osgDB::writeNodeFile(*group.get(), R"(D:\nginx-1.22.1\html\3dtiles\tet4\test)" + std::to_string(index) + ".b3dm");
			++index;
		}
	}

	return divide(node, rootBox);
}

bool TreeBuilder::compareStateSet(osg::ref_ptr<osg::StateSet> stateSet1, osg::ref_ptr<osg::StateSet> stateSet2)
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

bool TreeBuilder::comparePrimitiveSet(osg::ref_ptr<osg::PrimitiveSet> pSet1, osg::ref_ptr<osg::PrimitiveSet> pSet2)
{
	if (pSet1.get() == pSet2.get())
		return true;

	if (pSet1->getMode() != pSet2->getMode())
		return false;
	if (pSet1->getType() != pSet2->getType())
		return false;
	return true;
}

bool TreeBuilder::compareGeometry(osg::ref_ptr<osg::Geometry> geom1, osg::ref_ptr<osg::Geometry> geom2)
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
		if ((positions1->at(i) - positions2->at(i)).length() > 0.01)
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

bool TreeBuilder::compareGeode(osg::Geode& geode1, osg::Geode& geode2)
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

void TreeBuilder::apply(osg::Group& group)
{
	// 备份当前矩阵
	osg::Matrix previousMatrix = _currentMatrix;

	// 如果是Transform节点，累积变换矩阵
	if (osg::Transform* transform = group.asTransform())
	{
		osg::Matrix localMatrix;
		transform->computeLocalToWorldMatrix(localMatrix, this);
		_currentMatrix.preMult(localMatrix);
	}

	// 继续遍历子节点
	traverse(group);

	// 恢复到之前的矩阵状态
	_currentMatrix = previousMatrix;
}

void TreeBuilder::apply(osg::Geode& geode)
{
	if (_currentMatrix == osg::Matrix::identity())
	{
		_geodesToDivideList.insert(&geode);
	}
	else
	{
		osg::Group* transform = new osg::MatrixTransform(_currentMatrix);
		transform->addChild(&geode);
		_groupsToDivideList.insert(transform);

		bool bAdd = true;
		for (auto item = test.begin(); item != test.end(); ++item)
		{
			osg::Geode* geodeItem = item->first;
			if (compareGeode(*geodeItem, geode))
			{
				bAdd = false;
				item->second.push_back(transform);
				break;
			}
		}
		if (bAdd)
		{
			test[&geode].push_back(transform);
		}
	}

}
