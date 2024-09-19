#include "3dtiles/hlod/TreeBuilder.h"
#include "3dtiles/Tileset.h"
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
#include <osg/CoordinateSystemNode>
#include <utils/Simplifier.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <osgdb_gltf/b3dm/BatchIdVisitor.h>
using namespace osgGISPlugins;

void Tile::buildBaseHlodAndComputeGeometricError()
{
	if (this->node.valid())
	{
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
		const double currentRadius = computeRadius(boundingBox, this->axis) * 0.7;

		for (size_t i = 0; i < group->getNumChildren(); ++i)
		{
			osg::ref_ptr<osg::Node> childNode = group->getChild(i);
			osg::ComputeBoundsVisitor childCbv;
			childNode->accept(childCbv);
			const osg::BoundingBox childBoundingBox = childCbv.getBoundingBox();
			const double childRadius = computeRadius(childBoundingBox, this->axis);

			if (childRadius >= currentRadius)
			{
				//osg::ref_ptr<osg::Node> childNodeCopy = osg::clone(childNode.get(), osg::CopyOp::DEEP_COPY_ALL);
				//parentGroup->addChild(childNodeCopy);
				parentGroup->addChild(childNode);
			}
			else {
				const double childNodeGeometricError = childRadius * CesiumGeometricErrorOperator;
				this->parent->geometricError = osg::maximum(this->parent->geometricError, childNodeGeometricError);
				if (this->parent->geometricError < this->geometricError)
					this->parent->geometricError = this->geometricError + 0.0001;
			}
		}
	}
	else
	{
		unsigned int sumNumChildren = 0;
		for (size_t i = 0; i < children.size(); ++i)
		{
			this->children[i]->buildHlod();
			sumNumChildren += this->children[i]->node->asGroup()->getNumChildren();
		}
		if (this->node.valid())
		{
			const unsigned int currentNodeNumChildren = this->node->asGroup()->getNumChildren();
			if (sumNumChildren == currentNodeNumChildren || currentNodeNumChildren == 0)
			{
				for (size_t i = 0; i < children.size(); ++i)
				{
					this->geometricError = osg::maximum(this->geometricError, this->children[i]->geometricError);
				}
				this->geometricError += 0.0001;
			}
		}

		buildBaseHlodAndComputeGeometricError();
	}
}

void Tile::rebuildHlodByRefinement()
{
	if (this->node.valid())
	{

		osg::ref_ptr<osg::Group> group = new osg::Group;
		for (size_t i = 0; i < this->children.size(); ++i)
		{
			if (this->children[i]->node.valid())
			{
				osg::ref_ptr<osg::Group> currentNodeAsGroup = this->node->asGroup();
				osg::ref_ptr<osg::Group> childNodeAsGroup = this->children[i]->node->asGroup();
				for (size_t k = 0; k < childNodeAsGroup->getNumChildren(); ++k)
				{
					osg::ref_ptr<osg::Node> childNode = childNodeAsGroup->getChild(k);
					bool bFindNode = false;
					for (size_t j = 0; j < currentNodeAsGroup->getNumChildren(); ++j)
					{
						if (currentNodeAsGroup->getChild(j) == childNode)
						{
							bFindNode = true;
							break;
						}
					}
					if (!bFindNode)
						group->addChild(childNode);
				}
			}
		}

		TriangleCountVisitor tcv;
		this->node->accept(tcv);
		TriangleCountVisitor childTcv;
		group->accept(childTcv);
		if (childTcv.count * 2 < tcv.count)
		{
			osg::ref_ptr<osg::Group> currentNodeAsGroup = this->node->asGroup();
			for (size_t i = 0; i < this->children.size(); ++i)
			{
				if (this->children[i]->node.valid())
				{
					for (size_t j = 0; j < currentNodeAsGroup->getNumChildren(); ++j)
					{
						osg::ref_ptr<osg::Node> node = currentNodeAsGroup->getChild(j);
						osg::ref_ptr<osg::Group> childNodeAsGroup = this->children[i]->node->asGroup();
						for (size_t k = 0; k < childNodeAsGroup->getNumChildren(); ++k)
						{
							childNodeAsGroup->removeChild(node);
						}
					}
				}
			}
			this->refine = Refinement::ADD;
		}

		if (descendantNodeIsEmpty())
		{
			this->children.clear();
			this->geometricError = 0.0;
		}
	}

	for (size_t i = 0; i < this->children.size(); ++i)
	{

		if (this->children[i]->node->asGroup()->getNumChildren() == 0)
		{
			if (this->children[i]->children.size() == 0)
			{
				this->children.erase(this->children.begin() + i);
				i--;
				continue;
			}
		}

		this->children[i]->rebuildHlodByRefinement();
	}
}

bool Tile::descendantNodeIsEmpty()
{
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		if (this->children[i]->node->asGroup()->getNumChildren() > 0)
			return false;
		if (!this->children[i]->descendantNodeIsEmpty())
			return false;
	}
	return true;
}

osg::ref_ptr<osg::Group> Tile::getAllDescendantNodes()
{
	osg::ref_ptr<osg::Group> group = new osg::Group;
	osg::ref_ptr<osg::Group> currentNodeAsGroup = this->node->asGroup();
	for (size_t i = 0; i < currentNodeAsGroup->getNumChildren(); ++i)
	{
		group->addChild(currentNodeAsGroup->getChild(i));
	}
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		osg::ref_ptr<osg::Group> childGroup = this->children[i]->getAllDescendantNodes();
		for (size_t j = 0; j < childGroup->getNumChildren(); ++j)
		{
			group->addChild(childGroup->getChild(j));
		}
	}
	return group;
}

void Tile::buildHlod()
{
	buildBaseHlodAndComputeGeometricError();
	rebuildHlodByRefinement();
}

void Tile::computeGeometricError()
{

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
			{
				isValid = false;
				break;
			}
			if (!this->children[i]->geometricErrorValid())
			{
				isValid = false;
				break;
			}
		}
		return isValid;
	}
	else
		return this->geometricError == 0.0;
}

void Tile::write(const string& str, const float simplifyRatio, GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options)
{
	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		osg::ref_ptr<osg::Node> nodeCopy = osg::clone(node.get(), osg::CopyOp::DEEP_COPY_ALL);

		if (simplifyRatio < 1.0) {
			if (this->refine == Refinement::REPLACE)
			{
				Simplifier simplifier(std::pow(simplifyRatio, getMaxLevel() - level + 1), true);
				nodeCopy->accept(simplifier);
			}
			else
			{
				Simplifier simplifier(simplifyRatio);
				nodeCopy->accept(simplifier);
			}
		}
		GltfOptimizer gltfOptimzier;
		if (gltfTextureOptions.cachePath.empty())
			gltfTextureOptions.cachePath = str + "\\textures";
		gltfOptimzier.setGltfTextureOptimizationOptions(gltfTextureOptions);
		osgDB::makeDirectory(gltfTextureOptions.cachePath);
		gltfOptimzier.optimize(nodeCopy.get(), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);

		BatchIdVisitor biv;
		nodeCopy->accept(biv);

		const string path = str + "\\" + to_string(level);
		osgDB::makeDirectory(path);
		osgDB::writeNodeFile(*nodeCopy.get(), path + "\\" + "Tile_" + to_string(level) + "_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + ".b3dm", options);
		//osgDB::writeNodeFile(*nodeCopy.get(), path + "\\" + "Tile_" + to_string(level) + "_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + ".gltf", options);

	}
	/* single thread */
	//for (size_t i = 0; i < this->children.size(); ++i)
	//{
	//	this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
	//}

	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r) {
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
			}
		});

}

double Tile::computeRadius(const osg::BoundingBox& bbox, int axis)
{
	switch (axis) {
	case 0: // X-Y轴分割
		return (osg::Vec2(bbox._max.x(), bbox._max.y()) - osg::Vec2(bbox._min.x(), bbox._min.y())).length() / 2;
	case 1: // Y-Z轴分割
		return (osg::Vec2(bbox._max.z(), bbox._max.y()) - osg::Vec2(bbox._min.z(), bbox._min.y())).length() / 2;
	case 2: // Z-X轴分割
		return (osg::Vec2(bbox._max.z(), bbox._max.x()) - osg::Vec2(bbox._min.z(), bbox._min.x())).length() / 2;
	default:
		return 0.0;
	}
}

void Tileset::computeTransform(const double lng, const double lat, const double h)
{
	const osg::EllipsoidModel ellipsoidModel;
	osg::Matrixd matrix;
	ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(lat), osg::DegreesToRadians(lng), h, matrix);


	const double* ptr = matrix.ptr();
	for (unsigned i = 0; i < 16; ++i)
		this->root->transform.push_back(*ptr++);
}

void Tileset::computeGeometricError(osg::ref_ptr<osg::Node> node)
{
	osg::ComputeBoundsVisitor cbv;
	node->accept(cbv);
	const osg::BoundingBox bb = cbv.getBoundingBox();
	const double radius = bb.radius();
	this->geometricError = radius * CesiumGeometricErrorOperator;
}

bool Tileset::valid()
{
	if (!this->root) return false;
	return this->geometricError > this->root->geometricError && this->root->geometricErrorValid();
}
