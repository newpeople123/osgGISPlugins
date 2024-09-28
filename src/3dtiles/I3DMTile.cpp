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
	if (!this->children.size())
	{
		if (this->node.valid())
		{
			osg::ref_ptr<osg::Group> group = this->node->asGroup();
			if (group->getNumChildren())
			{
				osg::ComputeBoundsVisitor cbv;
				group->getChild(0)->accept(cbv);
				const osg::BoundingBox boundingBox = cbv.getBoundingBox();
				this->geometricError = (boundingBox._max - boundingBox._min).length() * 0.7 * CesiumGeometricErrorOperator;
			}
		}
		return;
	}
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		this->children[i]->computeGeometricError();
	}

	double total = 0.0;
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		const double childNodeGeometricError = this->children[i]->geometricError;
		this->geometricError += pow(childNodeGeometricError, 2);
		total += childNodeGeometricError;
		if (this->children[i]->children.size() == 0)
		{
			this->children[i]->geometricError = 0.0;
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

void I3DMTile::write(const string& str, const float simplifyRatio, const GltfOptimizer::GltfTextureOptimizationOptions& gltfTextureOptions, const osg::ref_ptr<osgDB::Options> options)
{
	computeBoundingVolumeBox();
	setContentUri();

	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		osg::ref_ptr<osg::Node> nodeCopy = osg::clone(node.get(), osg::CopyOp::DEEP_COPY_ALL);
		GltfOptimizer gltfOptimzier;
		gltfOptimzier.setGltfTextureOptimizationOptions(gltfTextureOptions);
		gltfOptimzier.optimize(nodeCopy.get(), GltfOptimizer::EXPORT_GLTF_OPTIMIZATIONS);

		BatchIdVisitor biv;
		nodeCopy->accept(biv);

		const string path = str + "\\InstanceTiles\\";
		osgDB::makeDirectory(path);
		osgDB::writeNodeFile(*nodeCopy.get(), path + "\\" + "Tile_" + to_string(z) + "." + type, options);

	}

	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r) {
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->write(str, simplifyRatio, gltfTextureOptions, options);
			}
		});
}

void I3DMTile::setContentUri()
{
	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		contentUri = "InstanceTiles/Tile_" + to_string(z) + "." + type;
	}
}
