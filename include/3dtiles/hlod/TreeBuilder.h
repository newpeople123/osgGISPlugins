#ifndef OSG_GIS_PLUGINS_TREE_BUILDER_H
#define OSG_GIS_PLUGINS_TREE_BUILDER_H
#include <osg/NodeVisitor>
#include <osg/PrimitiveSet>
#include <osg/Geometry>
#include "3dtiles/B3DMTile.h"
#include "3dtiles/I3DMTile.h"
#include "utils/Utils.h"
namespace osgGISPlugins
{

    class TreeBuilder :public osg::NodeVisitor
	{
    public:
        META_NodeVisitor(osgGISPlugins, TreeBuilder)

        TreeBuilder() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {};

        virtual osg::ref_ptr<B3DMTile> build();

        std::vector<osg::ref_ptr<osg::Geode>> _geodes;
        std::vector<osg::MatrixList> _matrixs;
        std::vector<std::vector<osg::ref_ptr<osg::UserDataContainer>>> _dataContainers;

        std::vector<osg::ref_ptr<osg::Geode>> _instanceGeodes;
        std::vector<osg::MatrixList> _instanceMatrixs;
        std::vector<std::vector<osg::ref_ptr<osg::UserDataContainer>>> _instanceDataContainers;

    protected:
        // 添加配置结构体
        struct BuilderConfig {
            size_t maxTriangleCount = 2.0e5;
            unsigned int maxTextureCount = 100;
            int maxLevel = 32;
            bool enableParallel = true;
        };
        BuilderConfig _config;

        osg::ref_ptr<osg::Group> _groupsToDivideList = new osg::Group;


        osg::Matrixd _currentMatrix;

        virtual void apply(osg::Group& group) override;

        virtual void apply(osg::Geode& geode) override;

        virtual osg::ref_ptr<B3DMTile> divideB3DM(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<B3DMTile> parent = nullptr, const int x = 0, const int y = 0, const int z = 0, const int level = 0);

        virtual void divideI3DM(std::vector<osg::ref_ptr<I3DMTile>>&, const osg::BoundingBox& bounds, osg::ref_ptr<I3DMTile> tile);

        virtual osg::ref_ptr<B3DMTile> generateB3DMTile();

        virtual osg::ref_ptr<I3DMTile> generateI3DMTile();

        void regroupI3DMTile(osg::ref_ptr<B3DMTile> b3dmTile, osg::ref_ptr<I3DMTile> i3dmTile);

        static osg::BoundingBox computeBoundingBox(const osg::ref_ptr<osg::Node> node);

        static double calculateBoundingBoxVolume(const osg::BoundingBox& box);

        static bool intersect(const osg::BoundingBox& tileBBox, const osg::BoundingBox& nodeBBox);

        static bool sortTileNodeByRadius(const osg::ref_ptr<Tile>& a, const osg::ref_ptr<Tile>& b);

        static bool sortNodeByRadius(const osg::ref_ptr<osg::Node>& a, const osg::ref_ptr<osg::Node>& b);

        void processOverSizedNodes();

        bool processGeometryWithMeshTextureLimit(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, const osg::ref_ptr<Tile> tile, const int level);
	};
}

#endif // !OSG_GIS_PLUGINS_TREE_BUILDER_H
