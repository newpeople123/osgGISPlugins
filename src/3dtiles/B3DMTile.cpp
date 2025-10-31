#include "3dtiles/B3DMTile.h"
#include <osgDB/FileUtils>
using namespace osgGISPlugins;

std::string B3DMTile::getOutputPath() const
{
	return config.path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + std::to_string(level);
}

std::string B3DMTile::getFullPath() const
{
	return getOutputPath() + OSG_GIS_PLUGINS_PATH_SPLIT_STRING +
		"Tile_L" + std::to_string(lod) + "_" + std::to_string(x) + "_" +
		std::to_string(y) + "_" + std::to_string(z) + "." + type;
}

std::string B3DMTile::getTextureCachePath(const std::string textureCachePath) const
{
	return textureCachePath + OSG_GIS_PLUGINS_PATH_SPLIT_STRING +
		"Tile_" + std::to_string(x) + "_" +
		std::to_string(y) + "_" + std::to_string(z) + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + std::to_string(lod);
}

void B3DMTile::setContentUri()
{
	if (this->lod != -1)
		contentUri = std::to_string(level) + "/" + "Tile_L" + std::to_string(lod) + "_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z) + "." + type;
}

B3DMTile* B3DMTile::createTileOfSameType(osg::ref_ptr<osg::Node> node, osg::ref_ptr<Tile> parent)
{
	return new B3DMTile(node, dynamic_cast<B3DMTile*>(parent.get()));
}

