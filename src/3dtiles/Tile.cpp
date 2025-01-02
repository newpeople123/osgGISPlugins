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

	const auto& textureOptions = config.gltfTextureOptions;
	const int maxTextureResolution = osg::maximum(textureOptions.maxTextureWidth,
		textureOptions.maxTextureHeight);

	float pixelSize = InitPixelSize;
	if (this->node.valid())
	{
		if (this->lod == 2)
		{
			pixelSize = maxTextureResolution * 0.25;
			pixelSize = pixelSize > 128.0 ? 128.0 : pixelSize;
		}
		else if (this->lod == 1)
		{
			pixelSize = maxTextureResolution * 0.5;
			pixelSize = pixelSize > 256.0 ? 256 : pixelSize;
		}
	}
	double totalWeightedError = 0.0;
	double totalWeight = 0.0;
	for (osg::ref_ptr<Tile> child : this->children) {
		if (child->lod == -1)
			for (osg::ref_ptr<Tile> grand : child->children)
			{
				if (
					child->level == grand->level &&
					child->x == grand->x &&
					child->y == grand->y &&
					child->z == grand->z &&
					grand->lod == 2
					)
				{
					child = grand;
					break;
				}
			}
		if (child->node.valid())
		{
			double childGeometricError = 0.0;
			if (this->lod != -1)
			{
				const double distance = getDistanceByPixelSize(pixelSize, child->childDiagonalLength * DIAGONAL_SCALE_FACTOR);
				childGeometricError = getCesiumGeometricErrorByDistance(distance) * child->diagonalLength / child->childDiagonalLength;
			}
			else
			{
				childGeometricError = getCesiumGeometricErrorByPixelSize(pixelSize, child->childDiagonalLength * DIAGONAL_SCALE_FACTOR);
			}
			totalWeightedError += childGeometricError * child->volume;
			totalWeight += child->volume;

		}
	}
	if (totalWeight > 0.0) {
		this->geometricError = totalWeightedError / totalWeight;

		// 确保父节点误差大于所有子节点
		double maxChildError = 0.0;
		for (const auto& child : this->children)
		{
			maxChildError = osg::maximum(maxChildError, child->geometricError);
		}
		if (this->geometricError < maxChildError)
		{
			this->geometricError = maxChildError + MIN_GEOMETRIC_ERROR_DIFFERENCE;
		}
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
	if (this->node.valid()) {
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

bool Tile::isEmptyNode(const osg::ref_ptr<Tile>& tile) const
{
	if (!tile->node.valid() || !tile->node->asGroup()) {
		return true;
	}
	return tile->node->asGroup()->getNumChildren() == 0 &&
		tile->children.empty();
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
	for (size_t i = 0; i < children.size();) {
		auto tile = children[i];
		if (!tile.valid()) {
			children.erase(children.begin() + i);
			continue;
		}

		if (tile->type != "i3dm") {  // i3dm类型特殊处理
			if (isEmptyNode(tile)) {
				children.erase(children.begin() + i);
				continue;
			}
			tile->cleanupEmptyNodes();  // 递归清理子节点
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
	setContentUri();  // 这个已经在基类中处理了不同类型的路径构建

	if (this->node.valid() && this->node->asGroup()->getNumChildren()) {
		writeNode();
	}

	writeChildren();
}

void Tile::writeChildren()
{
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

void Tile::applyLODStrategy(osg::ref_ptr<osg::Node>& nodeCopy, GltfOptimizer::GltfTextureOptimizationOptions& options)
{
	switch (this->lod) {
	case 2:
		applyLOD2Strategy(nodeCopy, options);
		break;
	case 1:
		applyLOD1Strategy(nodeCopy, options);
		break;
	default:
		applyLOD0Strategy(nodeCopy);
		break;
	}
}

void Tile::writeNode()
{
	// 1. 准备节点
	osg::ref_ptr<osg::Node> nodeCopy = osg::clone(node.get(), osg::CopyOp::DEEP_COPY_ALL);

	// 2. 配置纹理选项
	GltfOptimizer::GltfTextureOptimizationOptions gltfTextureOptions(config.gltfTextureOptions);
	applyLODStrategy(nodeCopy, gltfTextureOptions);

	// 3. 优化节点
	optimizeNode(nodeCopy, gltfTextureOptions);

	// 4. 写入文件
	writeToFile(nodeCopy);
}

void Tile::writeToFile(const osg::ref_ptr<osg::Node>& nodeCopy)
{
	const string outputPath = getOutputPath();
	osgDB::makeDirectory(outputPath);
	osgDB::writeNodeFile(*nodeCopy.get(), getFullPath(), config.options);
}

void Tile::buildLOD()
{
	// 1. 创建代理节点（LOD根节点）
	osg::ref_ptr<Tile> tileLOD2Proxy = this;

	// 构建LOD层级
	osg::ref_ptr<Tile> tileLOD2 = createLODTile(tileLOD2Proxy, 2);
	osg::ref_ptr<Tile> tileLOD1 = createLODTile(tileLOD2, 1);
	osg::ref_ptr<Tile> tileLOD0 = createLODTile(tileLOD1, 0);
	tileLOD0->refine = Refinement::ADD;

	// 建立节点关系
	tileLOD2Proxy->node = nullptr;
	tileLOD2Proxy->children.push_back(tileLOD2);
	tileLOD2->children.push_back(tileLOD1);
	tileLOD1->children.push_back(tileLOD0);

	// 递归处理子节点
	for (auto& child : tileLOD2Proxy->children) {
		if (child.get() != tileLOD2.get()) {
			child->config = config;
			child->buildLOD();
		}
	}
}

void Tile::computeDiagonalLengthAndVolume()
{
	if (this->node.valid())
	{
		computeDiagonalLengthAndVolume(this->node);
	}
}

void Tile::computeDiagonalLengthAndVolume(const osg::ref_ptr<osg::Node>& gnode)
{
	osg::ComputeBoundsVisitor cbv;
	gnode->accept(cbv);
	const osg::BoundingBox bb = cbv.getBoundingBox();
	this->diagonalLength = (bb._max - bb._min).length();

	// 计算体积
	this->volume = (bb._max.x() - bb._min.x()) *
		(bb._max.y() - bb._min.y()) *
		(bb._max.z() - bb._min.z());

	TextureMetricsVisitor tmv(config.gltfTextureOptions.maxTextureWidth > config.gltfTextureOptions.maxTextureHeight ? config.gltfTextureOptions.maxTextureWidth : config.gltfTextureOptions.maxTextureHeight);
	gnode->accept(tmv);
	if (tmv.diagonalLength > 0.0)
		this->childDiagonalLength = tmv.diagonalLength;
	else
	{
		MaxGeometryVisitor mgv;
		gnode->accept(mgv);
		const osg::BoundingBox maxClusterBb = mgv.maxBB;
		this->childDiagonalLength = (maxClusterBb._max - maxClusterBb._min).length();
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
	// 使用工厂方法创建对应类型的Tile
	auto tile = createTileOfSameType(this->node, parent);
	tile->config = this->config;
	tile->level = this->level;
	tile->x = this->x;
	tile->y = this->y;
	tile->z = this->z;
	tile->lod = lodLevel;
	tile->diagonalLength = this->diagonalLength;
	tile->volume = this->volume;
	tile->childDiagonalLength = this->childDiagonalLength;
	tile->refine = Refinement::REPLACE;
	return tile;
}

void Tile::applyLOD2Strategy(osg::ref_ptr<osg::Node>& nodeCopy, GltfOptimizer::GltfTextureOptimizationOptions& options)
{
	if (config.simplifyRatio < 1.0) {
		Simplifier simplifier(config.simplifyRatio, true);
		nodeCopy->accept(simplifier);
	}
	options.maxTextureWidth /= 4;
	options.maxTextureHeight /= 4;
}

void Tile::applyLOD1Strategy(osg::ref_ptr<osg::Node>& nodeCopy, GltfOptimizer::GltfTextureOptimizationOptions& options)
{
	if (config.simplifyRatio < 1.0) {
		Simplifier simplifier(config.simplifyRatio);
		nodeCopy->accept(simplifier);
	}
	options.maxTextureWidth /= 2;
	options.maxTextureHeight /= 2;
}

void Tile::applyLOD0Strategy(osg::ref_ptr<osg::Node>& nodeCopy)
{
	//if (config.simplifyRatio < 1.0) {
	//	Simplifier simplifier(config.simplifyRatio);
	//	nodeCopy->accept(simplifier);
	//}
}
