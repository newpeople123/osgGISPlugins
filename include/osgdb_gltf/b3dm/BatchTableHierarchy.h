#ifndef OSG_GIS_PLUGINS_B3DM_BATCHTABLEHIERARCHY_H
#define OSG_GIS_PLUGINS_B3DM_BATCHTABLEHIERARCHY_H 1
#include <osg/NodeVisitor>
#include <nlohmann/json.hpp>
#include <osg/MixinVector>
#include <string>

using namespace nlohmann;
namespace osgGISPlugins
{
    const std::string NAME = "GeodeName";
    struct Class
    {
        std::string name;
        uint32_t length;
        json instances;

        json toJson() const
        {
            return json{
                {"name", this->name},
                {"length", this->length},
                {"instances", this->instances}
            };
        }
    };

    struct BatchTableHierarchy
    {
        osg::MixinVector<Class> classes;
        uint32_t instancesLength;
        osg::MixinVector<int32_t> classIds;
        osg::MixinVector<int32_t> parentCounts;
        osg::MixinVector<int32_t> parentIds;

        json toJson() const
        {
            json jClasses = json::array();
            for (const Class& cls : classes)
            {
                jClasses.push_back(cls.toJson());
            }
            if (parentCounts.size())
            {
                return json{
                    {"classes", jClasses},
                    {"instancesLength", this->instancesLength},
                    {"classIds", this->classIds},
                    {"parentCounts", this->parentCounts},
                    {"parentIds", this->parentIds}
                };
            }
            return json{
                    {"classes", jClasses},
                    {"instancesLength", this->instancesLength},
                    {"classIds", this->classIds},
                    {"parentIds", this->parentIds}
            };
        }
    };

    class BatchTableHierarchyVisitor :public osg::NodeVisitor
    {
    public:
        META_NodeVisitor(osgGISPlugins, BatchTableHierarchyVisitor)

            BatchTableHierarchyVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), _currentParentBatchId(0), _currentBatchId(0) {}

        void apply(osg::Geode& geode) override;
        void apply(osg::Group& group) override;
        void apply(osg::Drawable& drawable) override;

        const unsigned int getBatchLength() const { return _currentBatchId + 1; }

        const std::map<std::vector<std::string>, std::vector<unsigned int>> getAttributeNameBatchIdsMap() const
        {
            return _attributeNameBatchIdsMap;
        }

        const std::map<unsigned int, std::map<std::string, std::string>> getBatchIdAttributesMap() const
        {
            return _batchIdAttributesMap;
        }

        const std::map<unsigned int, unsigned int> getBatchParentIdMap() const
        {
            return _batchParentIdMap;
        }

    private:
        unsigned int _currentParentBatchId;
        std::map<unsigned int, unsigned int> _nodeHierarchyMap;
        unsigned int _currentBatchId;
        typedef std::map<std::string, std::string> Attributes;
        std::map<unsigned int, Attributes> _batchIdAttributesMap;
        std::map<unsigned int, unsigned int> _batchParentIdMap;
        typedef std::vector<std::string> StringVector;

        static bool areAttributeNamesEqual(const Attributes& attributes, const StringVector& attributeNames);
        static StringVector convertAttributesToVector(const Attributes& attributes);

        std::map<std::vector<std::string>, std::vector<unsigned int>> _attributeNameBatchIdsMap;
    };

}
#endif // !OSG_GIS_PLUGINS_B3DM_BATCHTABLEHIERARCHY_H
