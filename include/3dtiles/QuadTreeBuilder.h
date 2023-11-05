#ifndef OSG_GIS_PLUGINS_QUADTREEBUILDER
#define OSG_GIS_PLUGINS_QUADTREEBUILDER 1

#include "TreeBuilder.h"

class QuadTreeBuilder:public TreeBuilder
{
public:
    QuadTreeBuilder(osg::ref_ptr<osg::Node> node) :TreeBuilder() {
        RebuildDataNodeVisitor* rdnv = new RebuildDataNodeVisitor(node);
        osg::BoundingBox totalBoundingBox = getBoundingBox(rdnv->output);
        rootTreeNode = buildTree(totalBoundingBox, rdnv->output);
        buildHlod(rootTreeNode);
    }
    QuadTreeBuilder(osg::ref_ptr<osg::Node> node,const unsigned int maxTriangleNumber,const int maxTreeDepth,const double simpleRatio) :TreeBuilder(maxTriangleNumber,maxTreeDepth,simpleRatio) {
        RebuildDataNodeVisitor* rdnv = new RebuildDataNodeVisitor(node);
        osg::BoundingBox totalBoundingBox = getBoundingBox(rdnv->output);
        rootTreeNode = buildTree(totalBoundingBox, rdnv->output);
        buildHlod(rootTreeNode);
    }
    ~QuadTreeBuilder() {};

protected:
 
    osg::ref_ptr<TileNode> buildTree(const osg::BoundingBox& total, const osg::ref_ptr<osg::Group>& inputRoot, int x = 0, int y = 0, int z = 0, osg::ref_ptr<TileNode> parent = nullptr, int depth = 0) {

        if (total.valid()) {
            int s[2];
            osg::Vec3f extentSet[3] = {
                total._min,
                (total._max + total._min) * 0.5f,
                total._max
            };
            osg::ref_ptr<osg::Group> childData = new osg::Group;

            for (unsigned int i = 0; i < inputRoot->getNumChildren(); ++i)
            {
                const osg::ref_ptr<osg::Node>& obj = inputRoot->getChild(i);
                obj->dirtyBound();
                osg::BoundingBox childBox = getBoundingBox(obj);
                if (total.contains(childBox._min) && total.contains(childBox._max))
                    childData->addChild(obj);
                else if (total.intersects(childBox))
                {
                    osg::Vec3f center = (childBox._max + childBox._min) * 0.5f;
                    if (total.contains(center))
                    {
                        childData->addChild(obj);
                    }
                }
            }
            unsigned int triangleNumber = 0;
            TriangleNumberNodeVisitor tnnv;
            childData->accept(tnnv);
            triangleNumber += tnnv.count;
            if (triangleNumber == 0) {
                return NULL;
            }
            bool isLeafNode = false;
            if (triangleNumber <= _maxTriangleNumber || depth > _maxTreeDepth)
            {
                isLeafNode = true;
            }


            osg::ref_ptr<TileNode> root = new TileNode;
            root->parentTreeNode = parent;
            root->level = depth;
            root->x = x;
            root->y = y;
            root->z = z;

            if (!isLeafNode)
            {
                osg::ref_ptr<osg::Group> childNodes = new osg::Group;

                for (s[0] = 0; s[0] < 2; ++s[0]) //x
                {
                    for (s[1] = 0; s[1] < 2; ++s[1]) //y
                    {
                        // Calculate the child extent
                        osg::Vec3f min, max;
                        for (int a = 0; a < 2; ++a)
                        {
                            min[a] = (extentSet[s[a] + 0])[a];
                            max[a] = (extentSet[s[a] + 1])[a];
                        }
                        min[2] = total._min.z();
                        max[2] = total._max.z();

                        int id = s[0] + (2 * s[1]);
                        osg::ref_ptr<TileNode> childTreeNode = buildTree(osg::BoundingBox(min, max), childData, root->x * 2 + s[0], root->y * 2 + s[1], root->z , root, depth + 1);
                        if (childTreeNode != NULL)
                            childNodes->addChild(childTreeNode);
                    }
                }

                for (unsigned int i = 0; i < childNodes->getNumChildren(); ++i)
                {
                    osg::ref_ptr<TileNode> childTreeNode = dynamic_cast<TileNode *>(childNodes->getChild(i));
                    if (childTreeNode->children->getNumChildren() > 0 || childTreeNode->currentNodes->getNumChildren() > 0) {
                        root->children->addChild(childTreeNode);
                        for (unsigned int j = 0; j < childTreeNode->nodes->getNumChildren(); j++) {
                            root->nodes->addChild(childTreeNode->nodes->getChild(j));
                        }
                    }
                }
            }
            else
            {
                for (unsigned int i = 0; i < childData->getNumChildren(); ++i)
                {
                    const osg::ref_ptr<osg::Node>& obj = childData->getChild(i);

                    root->nodes->addChild(obj);
                    root->currentNodes->addChild(obj);
                }
            }

            return root;
        }
        return NULL;
    }
};
#endif // !OSG_GIS_PLUGINS_QUADTREEBUILDER
