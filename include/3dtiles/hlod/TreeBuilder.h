#ifndef OSG_GIS_PLUGINS_TREE_BUILDER_H
#define OSG_GIS_PLUGINS_TREE_BUILDER_H
#include <osg/NodeVisitor>
#include <osg/PrimitiveSet>
#include <osg/Geometry>
#include "3dtiles/B3DMTile.h"
#include "3dtiles/I3DMTile.h"

namespace osgGISPlugins
{

    class TreeBuilder :public osg::NodeVisitor
	{
    public:
        struct BuilderConfig {
        public:
            const unsigned int InitDrawcallCommandCount = 5;
            void setMaxTriagnleCount(const unsigned int val)
            {
                maxTriangleCount = val;
            }
            void setMaxDrawcallCommandCount(const unsigned int val)
            {
                maxDrawcallCommandCount = val < InitDrawcallCommandCount ? InitDrawcallCommandCount : val;
            }
            void setMaxLevel(const unsigned int val)
            {
                maxLevel = val;
            }
            unsigned int getMaxTriangleCount() const {
                return maxTriangleCount;
            }
            unsigned int getMaxDrawcallCommandCount() const {
                return maxDrawcallCommandCount;
            }
            int getMaxLevel() const {
                return maxLevel;
            }
        private:
            unsigned int maxTriangleCount = 2.0e5;
            unsigned int maxDrawcallCommandCount = 20;
            int maxLevel = 32;
        };
    protected:
        BuilderConfig _config;

        osg::ref_ptr<osg::Group> _groupsToDivideList = new osg::Group;

        osg::Matrix _currentMatrix;
        std::vector<osg::Matrix> _matrixStack;
    public:
        META_NodeVisitor(osgGISPlugins, TreeBuilder)

        TreeBuilder() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {};

		TreeBuilder(const BuilderConfig config) :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN), _config(config) {};

        virtual osg::ref_ptr<B3DMTile> build();

        std::vector<osg::ref_ptr<osg::Geode>> _geodes;
        std::vector<osg::MatrixList> _matrixs;
        std::vector<std::vector<osg::ref_ptr<osg::UserDataContainer>>> _dataContainers;

        std::vector<osg::ref_ptr<osg::Geode>> _instanceGeodes;
        std::vector<osg::MatrixList> _instanceMatrixs;
        std::vector<std::vector<osg::ref_ptr<osg::UserDataContainer>>> _instanceDataContainers;

    protected:
        void pushMatrix(const osg::Matrix& matrix);

        void popMatrix();

        void apply(osg::Transform& transform) override;

        void apply(osg::Geode& geode) override;

        virtual osg::ref_ptr<B3DMTile> divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent = nullptr, int x = 0, int y = 0, int z = 0, int level = 0);

        virtual void divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>&, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile);

        virtual osg::ref_ptr<B3DMTile> generateB3DMTile();

        virtual osg::ref_ptr<I3DMTile> generateI3DMTile();

        static void regroupI3DMTile(const osg::ref_ptr<B3DMTile>& b3dmTile, const osg::ref_ptr<I3DMTile>& i3dmTile);

        static osg::BoundingBox computeBoundingBox(const osg::ref_ptr<osg::Node>& node);

        static double calculateBoundingBoxVolume(const osg::BoundingBox& box);

        static bool intersect(const osg::BoundingBox& tileBBox, const osg::BoundingBox& nodeBBox);

        static bool sortTileNodeByRadius(const osg::ref_ptr<Tile>& a, const osg::ref_ptr<Tile>& b);

        static bool sortNodeByRadius(const osg::ref_ptr<osg::Node>& a, const osg::ref_ptr<osg::Node>& b);

        void processOverSizedNodes();

        bool processB3DMWithMeshDrawcallCommandLimit(const osg::ref_ptr<osg::Group>& group, const osg::BoundingBox& bounds, const osg::ref_ptr<B3DMTile>& tile) const;

        static bool processI3DM(std::vector<osg::ref_ptr<I3DMTile>>& group, const osg::BoundingBox& bounds, const osg::ref_ptr<I3DMTile>& tile);

	};
}

#endif // !OSG_GIS_PLUGINS_TREE_BUILDER_H
