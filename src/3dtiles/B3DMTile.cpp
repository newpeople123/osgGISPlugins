#include "3dtiles/B3DMTile.h"
#include "3dtiles/hlod/TreeBuilder.h"
#include "utils/Simplifier.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/FileUtils>
void B3DMTile::computeGeometricError()
{
	if (!this->children.size()) return;
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		this->children[i]->computeGeometricError();
	}

	double total = 0.0;
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		if (this->children[i]->node.valid() && this->children[i]->node->asGroup()->getNumChildren())
		{
			osg::ComputeBoundsVisitor cbv;
			this->children[i]->node->accept(cbv);
			const osg::BoundingBox boundingBox = cbv.getBoundingBox();
			const double childNodeGeometricError = (boundingBox._max - boundingBox._min).length() * 0.7 * 0.5 * CesiumGeometricErrorOperator;
			this->geometricError += pow(childNodeGeometricError, 2);
			total += childNodeGeometricError;
		}
	}
	if (total > 0.0)
		this->geometricError /= total;
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		if (this->children[i]->geometricError >= this->geometricError)
			this->geometricError = this->children[i]->geometricError + 0.1;
	}
}

void B3DMTile::buildBaseHlodAndComputeGeometricError()
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
				parentGroup->addChild(childNode);
			}
		}
	}
	else
	{
		for (size_t i = 0; i < children.size(); ++i)
		{
			osg::ref_ptr<B3DMTile> b3dmTile = dynamic_cast<B3DMTile*>(this->children[i].get());
			if (b3dmTile.valid())
				b3dmTile->buildHlod();
		}

		buildBaseHlodAndComputeGeometricError();
	}
}

void B3DMTile::rebuildHlodAndComputeGeometricErrorByRefinement()
{
	if (this->node.valid())
	{

		osg::ref_ptr<osg::Group> group = new osg::Group;
		for (size_t i = 0; i < this->children.size(); ++i)
		{
			if (this->children[i]->type == "b3dm" && this->children[i]->node.valid())
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
		osg::ref_ptr<B3DMTile> b3dmTile = dynamic_cast<B3DMTile*>(this->children[i].get());
		if (b3dmTile.valid())
			b3dmTile->rebuildHlodAndComputeGeometricErrorByRefinement();
	}
}

void B3DMTile::buildHlod()
{
	buildBaseHlodAndComputeGeometricError();
	rebuildHlodAndComputeGeometricErrorByRefinement();
	computeGeometricError();
}

void B3DMTile::write(const string& str, const float simplifyRatio, const GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options)
{
	computeBoundingVolumeBox();
	setContentUri();

	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		osg::ref_ptr<osg::Node> nodeCopy = osg::clone(node.get(), osg::CopyOp::DEEP_COPY_ALL);

		if (simplifyRatio < 1.0) {
			if (this->refine == Refinement::REPLACE)
			{
				Simplifier simplifier(pow(simplifyRatio, getMaxLevel() - level + 1), true);
				nodeCopy->accept(simplifier);
			}
		}
		else
		{
			Simplifier simplifier(simplifyRatio);
			nodeCopy->accept(simplifier);
		}
		GltfOptimizer gltfOptimzier;
		gltfOptimzier.setGltfTextureOptimizationOptions(gltfTextureOptions);
		gltfOptimzier.optimize(nodeCopy.get(), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);

		BatchIdVisitor biv;
		nodeCopy->accept(biv);

		const string path = str + "\\" + to_string(level);
		osgDB::makeDirectory(path);
		osgDB::writeNodeFile(*nodeCopy.get(), path + "\\" + "Tile_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + "." + type, options);

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

double B3DMTile::computeRadius(const osg::BoundingBox& bbox, int axis)
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
