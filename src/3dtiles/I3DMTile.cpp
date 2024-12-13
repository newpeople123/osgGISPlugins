#include "3dtiles/I3DMTile.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/FileUtils>
using namespace osgGISPlugins;
void I3DMTile::computeGeometricError()
{
	this->refine = Refinement::ADD;
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		this->children[i]->computeGeometricError();
	}

	if (!this->children.size())
	{
		this->geometricError = 0.0;
	}
	else
	{
		for (size_t i = 0; i < this->children.size(); ++i)
		{
			osg::ref_ptr<osg::Node> child = this->children[i]->node;
			if (child.valid())
			{
				osg::ref_ptr<osg::Group> group = child->asGroup();
				if (group->getNumChildren())
				{
					osg::ComputeBoundsVisitor cbv;
					group->getChild(0)->accept(cbv);
					const osg::BoundingBox boundingBox = cbv.getBoundingBox();
					this->geometricError = osg::maximum((boundingBox._max - boundingBox._min).length() * 0.7 * CesiumGeometricErrorOperator, this->geometricError);
				}
			}
			if (this->geometricError < this->children[i]->geometricError)
				this->geometricError = this->children[i]->geometricError + 0.1;
		}
	}

}

void I3DMTile::write(const string& str, const float simplifyRatio, const GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options)
{
	computeBoundingVolumeBox();
	setContentUri();

	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		osg::ref_ptr<osg::Node> nodeCopy = osg::clone(node.get(), osg::CopyOp::DEEP_COPY_ALL);
		GltfOptimizer gltfOptimzier;
		gltfOptimzier.setGltfTextureOptimizationOptions(gltfTextureOptions);
		osg::ref_ptr<osg::Group> group = nodeCopy->asGroup();
		gltfOptimzier.optimize(group->getChild(0), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);

		BatchIdVisitor biv;
		nodeCopy->accept(biv);
		const string path = str + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + "InstanceTiles";
		osgDB::makeDirectory(path);
#ifndef OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
		tbb::spin_mutex::scoped_lock lock(writeMutex);
#endif // !OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
		osgDB::writeNodeFile(*nodeCopy.get(), path + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + "Tile_" + to_string(z) + "." + type, options);

	}
#ifdef OSG_GIS_PLUGINS_ENABLE_WRITE_TILE_BY_SINGLE_THREAD
	/* single thread */
	for (size_t i = 0; i < this->children.size(); ++i)
	{

		this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
	}
#else
	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r) {
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
			}
		});
#endif // !OSG_GIS_PLUGINS_WRITE_TILE_BY_SINGLE_THREAD
}

void I3DMTile::setContentUri()
{
	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		contentUri = "InstanceTiles/Tile_" + to_string(z) + "." + type;
	}
}
