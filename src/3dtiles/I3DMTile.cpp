#include "3dtiles/I3DMTile.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include "utils/Simplifier.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/FileUtils>
using namespace osgGISPlugins;

void I3DMTile::write()
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
		osg::ref_ptr<osg::Group> group = nodeCopy->asGroup();
		gltfOptimzier.optimize(group->getChild(0), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);

		BatchIdVisitor biv;
		nodeCopy->accept(biv);

		// 构建输出路径
		const string outputPath = config.path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + "InstanceTiles";
		const string filename = "Tile_L" + to_string(lod) + "_" + to_string(z) + "." + type;

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

void I3DMTile::buildLOD()
{
	// 1. 创建代理节点（LOD根节点）
	osg::ref_ptr<I3DMTile> tileLOD2Proxy = this;

	// 2. 创建三级LOD节点
	auto createLODTile = [this](osg::ref_ptr<I3DMTile> parent, int lodLevel) {
		auto tile = new I3DMTile(this->node, parent);
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
	osg::ref_ptr<I3DMTile> tileLOD2 = createLODTile(tileLOD2Proxy, 2);
	osg::ref_ptr<I3DMTile> tileLOD1 = createLODTile(tileLOD2, 1);
	osg::ref_ptr<I3DMTile> tileLOD0 = createLODTile(tileLOD1, 0);
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
