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
		contentUri = to_string(level) + "/" + "Tile_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + "." + type;
		if (type == "i3dm")
			contentUri = "InstanceTiles/" + contentUri;
	}
	else {
		return;
	}
}

void Tile::computeBoundingVolumeBox()
{
	osg::ref_ptr<osg::Group> group = getAllDescendantNodes();
	boundingVolume.computeBox(group);
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
