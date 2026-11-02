#include "osgdb_gltf/b3dm/BatchTableHierarchy.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/ValueObject>
#include "osgdb_gltf/b3dm/UserDataVisitor.h"
using namespace osgGISPlugins;
void BatchTableHierarchyVisitor::apply(osg::Group& group) {
	if (!_sequentialBatchIds) {
		_currentParentBatchId = _currentBatchId == 0 ? 0 : _currentBatchId - 1;
	}
	traverse(group);
}

void BatchTableHierarchyVisitor::apply(osg::Geode& geode) {
	//fbx的最小单位对应osg::Geode
	if (geode.getNumDrawables()) {
		traverse(geode);

		UserDataVisitor userDataVisitor;
		const osg::ref_ptr<osg::UserDataContainer> userDataContainer = geode.getUserDataContainer();

		Attributes attributes;
		std::set<std::string> keys;
		if (userDataContainer.valid()) {

			for (unsigned int j = 0; j < userDataContainer->getNumUserObjects(); ++j)
			{
				osg::Object* ref = userDataContainer->getUserObject(j);
				if (ref) {
					osg::ValueObject* userObject = ref->asValueObject();
					if (userObject)
					{
						const std::string key = userObject->getName();
						keys.insert(key);
						userDataVisitor.clear();
						userObject->get(userDataVisitor);
						attributes.insert(std::make_pair(key, userDataVisitor.value()));
					}
				}
			}
		}

		std::string defaultName = geode.getName();
		bool hasNameKey = (keys.find(NAME) != keys.end());

		if (hasNameKey) {
			attributes[NAME] = userDataVisitor.value();
		}
		else {
			if (!defaultName.empty()) {
				attributes[NAME] = defaultName;
			}
			else {
				attributes[NAME] = "Unnamed_Geode";
			}
		}

		// i3dm：按 Geode（实例）遍历顺序编号，与特征表 BATCH_ID = 0..n-1 对齐。
		// 不能读 _BATCHID：实例几何共享同一份顶点数组，读到的是最后一个实例的编号。
		const unsigned batchId = _sequentialBatchIds ? _nextSequentialBatchId++ : _currentBatchId;
		_batchIdAttributesMap.insert(std::make_pair(batchId, attributes));

		const BatchTableHierarchyVisitor::StringVector attrNames = convertAttributesToVector(attributes);
		_attributeNameBatchIdsMap[attrNames].push_back(batchId);

		// Cesium 的层级遍历以"parentId == 自身"作为根哨兵；写 -1 会继续向上
		// 访问 classes[-1] 并抛 TypeError（实例属性读取失败）。顺序模式下每个
		// 实例都是根，父 id 指向自身。
		_batchParentIdMap.insert(std::make_pair(
			batchId, _sequentialBatchIds ? batchId : _currentParentBatchId));
	}
}

bool BatchTableHierarchyVisitor::areAttributeNamesEqual(const Attributes& attr, const StringVector& attrNames) {
	const StringVector keys = convertAttributesToVector(attr);
	std::set<std::string> keys1, keys2;
	keys1.insert(keys.begin(), keys.end());
	keys2.insert(attrNames.begin(), attrNames.end());
	return keys1 == keys2;
}

BatchTableHierarchyVisitor::StringVector BatchTableHierarchyVisitor::convertAttributesToVector(const Attributes& attr) {
	StringVector resultSet;
	for (const auto& pair : attr) {
		resultSet.push_back(pair.first);  // 插入键
	}
	return resultSet;
}

void BatchTableHierarchyVisitor::apply(osg::Drawable& drawable) {
	if (_sequentialBatchIds) {
		return;  // 顺序编号模式不依赖顶点属性里的 _BATCHID
	}
	osg::Geometry* geometry = drawable.asGeometry();
	if (geometry) {
		const osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geometry->getVertexAttribArray(0));
		if (batchIds.valid() && batchIds->size()) {
			_currentBatchId = batchIds->at(0);
		}
	}
}
