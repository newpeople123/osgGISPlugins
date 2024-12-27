#include "3dtiles/Tile.h"
#include <3dtiles/B3DMTile.h>
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

	if (j.contains("children")) {
		for (const auto& child : j.at("children")) {
			osg::ref_ptr<B3DMTile> childTile = new B3DMTile;
			childTile->fromJson(child);
			children.push_back(childTile);
		}
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

void Tile::setContentUri()
{
	if (this->node.valid() && this->node->asGroup()->getNumChildren())
	{
		
		if (type == "i3dm")
			contentUri = "InstanceTiles/Tile_L" + to_string(lod) + "_" + to_string(z) + "." + type;
		else
			contentUri = to_string(level) + "/" + "Tile_L" + to_string(lod) + "_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + "." + type;
	}
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
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		this->children[i]->computeGeometricError();
	}

	const auto& textureOptions = config.gltfTextureOptions;
	const int maxTextureResolution = osg::maximum(textureOptions.maxTextureWidth,
		textureOptions.maxTextureHeight);

	// 根据LOD层级确定纹理分辨率缩放因子
	double textureSizeFactor = 1.0;
	double CesiumGeometricErrorOperator = 0.0;
	if (this->node.valid())
	{

		// LOD2使用1/4分辨率，其他LOD使用1/2分辨率
		textureSizeFactor = this->lod == 2 ? 0.25 : 0.5;

		const double pixelSize = maxTextureResolution * textureSizeFactor;
		CesiumGeometricErrorOperator = getCesiumGeometricErrorOperatorByPixelSize(pixelSize);
	}
	else
	{
		CesiumGeometricErrorOperator = getCesiumGeometricErrorOperatorByPixelSize(InitPixelSize);
	}

	// 使用预缓存值计算加权误差
	double totalWeightedError = 0.0;
	double totalVolume = 0.0;
	for (const auto& child : this->children) {
		if (child->node.valid())
		{
			auto b3dmTile = dynamic_cast<B3DMTile*>(child.get());

			if (b3dmTile) {
				double geometricError = b3dmTile->diagonalLength * GEOMETRIC_ERROR_SCALE_FACTOR * CesiumGeometricErrorOperator;

				totalWeightedError += geometricError * b3dmTile->volume;
				totalVolume += b3dmTile->volume;
			}
			else
			{
				osg::ref_ptr<osg::Group> group = child->node->asGroup();
				if (group->getNumChildren())
				{
					osg::ComputeBoundsVisitor cbv;
					group->getChild(0)->accept(cbv);
					const osg::BoundingBox boundingBox = cbv.getBoundingBox();
					const float i3dmGeometricError = (boundingBox._max - boundingBox._min).length() * GEOMETRIC_ERROR_SCALE_FACTOR * CesiumGeometricErrorOperator;
					const float i3dmVolume = (boundingBox._max.x() - boundingBox._min.x()) *
						(boundingBox._max.y() - boundingBox._min.y()) *
						(boundingBox._max.z() - boundingBox._min.z());
					totalWeightedError += i3dmGeometricError * i3dmVolume;
					totalVolume += i3dmVolume;
				}
			}

		}
	}
	if (totalVolume > 0.0) {
		this->geometricError = totalWeightedError / totalVolume;
	}
	// 确保父瓦片的几何误差大于任何子瓦片
	for (const auto& child : this->children) {
		if (child->geometricError >= this->geometricError) {
			this->geometricError = child->geometricError + MIN_GEOMETRIC_ERROR_DIFFERENCE;
			OSG_NOTICE << "父子瓦片几何误差相差无几：父亲：level:" << this->level << ",x:" << this->x << ",y:" << this->y << ",z:" << this->z
				<< ";儿子：level:" << child->level << ",x:" << child->x << ",y:" << child->y << ",z:" << child->z << std::endl;
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
	else if(this->type=="b3dm")
		return this->geometricError == 0.0;
	return true;
}

double Tile::getCesiumGeometricErrorOperatorByPixelSize(const float pixelSize)
{
	    // 计算视锥体在单位距离处的高度
    const double frustumHeight = 2.0 * tan(CesiumFrustumFovy * 0.5);
    // 计算每像素对应的世界空间大小
    const double pixelWorldSize = frustumHeight / CesiumCanvasViewportHeight;
    // 计算几何误差系数
    return (CesiumMaxScreenSpaceError * pixelSize * pixelWorldSize);
}

void Tile::validate()
{
	for (size_t i = 0; i < this->children.size(); ++i)
	{
		osg::ref_ptr<Tile> tile = this->children[i].get();
		if (tile.valid() && tile->type != "i3dm")
		{
			if (tile->node->asGroup()->getNumChildren() == 0)
			{
				if (this->children[i]->children.size() == 0)
				{
					this->children.erase(this->children.begin() + i);
					i--;
					continue;
				}
			}
			tile->validate();
		}
	}
}
