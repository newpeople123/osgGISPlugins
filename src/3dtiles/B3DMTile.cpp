#include "3dtiles/B3DMTile.h"
#include <osgDB/FileUtils>
using namespace osgGISPlugins;

void B3DMTile::optimizeNode(osg::ref_ptr<osg::Node>& nodeCopy, const GltfOptimizer::GltfTextureOptimizationOptions& options)
{
	if(!this->config.noApplyTransformToVertices)
		Tile::optimizeNode(nodeCopy, options, GltfOptimizer::GENERATE_NORMAL_TEXTURE | GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS | GltfOptimizer::FLATTEN_TRANSFORMS);
	else
		Tile::optimizeNode(nodeCopy, options, GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);
}

string B3DMTile::getOutputPath() const
{
	return config.path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + to_string(level);
}

string B3DMTile::getFullPath() const
{
	return getOutputPath() + OSG_GIS_PLUGINS_PATH_SPLIT_STRING +
		"Tile_L" + to_string(lod) + "_" + to_string(x) + "_" +
		to_string(y) + "_" + to_string(z) + "." + type;
}

string B3DMTile::getTextureCachePath(const string textureCachePath) const
{
	return textureCachePath + OSG_GIS_PLUGINS_PATH_SPLIT_STRING +
		"Tile_" + to_string(x) + "_" +
		to_string(y) + "_" + to_string(z) + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + to_string(lod);
}

void B3DMTile::setContentUri()
{
	if (this->lod != -1)
		contentUri = to_string(level) + "/" + "Tile_L" + to_string(lod) + "_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + "." + type;
}

B3DMTile* B3DMTile::createTileOfSameType(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent)
{
	return new B3DMTile(node, dynamic_cast<B3DMTile*>(parent.get()));
}

