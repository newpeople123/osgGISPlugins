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

void B3DMTile::buildLOD()
{
	// 1. 创建代理节点（LOD根节点）
	osg::ref_ptr<B3DMTile> tileLOD2Proxy = this;

	// 2. 创建三级LOD节点
	auto createLODTile = [this](osg::ref_ptr<B3DMTile> parent, int lodLevel) {
		auto tile = new B3DMTile(this->node, parent);
		tile->config = this->config;
		tile->level = this->level;
		tile->x = this->x;
		tile->y = this->y;
		tile->z = this->z;
		tile->lod = lodLevel;
		tile->refine = Refinement::REPLACE;
		return tile;
		};

	// 构建LOD层级
	osg::ref_ptr<B3DMTile> tileLOD2 = createLODTile(tileLOD2Proxy, 2);
	osg::ref_ptr<B3DMTile> tileLOD1 = createLODTile(tileLOD2, 1);
	osg::ref_ptr<B3DMTile> tileLOD0 = createLODTile(tileLOD1, 0);
	tileLOD0->refine = Refinement::ADD;

	// 建立节点关系
	tileLOD2Proxy->node = nullptr;
	tileLOD2Proxy->children.push_back(tileLOD2);
	tileLOD2->children.push_back(tileLOD1);
	tileLOD1->children.push_back(tileLOD0);

	// 递归处理子节点
	for (auto& child : tileLOD2Proxy->children)
	{
		if (child.get() != tileLOD2.get())
		{
			child->config = config;
			child->buildLOD();
		}
	}

}

void B3DMTile::computeBoundingBox()
{
	if (this->node.valid())
	{
		osg::ComputeBoundsVisitor cbv;
		this->node->accept(cbv);
		const osg::BoundingBox bb = cbv.getBoundingBox();
		this->diagonalLength = (bb._max - bb._min).length();

		// 计算体积
		this->volume = (bb._max.x() - bb._min.x()) *
			(bb._max.y() - bb._min.y()) *
			(bb._max.z() - bb._min.z());

		MaxGeometryVisitor mgv;
		this->node->accept(mgv);
		const osg::BoundingBox maxClusterBb = mgv.maxBB;

		// 使用聚类后的体积
		const double maxClusterVolume = (maxClusterBb._max.x() - maxClusterBb._min.x()) *
			(maxClusterBb._max.y() - maxClusterBb._min.y()) *
			(maxClusterBb._max.z() - maxClusterBb._min.z());

		const double maxClusterDiagonalLength = (maxClusterBb._min - maxClusterBb._max).length();

		this->diagonalLength = this->diagonalLength > 2 * maxClusterDiagonalLength ? maxClusterDiagonalLength : this->diagonalLength;
		this->volume = this->diagonalLength > 2 * maxClusterDiagonalLength ? maxClusterVolume : this->volume;
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

void B3DMTile::write()
{
	computeBoundingVolumeBox();
	setContentUri();

	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		osg::ref_ptr<osg::Node> nodeCopy = osg::clone(node.get(), osg::CopyOp::DEEP_COPY_ALL);
		GltfOptimizer::GltfTextureOptimizationOptions gltfTextureOptions(config.gltfTextureOptions);

		// 根据LOD层级应用不同的简化策略
		switch (this->lod) {
		case 2: {  // LOD2: 最低精度
			if (config.simplifyRatio < 1.0)
			{
				Simplifier simplifier(config.simplifyRatio, false);
				nodeCopy->accept(simplifier);
			}
			gltfTextureOptions.maxTextureWidth /= 4;
			gltfTextureOptions.maxTextureHeight /= 4;
			break;
		}
		case 1: {  // LOD1: 中等精度
			if (config.simplifyRatio < 1.0)
			{
				Simplifier simplifier(config.simplifyRatio, true);
				nodeCopy->accept(simplifier);
			}
			gltfTextureOptions.maxTextureWidth /= 2;
			gltfTextureOptions.maxTextureHeight /= 2;
			break;
		}
		}
	
		GltfOptimizer gltfOptimzier;
		gltfOptimzier.setGltfTextureOptimizationOptions(gltfTextureOptions);
		gltfOptimzier.optimize(nodeCopy.get(), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS | GltfOptimizer::MERGE_TRANSFORMS);

		BatchIdVisitor biv;
		nodeCopy->accept(biv);
		
		// 构建输出路径
		const string outputPath = config.path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + to_string(level);
		const string filename = "Tile_L" + to_string(lod) + "_" +
			to_string(x) + "_" +
			to_string(y) + "_" +
			to_string(z) + "." + type;

		// 写入文件
#ifndef OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
		tbb::spin_mutex::scoped_lock lock(writeMutex);
#endif
		osgDB::makeDirectory(outputPath);
		osgDB::writeNodeFile(*nodeCopy.get(), outputPath + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + filename, config.options);
	}

#ifdef OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
	for (auto& child : children) {
		child->write();
	}
#else
	tbb::parallel_for(tbb::blocked_range<size_t>(0, children.size()),
		[&](const tbb::blocked_range<size_t>& r) {
			for (size_t i = r.begin(); i < r.end(); ++i) {
				children[i]->write();
			}
		});
#endif

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
