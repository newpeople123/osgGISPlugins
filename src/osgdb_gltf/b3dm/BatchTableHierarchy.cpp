#include "osgdb_gltf/b3dm/BatchTableHierarchy.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/ValueObject>
#include "osgdb_gltf/b3dm/UserDataVisitor.h"
using namespace osgGISPlugins;
void BatchTableHierarchyVisitor::apply(osg::Group& group) {
    _currentParentBatchId = _currentBatchId == 0 ? 0 : _currentBatchId - 1;
    traverse(group);
}

void BatchTableHierarchyVisitor::apply(osg::Geode& geode) {
    //fbx的最小单位对应osg::Geode
    if (geode.getNumDrawables()) {
        traverse(geode);
        UserDataVisitor userDataVisitor;
        const osg::ref_ptr<osg::UserDataContainer> userDataContainer = geode.getUserDataContainer();
        if (userDataContainer.valid()) {
            Attributes attributes;

            std::set<std::string> keys;
            for (unsigned int j = 0; j < userDataContainer->getNumUserObjects(); ++j)
            {
                osg::ValueObject* userObject = userDataContainer->getUserObject(j)->asValueObject();

                const std::string key = userObject->getName();
                keys.insert(key);
                std::string val;
                userDataVisitor.clear();
                userObject->get(userDataVisitor);

                attributes.insert(std::make_pair(key, userDataVisitor.value()));
            }
            bool isNameAdded = false;
            unsigned int index = 0;
            while (!isNameAdded)
            {
                const std::string name = index == 0 ? "name" : "name" + std::to_string(index);
                if (keys.find(name) == keys.end()) {
                    attributes.insert(std::make_pair(name, geode.getName()));
                    isNameAdded = true;
                }
                index++;
            }

            _batchIdAttributesMap.insert(std::make_pair(_currentBatchId, attributes));

            const BatchTableHierarchyVisitor::StringVector attrNames = convertAttributesToVector(attributes);
            _attributeNameBatchIdsMap[attrNames].push_back(_currentBatchId);
        }

        _batchParentIdMap.insert(std::make_pair(_currentBatchId, _currentParentBatchId));
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
    osg::Geometry* geometry = drawable.asGeometry();
    if (geometry) {
        const osg::ref_ptr<osg::FloatArray> batchIds = dynamic_cast<osg::FloatArray*>(geometry->getVertexAttribArray(0));
        if (batchIds.valid()) {
            _currentBatchId = batchIds->at(0);
        }
    }
}
