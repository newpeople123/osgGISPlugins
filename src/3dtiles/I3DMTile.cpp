#include "3dtiles/I3DMTile.h"
#include <osgDB/FileUtils>
using namespace osgGISPlugins;

void I3DMTile::optimizeNode()
{
	unsigned int options = config.gltfOptimizerOptions & ~GltfOptimizer::FLATTEN_STATIC_TRANSFORMS;
	options &= ~GltfOptimizer::FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS;
	options &= ~GltfOptimizer::FLATTEN_TRANSFORMS;
	options &= ~GltfOptimizer::MERGE_TRANSFORMS;

	Tile::optimizeNode(options);
}

std::string I3DMTile::getOutputPath() const
{
	return config.path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + "InstanceTiles";
}

std::string I3DMTile::getFullPath() const
{
	return getOutputPath() + OSG_GIS_PLUGINS_PATH_SPLIT_STRING +
		"Tile_L" + std::to_string(lod) + "_" + std::to_string(z) + "." + type;
}

std::string I3DMTile::getTextureCachePath(const std::string textureCachePath) const
{
	return textureCachePath + OSG_GIS_PLUGINS_PATH_SPLIT_STRING +
		"Tile_" + std::to_string(z) + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + std::to_string(lod);
}

void I3DMTile::setContentUri()
{
	if (this->lod != -1)
		contentUri = "InstanceTiles/Tile_L" + std::to_string(lod) + "_" + std::to_string(z) + "." + type;
}

I3DMTile* I3DMTile::createTileOfSameType(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent)
{
	return new I3DMTile(node, dynamic_cast<I3DMTile*>(parent.get()));
}

void I3DMTile::computeDiagonalLengthAndVolume()
{
	osg::ref_ptr<osg::Node> instanceNode = this->node->asGroup()->getChild(0);
	Tile::computeDiagonalLengthAndVolume(instanceNode);
}
