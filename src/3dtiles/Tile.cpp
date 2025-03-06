#include "3dtiles/Tile.h"
#include "utils/Simplifier.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
void Tile::fromJson(const json& j) {
	if (j.contains("boundingVolume")) boundingVolume.fromJson(j.at("boundingVolume"));
	if (j.contains("geometricError")) j.at("geometricError").get_to(geometricError);
	if (j.contains("refine")) {
		string refineStr;
		j.at("refine").get_to(refineStr);
		if (refineStr == "REPLACE") {
			refine = Refinement::REPLACE;
		}
		else if (refineStr == "ADD") {
			refine = Refinement::ADD;
		}
	}
	if (j.contains("content") && j.at("content").contains("uri")) {
		j.at("content").at("uri").get_to(contentUri);
	}

	if (j.contains("transform")) {
		j.at("transform").get_to(transform);
	}
}

json Tile::toJson() const {
	json j;
	j["boundingVolume"] = boundingVolume.toJson();
	j["geometricError"] = geometricError;
	j["refine"] = (refine == Refinement::REPLACE) ? "REPLACE" : "ADD";

	if (!contentUri.empty()) {
		j["content"]["uri"] = contentUri;
	}

	if (!transform.empty()) {
		j["transform"] = transform;
	}

	if (!children.empty()) {
		j["children"] = json::array();
		for (const auto& child : children) {
			j["children"].push_back(child->toJson());
		}
	}
	return j;
}

int Tile::getMaxLevel() const
{
	int currentLevel = this->level;
	for (auto& child : this->children)
	{
		currentLevel = osg::maximum(child->getMaxLevel(), currentLevel);
	}
	return currentLevel;
}

void Tile::computeBoundingVolumeBox()
{
	osg::ref_ptr<osg::Group> group = getAllDescendantNodes();
	boundingVolume.computeBox(group);
}

void Tile::computeGeometricError()
{
	if (!this->children.size())
		return;

	//for (size_t i = 0; i < this->children.size(); ++i)
	//{
	//	this->children[i]->computeGeometricError();
	//}

	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r)
		{
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->computeGeometricError();
			}
		});

	double totalWeightedError = 0.0;
	double totalWeight = 0.0;
	for (osg::ref_ptr<Tile> child : this->children) {
		if (child->node.valid())
		{
			double childGeometricError = 0.0;

			if (this->lod != -1)
			{
				if (child->lod == 0)
					childGeometricError = getCesiumGeometricErrorByLodError(this->lodError * 0.5, child->diagonalLength * DIAGONAL_SCALE_FACTOR);
				else
					childGeometricError = getCesiumGeometricErrorByLodError(child->lodError, child->diagonalLength * DIAGONAL_SCALE_FACTOR);
			}
			else
			{
				childGeometricError = getCesiumGeometricErrorByPixelSize(InitPixelSize, child->diagonalLength * DIAGONAL_SCALE_FACTOR);
			}

			totalWeightedError += childGeometricError * child->volume;
			totalWeight += child->volume;

		}
	}
	if (totalWeight > 0.0) {
		this->geometricError = totalWeightedError / totalWeight;
	}
	// 确保父节点误差大于所有子节点
	double maxChildError = 0.0;
	for (const auto& child : this->children)
	{
		maxChildError = osg::maximum(maxChildError, child->geometricError);
	}
	if (maxChildError >= this->geometricError)
	{
		this->geometricError = maxChildError + MIN_GEOMETRIC_ERROR_DIFFERENCE;
	}

}

bool Tile::descendantNodeIsEmpty() const
{
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		if (this->children[i]->node->asGroup()->getNumChildren() > 0)
			return false;
		if (!this->children[i]->descendantNodeIsEmpty())
			return false;
	}
	return true;
}

osg::ref_ptr<osg::Group> Tile::getAllDescendantNodes() const
{
	osg::ref_ptr<osg::Group> group = new osg::Group;
	if (this->node.valid() && this->lod == 0) {
		osg::ref_ptr<osg::Group> currentNodeAsGroup = this->node->asGroup();
		for (size_t i = 0; i < currentNodeAsGroup->getNumChildren(); ++i)
		{
			group->addChild(currentNodeAsGroup->getChild(i));
		}
	}
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		osg::ref_ptr<osg::Group> childGroup = this->children[i]->getAllDescendantNodes();
		for (size_t j = 0; j < childGroup->getNumChildren(); ++j)
		{
			group->addChild(childGroup->getChild(j));
		}
	}
	return group;
}

bool Tile::valid() const
{
	const size_t size = this->children.size();
	if (size)
	{
		bool isValid = true;
		for (size_t i = 0; i < size; ++i)
		{
			if (this->geometricError <= this->children[i]->geometricError)
			{
				isValid = false;
				break;
			}
			if (!this->children[i]->valid())
			{
				isValid = false;
				break;
			}
		}
		return isValid;
	}
	else
		return this->geometricError == 0.0;
	return true;
}

void Tile::cleanupEmptyNodes()
{
	for (size_t i = 0; i < this->children.size();) {
		osg::ref_ptr<Tile> child = this->children[i];
		child->cleanupEmptyNodes();  // 递归清理子节点

		if (!child->node.valid() && child->children.size() == 0) {
			this->children.erase(this->children.begin() + i);
			continue;
		}
		++i;
	}
}

void Tile::build()
{
	cleanupEmptyNodes();
	computeDiagonalLengthAndVolume();
	buildLOD();
	computeGeometricError();
}

double Tile::getCesiumGeometricErrorByPixelSize(const float pixelSize, const float radius)
{
	const double distance = getDistanceByPixelSize(pixelSize, radius);
	return getCesiumGeometricErrorByDistance(distance);
}

double Tile::getCesiumGeometricErrorByLodError(const float lodError, const double radius)
{
	/*
	float d = max(0, distance(camera_position, mesh_center) - mesh_radius);
	float e = d * (tan(camera_fovy / 2) * 2 / screen_height); // 1px in mesh space
	bool lod_ok = e * lod_factor >= lod_error;
	*/
	const double distance = lodError / LOD_FACTOR / (tan(CesiumFrustumFovy / 2) * 2 / CesiumCanvasClientHeight) + radius;
	const double geometricError = getCesiumGeometricErrorByDistance(distance);
	return geometricError;
}

double Tile::getCesiumGeometricErrorByDistance(const float distance)
{
	return CesiumSSEDenominator * CesiumMaxScreenSpaceError * distance / CesiumCanvasClientHeight;
}

double Tile::getDistanceByPixelSize(const float pixelSize, const float radius)
{
	const double angularSize = DPP * pixelSize;
	const double distance = radius / tan(angularSize / 2.0);
	return distance;
}

void Tile::write()
{
	computeBoundingVolumeBox();

	if (this->node.valid()) {
		if (writeNode())
			setContentUri();
	}

	writeChildren();
}

void Tile::writeChildren()
{
	for (auto& child : children) {
		child->write();
	}
}

bool Tile::writeNode()
{
	Utils::TriangleCounterVisitor tcv;
	node->accept(tcv);
	if (tcv.count == 0)
		return false;

	optimizeNode(node, config.gltfTextureOptions);

	writeToFile(node);

	node = nullptr;
	return true;
}

void Tile::writeToFile(const osg::ref_ptr<osg::Node>& nodeCopy)
{

	const string outputPath = getOutputPath();
	if(osgDB::makeDirectory(outputPath))
		osgDB::writeNodeFile(*nodeCopy.get(), getFullPath(), config.options);
}

void Tile::buildLOD()
{
	// 创建代理节点（LOD根节点）
	osg::ref_ptr<Tile> rootProxy = this;
	unsigned int size = rootProxy->children.size();

	if (rootProxy->node.valid())
	{
		osg::ref_ptr<Tile> lodProxy;

		// 根据是否有子节点决定是否需要额外的代理层
		if (this->children.size() > 0)
		{
			lodProxy = createLODTile(rootProxy, -1);
			rootProxy->node = nullptr;
			rootProxy->children.push_back(lodProxy);
		}
		else
		{
			lodProxy = rootProxy;
		}

		// 构建LOD层级
		osg::ref_ptr<Tile> tileLOD2 = createLODTile(lodProxy, 2);
		osg::ref_ptr<Tile> tileLOD1 = createLODTile(tileLOD2, 1);
		osg::ref_ptr<Tile> tileLOD0 = createLODTile(tileLOD1, 0);

		// 设置属性和关系
		tileLOD0->refine = Refinement::ADD;
		lodProxy->node = nullptr;
		lodProxy->children.push_back(tileLOD2);
		tileLOD2->children.push_back(tileLOD1);
		tileLOD1->children.push_back(tileLOD0);

		GltfOptimizer optimizer;
		optimizer.optimize(tileLOD0->node.get(), GltfOptimizer::INDEX_MESH_BY_MESHOPTIMIZER);
		optimizer.optimize(tileLOD1->node.get(), GltfOptimizer::INDEX_MESH_BY_MESHOPTIMIZER);
		optimizer.optimize(tileLOD2->node.get(), GltfOptimizer::INDEX_MESH_BY_MESHOPTIMIZER);
		tileLOD0->applyLODStrategy();
		tileLOD1->applyLODStrategy();
		tileLOD2->applyLODStrategy();

		size = rootProxy->children.size() - 1;// 最后一个是lod2或者lodProxy,不处理
	}
	// 递归处理子节点
	for (size_t i = 0; i < size; ++i)
	{
		auto& child = rootProxy->children.at(i);
		child->config = config;
		child->buildLOD();
	}
}

void Tile::computeDiagonalLengthAndVolume()
{
	computeDiagonalLengthAndVolume(this->node);
}

void Tile::computeDiagonalLengthAndVolume(const osg::ref_ptr<osg::Node>& gnode)
{
	if (gnode.valid())
	{
		osg::ComputeBoundsVisitor cbv;
		gnode->accept(cbv);
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

void Tile::optimizeNode(osg::ref_ptr<osg::Node>& nodeCopy, const GltfOptimizer::GltfTextureOptimizationOptions& textureOptions, unsigned int options)
{
	GltfOptimizer gltfOptimizer;
	gltfOptimizer.setGltfTextureOptimizationOptions(textureOptions);
	gltfOptimizer.optimize(nodeCopy.get(), options);

	BatchIdVisitor biv;
	nodeCopy->accept(biv);
}

osg::ref_ptr<Tile> Tile::createLODTile(osg::ref_ptr<Tile> parent, int lodLevel)
{
	osg::ref_ptr<osg::Node> nodeCopy = osg::clone(parent->node.get(),
		osg::CopyOp::DEEP_COPY_NODES |
		osg::CopyOp::DEEP_COPY_DRAWABLES |
		osg::CopyOp::DEEP_COPY_ARRAYS |
		osg::CopyOp::DEEP_COPY_PRIMITIVES |
		osg::CopyOp::DEEP_COPY_USERDATA);
	// 使用工厂方法创建对应类型的Tile
	auto tile = createTileOfSameType(nodeCopy, parent);
	tile->config = parent->config;
	tile->level = parent->level;
	tile->x = parent->x;
	tile->y = parent->y;
	tile->z = parent->z;
	tile->lod = lodLevel;
	tile->diagonalLength = parent->diagonalLength;
	tile->volume = parent->volume;
	tile->refine = Refinement::REPLACE;
	return tile;
}

void Tile::applyLODStrategy()
{
	switch (this->lod) {
	case 2:
		applyLODStrategy(0.5f, 0.25f);
		break;
	case 1:
		applyLODStrategy(1.0f, 0.5f);
		break;
	default:
		break;
	}
}

void Tile::applyLODStrategy(const float simplifyRatioFactor, const float textureFactor)
{
	if (config.simplifyRatio < 1.0) {
		Simplifier simplifier(config.simplifyRatio * simplifyRatioFactor);
		node->accept(simplifier);
		lodError = simplifier.lodError;
		if (osg::equivalent(lodError, 0.0f))
		{
			this->parent->children.push_back(this->children[0]);
			this->children[0]->parent = this->parent;
			auto it = std::find(this->parent->children.begin(), this->parent->children.end(), this);
			if (it != this->parent->children.end()) {
				this->parent->children.erase(it);
			}
			node = nullptr;
		}
	}

	config.gltfTextureOptions.maxTextureWidth *= textureFactor;
	config.gltfTextureOptions.maxTextureHeight *= textureFactor;
	config.gltfTextureOptions.maxTextureAtlasWidth *= textureFactor;
	config.gltfTextureOptions.maxTextureAtlasHeight *= textureFactor;
}
