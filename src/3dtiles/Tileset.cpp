#include "3dtiles/Tileset.h"
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
using namespace osgGISPlugins;

void Tile::buildHlod()
{
	if (this->node.valid())
	{
		computeGeometricError();

		if (!this->parent.valid()) return;
		if (!this->parent->node.valid())
		{
			this->parent->node = new osg::Group;
		}
		osg::ref_ptr<osg::Group> parentGroup = this->parent->node->asGroup();

		osg::ref_ptr<osg::Group> group = this->node->asGroup();
		osg::ComputeBoundsVisitor cbv;
		this->node->accept(cbv);
		const osg::BoundingBox boundingBox = cbv.getBoundingBox();
		const float radius = boundingBox.radius() * 0.7f;
		for (size_t i = 0; i < group->getNumChildren(); ++i)
		{
			osg::ref_ptr<osg::Node> childNode = group->getChild(i);
			osg::ComputeBoundsVisitor childCbv;
			childNode->accept(childCbv);
			const osg::BoundingBox childBoundingBox = childCbv.getBoundingBox();
			const float childRadius = childBoundingBox.radius();

			if (childRadius >= radius)
			{
				osg::ref_ptr<osg::Node> childNodeCopy = osg::clone(childNode.get(), osg::CopyOp::DEEP_COPY_ALL);
				parentGroup->addChild(childNodeCopy);
			}
		}
	}
	else
	{
		for (size_t i = 0; i < children.size(); ++i)
		{
			this->children[i]->buildHlod();
		}
		buildHlod();
	}
}

void Tile::computeGeometricError()
{
	const size_t size = this->children.size();
	if (size)
	{
		TraingleAreaVisitor tav;
		this->node->accept(tav);
		const double currentArea = tav.area;

		double test = 0.0;

		double childArea = 0.0;
		for (size_t i = 0; i < size; ++i)
		{
			TraingleAreaVisitor tav1;
			this->children[i]->node->accept(tav1);
			childArea += tav1.area;

			osg::ComputeBoundsVisitor cbv1;
			this->children[i]->node->accept(cbv1);
			const double radius1 = cbv1.getBoundingBox().radius();
			const double temp1 = radius1 * CesiumGeometricErrorOperator;
			test += temp1;
		}

		osg::ComputeBoundsVisitor cbv2;
		this->node->accept(cbv2);
		const double radius2 = cbv2.getBoundingBox().radius();
		const double temp2 = radius2 * CesiumGeometricErrorOperator;
		const double change = test - temp2;

		this->geometricError = childArea - currentArea;
	}
}

bool Tile::geometricErrorValid()
{
	const size_t size = this->children.size();
	if (size)
	{
		bool isValid = true;
		for (size_t i = 0; i < size; ++i)
		{
			if (this->geometricError <= this->children[i]->geometricError)
				isValid = false;
		}
		return isValid;
	}
	else
		return this->geometricError == 0.0;
}

void osgGISPlugins::Tile::write(const string& str)
{
	const string path = str + "\\" + to_string(level);
	osgDB::makeDirectory(path);
	osgDB::writeNodeFile(*this->node.get(), path + "\\" + "Tile" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + ".b3dm");
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		this->children[i]->write(str);
	}
}

void Tileset::computeGeometricError(osg::ref_ptr<osg::Node> node)
{
	osg::ComputeBoundsVisitor cbv;
	node->accept(cbv);
	const double radius = cbv.getBoundingBox().radius();
	this->geometricError = radius * CesiumGeometricErrorOperator;
}

bool Tileset::geometricErrorValid()
{
	if (!this->root) return false;
	return this->geometricError > this->root->geometricError && this->root->geometricErrorValid();
}
