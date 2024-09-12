#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osg/NodeVisitor>
#include <osg/ShapeDrawable>
#include <osgViewer/Viewer>
#ifdef _WIN32
#include <windows.h>
#endif
#include <utils/GltfOptimizer.h>
#include <osg/ComputeBoundsVisitor>
#include <osg/LineWidth>
#include <3dtiles/Tileset.h>
#include <3dtiles/hlod/QuadtreeBuilder.h>
using namespace std;
using namespace osgGISPlugins;
//const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.27.0\html\test\gltf\)";
//const std::string INPUT_BASE_PATH = R"(E:\Data\data\)";

const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.22.1\html\gltf\)";
const std::string INPUT_BASE_PATH = R"(E:\Code\2023\Other\data\)";

osg::ref_ptr<Tile> convertOsgGroup2Tile(osg::ref_ptr<osg::Group> group, osg::ref_ptr<Tile> parent = nullptr);
void computedTreeDepth(osg::ref_ptr<osg::Node> node, int& depth);
osg::Geometry* createBoundingBoxGeometry(const osg::BoundingBox& bbox);
int getMaxDepth(osg::Node* node);
void buildTree(const std::string& filename);
osg::ref_ptr<Tileset> convertOsgNode2Tileset(osg::ref_ptr<osg::Node> node);

class GroupCountVisitor :public osg::NodeVisitor
{
public:
    GroupCountVisitor() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}
    void apply(osg::Group& group) override
    {
        int count = 0;
        for (size_t i = 0; i < group.getNumChildren(); ++i)
        {
            if (typeid(*group.getChild(i)) == typeid(osg::Geode)|| typeid(*group.getChild(i)) == typeid(osg::Drawable))
            {
                count++;
            }
        }
        //if(count)
        //    std::cout << count << ", ";
        if(count>1)
            std::cout << count << ", ";
        traverse(group);
    }
};

void computedTreeDepth(osg::ref_ptr<osg::Node> node, int& depth)
{
    if (node.valid())
    {
        depth++;
        osg::ref_ptr<osg::Group> group = node->asGroup();
        if (group.valid())
        {
            for (size_t i = 0; i < group->getNumChildren(); ++i)
            {
                int temp = depth;
                computedTreeDepth(group->getChild(i), temp);
                depth = osg::maximum(temp, depth);
            }
        }
    }
}

osg::Geometry* createBoundingBoxGeometry(const osg::BoundingBox& bbox) {
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array(8);
    (*vertices)[0] = osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMin());
    (*vertices)[1] = osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMin());
    (*vertices)[2] = osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMin());
    (*vertices)[3] = osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMin());
    (*vertices)[4] = osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMax());
    (*vertices)[5] = osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMax());
    (*vertices)[6] = osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMax());
    (*vertices)[7] = osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMax());

    osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(GL_LINES);
    indices->push_back(0); indices->push_back(1);
    indices->push_back(1); indices->push_back(2);
    indices->push_back(2); indices->push_back(3);
    indices->push_back(3); indices->push_back(0);
    indices->push_back(4); indices->push_back(5);
    indices->push_back(5); indices->push_back(6);
    indices->push_back(6); indices->push_back(7);
    indices->push_back(7); indices->push_back(4);
    indices->push_back(0); indices->push_back(4);
    indices->push_back(1); indices->push_back(5);
    indices->push_back(2); indices->push_back(6);
    indices->push_back(3); indices->push_back(7);

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setVertexArray(vertices);
    geom->addPrimitiveSet(indices);

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
    colors->push_back(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));  // 红色
    geom->setColorArray(colors, osg::Array::BIND_OVERALL);

    geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(2.0f), osg::StateAttribute::ON);

    return geom.release();
}

int getMaxDepth(osg::Node* node) {
    if (!node) return 0; // 如果节点为空，返回0
    if (typeid(*node) == typeid(osg::Geode))
        return 1;
    osg::Group* group = node->asGroup();
    if (!group) return 1; // 如果节点不是Group，返回1（当前节点深度）

    vector<osg::ref_ptr<osg::Geode>> geodes;
    int maxDepth = 0;
    for (unsigned int i = 0; i < group->getNumChildren(); ++i) {
        // 获取每个子节点的深度
        int childDepth = getMaxDepth(group->getChild(i));

        osg::ComputeBoundsVisitor cbVisitor;
        node->accept(cbVisitor);
        osg::BoundingBox bbox = cbVisitor.getBoundingBox();
        osg::ref_ptr<osg::Geode> geode = new osg::Geode();

        geode->addDrawable(createBoundingBoxGeometry(bbox));
        geodes.push_back(geode);

        // 更新最大深度
        if (childDepth > maxDepth) {
            maxDepth = childDepth;
        }
    }
    for (auto item : geodes) {
        group->addChild(item);
    }
    return maxDepth + 1; // 加1表示包括当前Group节点的深度
}

void buildTree(const std::string& filename)
{
    osg::ref_ptr<osgDB::Options> options = new osgDB::Options;
    //options->setOptionString("TessellatePolygons");
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(INPUT_BASE_PATH + filename + R"(.fbx)", options.get());

    osg::BoundingBox bb;
    bb.expandBy(node->getBound());
    const double xLength = bb.xMax() - bb.xMin();
    const double yLength = bb.yMax() - bb.yMin();
    const double zLength = bb.zMax() - bb.zMin();
    const double max = osg::maximum(osg::maximum(xLength, yLength), zLength);

    GroupCountVisitor gcv;
    node->accept(gcv);
    std::cout << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    GltfOptimizer gltfOptimizer;
    gltfOptimizer.optimize(node, osgUtil::Optimizer::INDEX_MESH);
    QuadtreeBuilder builder;
    node->accept(builder);
    osg::ref_ptr<Tile> root = builder.build();
    root->buildHlod();
    osg::ref_ptr<Tileset> tileset = new Tileset;
    tileset->root = root;
    tileset->computeGeometricError(node);
    GltfOptimizer::GltfTextureOptimizationOptions gltfTextureOptions;
    gltfTextureOptions.maxTextureAtlasWidth = 2048;
    gltfTextureOptions.maxTextureAtlasHeight = 2048;
    gltfTextureOptions.ext = ".jpg";
    tileset->root->write(R"(D:\nginx-1.22.1\html\3dtiles\tet4)", 0.5, gltfTextureOptions);
    tileset->computeTransform(116, 30, 100);
    tileset->toFile(R"(D:\nginx-1.22.1\html\3dtiles\tet4\tileset.json)");

}

osg::ref_ptr<Tile> convertOsgGroup2Tile(osg::ref_ptr<osg::Group> group,osg::ref_ptr<Tile> parent)
{
    osg::ref_ptr<Tile> tile = new Tile(group,parent);

    if (group.valid())
    {
        if (typeid(*group) != typeid(osg::Geode))
        {
            for (size_t i = 0; i < group->getNumChildren(); ++i)
            {
                osg::ref_ptr<Tile> childTile = convertOsgGroup2Tile(group->getChild(i)->asGroup(), tile);
                tile->children.push_back(childTile);
            }
        }
    }
    

    return tile.release();
}

osg::ref_ptr<Tileset> convertOsgNode2Tileset(osg::ref_ptr<osg::Node> node)
{
    osg::ref_ptr<Tileset> tileset = new Tileset;
    osg::ref_ptr<Tile> rootTile = convertOsgGroup2Tile(node->asGroup());
    tileset->root = rootTile;
    return tileset.release();
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
    osgDB::Registry* instance = osgDB::Registry::instance();
    instance->addFileExtensionAlias("glb", "gltf");//插件注册别名
    instance->addFileExtensionAlias("b3dm", "gltf");//插件注册别名
    instance->addFileExtensionAlias("i3dm", "gltf");//插件注册别名
    instance->addFileExtensionAlias("ktx2", "ktx");//插件注册别名

    buildTree(R"(20240529卢沟桥分洪枢纽)");//芜湖水厂总装单位M  20240529卢沟桥分洪枢纽
    //OSG_NOTICE << R"(龙翔桥站厅处理完毕)" << std::endl;
    return 1;
}
