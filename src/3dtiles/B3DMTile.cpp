#include "3dtiles/B3DMTile.h"
#include "3dtiles/hlod/TreeBuilder.h"
#include "utils/Simplifier.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/spin_mutex.h>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/FileUtils>
#include <unordered_map>
#include <limits>

void B3DMTile::computeGeometricError()
{
	if (!this->children.size())
		return;
	// 并行计算所有子瓦片的几何误差
	tbb::parallel_for(size_t(0), this->children.size(), [&](size_t i) {
		this->children[i]->computeGeometricError();
		});

	// 使用预缓存值计算加权误差
	double totalWeightedError = 0.0;
	double totalVolume = 0.0;

	for (const auto& child : this->children) {
		if (child->node.valid())
		{
			auto b3dmTile = dynamic_cast<B3DMTile*>(child.get());
			if (b3dmTile) {
				double range = std::max(b3dmTile->diagonalLength, 2 * b3dmTile->maxClusterDiagonalLength);
				double geometricError = range * 0.5 * CesiumGeometricErrorOperator;

				double volume = b3dmTile->diagonalLength > 2 * b3dmTile->maxClusterDiagonalLength
					? b3dmTile->maxClusterVolume
					: b3dmTile->volume;

				totalWeightedError += geometricError * volume;
				totalVolume += volume;
			}
			else
			{
				osg::ref_ptr<osg::Group> group = child->node->asGroup();
				if (group->getNumChildren())
				{
					osg::ComputeBoundsVisitor cbv;
					group->getChild(0)->accept(cbv);
					const osg::BoundingBox boundingBox = cbv.getBoundingBox();
					const float i3dmGeometricError = (boundingBox._max - boundingBox._min).length() * 0.7 * CesiumGeometricErrorOperator;
					const float i3dmVolume = (boundingBox._max.x() - boundingBox._min.x()) *
						(boundingBox._max.y() - boundingBox._min.y()) *
						(boundingBox._max.z() - boundingBox._min.z());
					totalWeightedError += i3dmGeometricError * i3dmVolume;
					totalVolume += i3dmVolume;
				}
			}
		}
	}

	if (totalVolume > 0.0) {
		this->geometricError = totalWeightedError / totalVolume;
	}

	// 确保父瓦片的几何误差大于任何子瓦片
	for (const auto& child : this->children) {
		if (child->geometricError >= this->geometricError) {
			this->geometricError = child->geometricError + 0.1;
			OSG_NOTICE << "父子瓦片几何误差相差无几：父亲：level:" << this->level << ",x:" << this->x << ",y:" << this->y << ",z:" << this->z
				<< ";儿子：level:" << child->level << ",x:" << child->x << ",y:" << child->y << ",z:" << child->z << std::endl;
		}
	}
}

void B3DMTile::computeBoundingBox()
{
	if (this->node.valid())
	{
		osg::ComputeBoundsVisitor cbv;
		this->node->accept(cbv);
		this->bb = cbv.getBoundingBox();
		this->diagonalLength = (this->bb._max - this->bb._min).length();

		// 计算体积
		this->volume = (this->bb._max.x() - this->bb._min.x()) *
			(this->bb._max.y() - this->bb._min.y()) *
			(this->bb._max.z() - this->bb._min.z());

		MaxGeometryVisitor mgv;
		this->node->accept(mgv);
		this->maxClusterBb = mgv.maxBB;

		// 使用聚类后的体积
		this->maxClusterVolume = (this->maxClusterBb._max.x() - this->maxClusterBb._min.x()) *
			(this->maxClusterBb._max.y() - this->maxClusterBb._min.y()) *
			(this->maxClusterBb._max.z() - this->maxClusterBb._min.z());

		this->maxClusterDiagonalLength = (this->maxClusterBb._min - this->maxClusterBb._max).length();
	}

#ifdef OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
	/* single thread */
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		osg::ref_ptr<B3DMTile> b3dmTile = dynamic_cast<B3DMTile*>(this->children[i].get());
		if (b3dmTile.valid())
			b3dmTile->computeBoundingBox();
	}
#else
	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r)
		{
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				osg::ref_ptr<B3DMTile> b3dmTile = dynamic_cast<B3DMTile*>(this->children[i].get());
				if (b3dmTile.valid())
					b3dmTile->computeBoundingBox();
			}
		});
#endif // !OSG_GIS_PLUGINS_WRITE_TILE_BY_SINGLE_THREAD
	}

void B3DMTile::buildBaseHlod()
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
		const osg::BoundingBox boundingBox = this->bb;
		const double currentRadius = computeRadius(boundingBox, this->axis);

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

		buildBaseHlod();
	}
}

void B3DMTile::rebuildHlod()
{
	if (this->refine == Refinement::UNDEFINED)
		this->refine = Refinement::REPLACE;
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		osg::ref_ptr<B3DMTile> b3dmTile = dynamic_cast<B3DMTile*>(this->children[i].get());
		if (b3dmTile.valid())
		{
			if (b3dmTile->node->asGroup()->getNumChildren() == 0)
			{
				if (this->children[i]->children.size() == 0)
				{
					this->children.erase(this->children.begin() + i);
					i--;
					continue;
				}
			}
			b3dmTile->rebuildHlod();
		}
	}
	/*
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

		if (descendantNodeIsEmpty())
		{
			this->children.clear();
			this->geometricError = 0.0;
		}
	}

	for (size_t i = 0; i < this->children.size(); ++i)
	{
		osg::ref_ptr<B3DMTile> b3dmTile = dynamic_cast<B3DMTile*>(this->children[i].get());
		if (b3dmTile.valid())
		{
			if (b3dmTile->node->asGroup()->getNumChildren() == 0)
			{
				if (this->children[i]->children.size() == 0)
				{
					this->children.erase(this->children.begin() + i);
					i--;
					continue;
				}
			}
			b3dmTile->rebuildHlod();
		}
	}
	*/
}

void B3DMTile::buildHlod()
{
	//buildBaseHlod();
	rebuildHlod();
	computeBoundingBox();
	computeGeometricError();
}

void B3DMTile::write(const string& str, const float simplifyRatio, const GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options)
{
	computeBoundingVolumeBox();
	setContentUri();

	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		osg::ref_ptr<osg::Node> nodeCopy = osg::clone(node.get(), osg::CopyOp::DEEP_COPY_ALL);

		if (simplifyRatio < 1.0 && this->geometricError != 0.0 && this->refine == Refinement::REPLACE)
		{
			Simplifier simplifier(pow(simplifyRatio, getMaxLevel() - level - 1), true);
			nodeCopy->accept(simplifier);
		}


		GltfOptimizer gltfOptimzier;
		gltfOptimzier.setGltfTextureOptimizationOptions(gltfTextureOptions);
		gltfOptimzier.optimize(nodeCopy.get(), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS | GltfOptimizer::MERGE_TRANSFORMS);

		BatchIdVisitor biv;
		nodeCopy->accept(biv);
		const string path = str + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + to_string(level);
#ifndef OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
		tbb::spin_mutex::scoped_lock lock(writeMutex);
#endif // !OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
		osgDB::makeDirectory(path);
		osgDB::writeNodeFile(*nodeCopy.get(), path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + "Tile_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + "." + type, options);
	}
#ifdef OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
	/* single thread */
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
	}
#else
	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r)
		{
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
			}
		});
#endif // !OSG_GIS_PLUGINS_WRITE_TILE_BY_SINGLE_THREAD

}

double B3DMTile::computeRadius(const osg::BoundingBox& bbox, int axis)
{
	switch (axis)
	{
	case 0: // X-Y轴分割
		return (osg::Vec2(bbox._max.x(), bbox._max.y()) - osg::Vec2(bbox._min.x(), bbox._min.y())).length() / 2;
	case 1: // Y-Z轴分割
		return (osg::Vec2(bbox._max.z(), bbox._max.y()) - osg::Vec2(bbox._min.z(), bbox._min.y())).length() / 2;
	case 2: // Z-X轴分割
		return (osg::Vec2(bbox._max.z(), bbox._max.x()) - osg::Vec2(bbox._min.z(), bbox._min.x())).length() / 2;
	default:
		return (bbox._max - bbox._min).length();
	}
}
