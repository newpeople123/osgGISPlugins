#ifndef OSG_GIS_PLUGINS_B3DM_BATCHTABLEHIERARCHY_H
#define OSG_GIS_PLUGINS_B3DM_BATCHTABLEHIERARCHY_H 1

#include <nlohmann/json.hpp>
#include <osg/MixinVector>
#include <string>

using namespace nlohmann;

struct Class {
    std::string name;
    uint32_t length;
    json instances;

    json toJson() const {
        return json{
            {"name", this->name},
            {"length", this->length},
            {"instances", this->instances}
        };
    }
};

struct BatchTableHierarchy {
    osg::MixinVector<Class> classes;
    uint32_t instancesLength;
    osg::MixinVector<int32_t> classIds;
    osg::MixinVector<int32_t> parentCounts;
    osg::MixinVector<int32_t> parentIds;

    json toJson() const {
        json jClasses = json::array();
        for (const auto& cls : classes) {
            jClasses.push_back(cls.toJson());
        }
        if(parentCounts.size())
            return json{
                {"classes", jClasses},
                {"instancesLength", this->instancesLength},
                {"classIds", this->classIds},
                {"parentCounts", this->parentCounts},
                {"parentIds", this->parentIds}
            };
        return json{
                {"classes", jClasses},
                {"instancesLength", this->instancesLength},
                {"classIds", this->classIds},
                {"parentIds", this->parentIds}
        };
    }
};

#endif // !OSG_GIS_PLUGINS_B3DM_BATCHTABLEHIERARCHY_H
