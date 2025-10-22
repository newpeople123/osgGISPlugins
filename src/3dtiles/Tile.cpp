#include "3dtiles/Tile.h"
#include "utils/Simplifier.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"

void Tile::fromJson(const json& j)
{
	if (j.contains("boundingVolume"))
		boundingVolume.fromJson(j.at("boundingVolume"));
	if (j.contains("geometricError"))
		j.at("geometricError").get_to(geometricError);
	if (j.contains("refine"))
	{
		string refineStr;
		j.at("refine").get_to(refineStr);
		if (refineStr == "REPLACE")
		{
			refine = Refinement::REPLACE;
		}
		else if (refineStr == "ADD")
		{
			refine = Refinement::ADD;
		}
	}
	if (j.contains("content") && j.at("content").contains("uri"))
	{
		j.at("content").at("uri").get_to(contentUri);
	}

	if (j.contains("transform"))
	{
		j.at("transform").get_to(transform);
	}
}

json Tile::toJson() const
{
	json j;
	j["boundingVolume"] = boundingVolume.toJson();
	j["geometricError"] = geometricError;
	j["refine"] = (refine == Refinement::REPLACE) ? "REPLACE" : "ADD";

	if (!contentUri.empty())
	{
		j["content"]["uri"] = contentUri;
	}

	if (!transform.empty())
	{
		j["transform"] = transform;
	}

	if (!children.empty())
	{
		j["children"] = json::array();
		for (const auto& child : children)
		{
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

#ifndef USE_TBB_PARALLEL
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		this->children[i]->computeGeometricError();
	}
#else
	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r)
		{
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->computeGeometricError();
			}
		});
#endif // !USE_TBB_PARALLEL


	double totalWeightedError = 0.0;
	double totalWeight = 0.0;
	for (osg::ref_ptr<Tile> child : this->children)
	{
		//if (child->node.valid())
		//{
		double childGeometricError = 0.0;

		if (this->lod != -1)
		{
			childGeometricError = getCesiumGeometricErrorByLodError(child->lodError, child->diagonalLength * DIAGONAL_SCALE_FACTOR);
		}
		else
		{
			float pixelSize = this->level > -1 ? InitPixelSize * std::pow(2, this->level) : InitPixelSize;
			pixelSize = osg::clampBetween(pixelSize, InitPixelSize, 250.0f);
			//pixelSize = InitPixelSize;
			childGeometricError = getCesiumGeometricErrorByPixelSize(pixelSize, child->diagonalLength * DIAGONAL_SCALE_FACTOR);
		}

		totalWeightedError += childGeometricError * child->volume;
		totalWeight += child->volume;
		//}
	}
	if (totalWeight > 0.0)
	{
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
	if (this->node.valid() && this->lod == 0)
	{
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
	for (size_t i = 0; i < this->children.size();)
	{
		osg::ref_ptr<Tile> child = this->children[i];
		child->cleanupEmptyNodes(); // 递归清理子节点

		if (!child->node.valid() && child->children.size() == 0)
		{
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

void Tile::getVolumeWeightedDiagonalLength(osg::ref_ptr<osg::Group> group, double& outDiagonalLength, double& outVolume)
{
	outDiagonalLength = 0.0;
	outVolume = 0.0;

	std::vector<double> clusterVolumes;
	std::vector<double> clusterDiagonals;
	double totalVolume = 0.0;

	for (size_t i = 0; i < group->getNumChildren(); ++i)
	{
		osg::ref_ptr<osg::Node> node = group->getChild(i);

		Utils::MaxGeometryVisitor mgv;
		node->accept(mgv);
		const osg::BoundingBox clusterMaxBound = mgv.maxBB;

		// 计算聚类体积
		const double clusterVolume = (clusterMaxBound._max.x() - clusterMaxBound._min.x()) *
			(clusterMaxBound._max.y() - clusterMaxBound._min.y()) *
			(clusterMaxBound._max.z() - clusterMaxBound._min.z());

		// 计算聚类体积的对角线长度
		const double clusterDiagonalLength = (clusterMaxBound._max - clusterMaxBound._min).length();

		clusterVolumes.push_back(clusterVolume);
		clusterDiagonals.push_back(clusterDiagonalLength);

		totalVolume += clusterVolume;
	}

	// 计算加权平均对角线长度
	if (totalVolume > 0.0)
	{
		double weightedDiagonalSum = 0.0;
		for (size_t i = 0; i < clusterVolumes.size(); ++i)
		{
			double weight = clusterVolumes[i] / totalVolume;
			weightedDiagonalSum += clusterDiagonals[i] * weight;
		}
		outDiagonalLength = weightedDiagonalSum;
	}

	outVolume = totalVolume;
}

void Tile::write()
{
	computeBoundingVolumeBox();

	if (this->lod != -1)
	{
		if (writeNode())
			setContentUri();
	}

	writeChildren();
}

void Tile::writeChildren()
{
#ifndef USE_TBB_PARALLEL
	for (auto& child : children)
	{
		child->write();
	}
#else
	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r)
		{
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->write();
			}
		});
#endif // !USE_TBB_PARALLEL
}

bool Tile::writeNode()
{
	osg::ref_ptr<osg::Node> oldNode = this->node;
	this->node.release();
	switch (this->lod)
	{
	case 2:
		if (this->children[0]->lod == 0)
		{
			this->node = osg::clone(this->children[0]->node.get(), osg::CopyOp::DEEP_COPY_ALL);
		}
		else {
			this->node = osg::clone(this->children[0]->children[0]->node.get(), osg::CopyOp::DEEP_COPY_ALL);
		}
		this->applyLODStrategy();
		break;
	case 1:
		this->node = osg::clone(this->children[0]->node.get(), osg::CopyOp::DEEP_COPY_ALL);
		this->applyLODStrategy();
		break;
	case 0:
		this->node = osg::clone(oldNode.get(), osg::CopyOp::DEEP_COPY_ALL);
		this->applyLODStrategy();
		oldNode.release();
		break;
	default:
		break;
	}
	if (!this->node.valid()) return false;
	Utils::TriangleCounterVisitor tcv;
	this->node->accept(tcv);
	if (tcv.count == 0)
	{
		this->node = nullptr;
		return false;
	}

	this->optimizeNode();

	this->writeToFile();

	this->node = nullptr;
	return true;
}

void Tile::writeToFile()
{

	const string outputPath = getOutputPath();
	if (osgDB::makeDirectory(outputPath))
		osgDB::writeNodeFile(*this->node.get(), getFullPath(), config.options);
}

void Tile::buildLOD()
{
	unsigned int size = this->children.size();
	osg::ref_ptr<osg::Node> waitCopyNode = this->node;
	// 创建代理节点（LOD根节点）
	if (this->node.valid())
	{
		osg::ref_ptr<Tile> lodProxy;

		// 根据是否有子节点决定是否需要额外的代理层
		if (this->children.size() > 0)
		{
			lodProxy = createLODTileWithoutNode(this, -1);
			this->children.push_back(lodProxy);
		}
		else
		{
			lodProxy = this;
		}

		// 构建LOD层级
		osg::ref_ptr<Tile> tileLOD2 = createLODTile(lodProxy, waitCopyNode, 2);
		osg::ref_ptr<Tile> tileLOD1 = createLODTile(tileLOD2, waitCopyNode, 1);
		osg::ref_ptr<Tile> tileLOD0 = createLODTileWithoutNode(tileLOD1, 0);
		tileLOD0->node = waitCopyNode;

		// 设置属性和关系
		tileLOD0->refine = Refinement::ADD;
		lodProxy->node = nullptr;
		lodProxy->children.push_back(tileLOD2);
		tileLOD2->children.push_back(tileLOD1);
		tileLOD1->children.push_back(tileLOD0);

		tileLOD0->applyLODStrategy();
		tileLOD1->applyLODStrategy();
		tileLOD2->applyLODStrategy();

		if (tileLOD2->lodError == tileLOD1->lodError)
		{
			lodProxy->children.clear();
			lodProxy->children.push_back(tileLOD1);
			tileLOD1->parent = lodProxy;

			tileLOD2->parent = nullptr;
			tileLOD2->children.clear();
			tileLOD2->node = nullptr;
			if (tileLOD1->lodError == tileLOD0->lodError)
			{
				lodProxy->children.clear();
				lodProxy->children.push_back(tileLOD0);
				tileLOD0->parent = lodProxy;

				tileLOD1->parent = nullptr;
				tileLOD1->children.clear();
				tileLOD1->node = nullptr;
			}
		}
		else
		{
			if (tileLOD1->lodError == tileLOD0->lodError)
			{
				tileLOD2->children.clear();
				tileLOD2->children.push_back(tileLOD0);
				tileLOD0->parent = tileLOD2;

				tileLOD1->parent = nullptr;
				tileLOD1->children.clear();
				tileLOD1->node = nullptr;
			}
		}

		tileLOD2->node = nullptr;
		tileLOD1->node = nullptr;
		this->node = nullptr;

		size = this->children.size() - 1; // 最后一个是lod2或者lodProxy,不处理
	}

	// 递归处理子节点
	for (size_t i = 0; i < size; ++i)
	{
		auto& child = this->children.at(i);
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
		//osg::ComputeBoundsVisitor cbv;
		//gnode->accept(cbv);
		//const osg::BoundingBox bb = cbv.getBoundingBox();
		//this->diagonalLength = (bb._max - bb._min).length();

		//// 计算体积
		//this->volume = (bb._max.x() - bb._min.x()) *
		//	(bb._max.y() - bb._min.y()) *
		//	(bb._max.z() - bb._min.z());
		getVolumeWeightedDiagonalLength(gnode->asGroup(), this->diagonalLength, this->volume);
	}
#ifndef USE_TBB_PARALLEL
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		this->children[i]->computeDiagonalLengthAndVolume();
	}
#else
	tbb::parallel_for(tbb::blocked_range<size_t>(0, this->children.size()),
		[&](const tbb::blocked_range<size_t>& r)
		{
			for (size_t i = r.begin(); i < r.end(); ++i)
			{
				this->children[i]->computeDiagonalLengthAndVolume();
			}
		});
#endif // !USE_TBB_PARALLEL
}

void Tile::optimizeNode(const unsigned int options)
{
	GltfOptimizer gltfOptimizer;
	gltfOptimizer.setGltfTextureOptimizationOptions(this->config.gltfTextureOptions);
	gltfOptimizer.optimize(this->node.get(), options);

	BatchIdVisitor biv;
	this->node->accept(biv);
}

void Tile::optimizeNode()
{
	optimizeNode(config.gltfOptimizerOptions);
}

osg::ref_ptr<Tile> Tile::createLODTile(osg::ref_ptr<Tile> parent, osg::ref_ptr<osg::Node> waitCopyNode, int lodLevel)
{
	try
	{
		osg::ref_ptr<osg::Node> nodeCopy = osg::clone(
			waitCopyNode.get(),
			osg::CopyOp::DEEP_COPY_ALL);

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
	catch (const std::bad_alloc& e)
	{
		osg::notify(osg::FATAL) << "Error: Memory allocation failed. The model is too large or the available memory is insufficient. Please split the model or increase the available memory and try again!" << e.what() << std::endl;
		return nullptr;
	}
}

osg::ref_ptr<Tile> osgGISPlugins::Tile::createLODTileWithoutNode(osg::ref_ptr<Tile> parent, int lodLevel)
{
	try
	{
		auto tile = createTileOfSameType(nullptr, parent);
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
	catch (const std::bad_alloc& e)
	{
		osg::notify(osg::FATAL) << "Error: Memory allocation failed. The model is too large or the available memory is insufficient. Please split the model or increase the available memory and try again!" << e.what() << std::endl;
		return nullptr;
	}
}

void Tile::applyLODStrategy()
{
	switch (this->lod)
	{
	case 2:
		applyLODStrategy(0.5f, 0.25f);
		break;
	case 1:
		applyLODStrategy(1.0f, 0.5f);
		break;
	default:
		applyLODStrategy(1.0f, 1.0f);
		break;
	}
}

void Tile::applyLODStrategy(const float simplifyRatioFactor, const float textureFactor)
{
	if (config.simplifyRatio < 1.0)
	{
		const float sampleRatio = simplifyRatioFactor * config.simplifyRatio;
		float targetError = 0.01;
		if (this->lod == 2)
		{
			// targetError范围:0.2~0.5
			// 几何误差:1.0~5.0m
			targetError = 5.0 / this->diagonalLength;
			targetError = targetError > 0.3 ? 0.3 : targetError;
		}
		else if (this->lod == 1)
		{
			// targetError范围:0.05~0.1
			// 几何误差:0.2~1.0m
			targetError = 0.6 / this->diagonalLength;
			targetError = targetError > 0.075 ? 0.075 : targetError;
		}
		else
		{
			// targetError范围:0.01~0.03
			// 几何误差:0.1~0.01m
			targetError = 0.01 / this->diagonalLength;
		}
		if (this->lod != 0 && node.valid())
		{
			Simplifier simplifier(sampleRatio, false, false, targetError);
			node->accept(simplifier);
			this->lodError = simplifier.lodError * this->diagonalLength;
		}
	}

	config.gltfTextureOptions.maxTextureWidth *= textureFactor;
	config.gltfTextureOptions.maxTextureHeight *= textureFactor;
	config.gltfTextureOptions.maxTextureAtlasWidth *= textureFactor;
	config.gltfTextureOptions.maxTextureAtlasHeight *= textureFactor;
}
