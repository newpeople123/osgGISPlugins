#include "3dtiles/I3DMTile.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <osg/ComputeBoundsVisitor>
#include <osgDB/FileUtils>
using namespace osgGISPlugins;

void I3DMTile::optimizeNode(osg::ref_ptr<osg::Node>& nodeCopy, const GltfOptimizer::GltfTextureOptimizationOptions& options)
{
	GltfOptimizer gltfOptimizer;
	gltfOptimizer.setGltfTextureOptimizationOptions(options);
	osg::ref_ptr<osg::Group> group = nodeCopy->asGroup();
	gltfOptimizer.optimize(group->getChild(0), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);

	BatchIdVisitor biv;
	nodeCopy->accept(biv);
}

string I3DMTile::getOutputPath() const
{
	return config.path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + "InstanceTiles";
}

string I3DMTile::getFullPath() const
{
	return getOutputPath() + OSG_GIS_PLUGINS_PATH_SPLIT_STRING +
		"Tile_L" + to_string(lod) + "_" + to_string(z) + "." + type;
}

void I3DMTile::setContentUri()
{
	if (this->lod != -1)
		contentUri = "InstanceTiles/Tile_L" + to_string(lod) + "_" + to_string(z) + "." + type;
}

I3DMTile* I3DMTile::createTileOfSameType(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent)
{
	return new I3DMTile(node, dynamic_cast<I3DMTile*>(parent.get()));
}

void I3DMTile::computeDiagonalLengthAndVolume()
{
	if (this->node.valid())
	{
		osg::ref_ptr<osg::Node> instanceNode = this->node->asGroup()->getChild(0);
		osg::ComputeBoundsVisitor cbv;
		instanceNode->accept(cbv);
		const osg::BoundingBox bb = cbv.getBoundingBox();
		this->diagonalLength = (bb._max - bb._min).length();

		// 计算体积
		this->volume = (bb._max.x() - bb._min.x()) *
			(bb._max.y() - bb._min.y()) *
			(bb._max.z() - bb._min.z());
	}

	/* single thread */
	//for (size_t i = 0; i < this->children.size(); ++i)
	//{
	//	this->children[i]->computeDiagonalLengthAndVolume();
	//}

	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r)
		{
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->computeDiagonalLengthAndVolume();
			}
	});

}
