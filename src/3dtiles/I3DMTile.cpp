#include "3dtiles/I3DMTile.h"
#include <osgDB/FileUtils>
using namespace osgGISPlugins;

void I3DMTile::optimizeNode(osg::ref_ptr<osg::Node>& nodeCopy, const GltfOptimizer::GltfTextureOptimizationOptions& options)
{
	Tile::optimizeNode(nodeCopy, options, GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);
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
		Tile::computeDiagonalLengthAndVolume(instanceNode);
	}
}
