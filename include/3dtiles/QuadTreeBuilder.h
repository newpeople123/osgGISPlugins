#ifndef OSG_GIS_PLUGINS_QUADTREEBUILDER
#define OSG_GIS_PLUGINS_QUADTREEBUILDER 1

#include "TreeBuilder.h"

class QuadTreeBuilder:public TreeBuilder
{
public:
    QuadTreeBuilder(osg::ref_ptr<osg::Node> node) :TreeBuilder() {
        RebuildDataNodeVisitor rdnv;
        node->accept(rdnv);
        osg::ref_ptr<osg::Group> group = rdnv.output;

        osg::BoundingBox totalBoundingBox = getBoundingBox(group);
        rootTreeNode = buildTree(totalBoundingBox, group);
        buildHlod(rootTreeNode);
    }
    QuadTreeBuilder(osg::ref_ptr<osg::Node> node,const unsigned int maxTriangleNumber,const int maxTreeDepth,const int simpleRatio) :TreeBuilder(maxTriangleNumber,maxTreeDepth,simpleRatio) {
        RebuildDataNodeVisitor rdnv;
        node->accept(rdnv);
        osg::ref_ptr<osg::Group> group = rdnv.output;

        osg::BoundingBox totalBoundingBox = getBoundingBox(group);
        rootTreeNode = buildTree(totalBoundingBox, group);
        buildHlod(rootTreeNode);
    }
    ~QuadTreeBuilder() {};

protected:
    osg::ref_ptr<TreeNode> buildTree(const osg::BoundingBox& total, const osg::ref_ptr<osg::Group>& inputRoot, int parentX = 0, int parentY = 0, int parentZ = 0, osg::ref_ptr<TreeNode> parent = nullptr, int depth = 0) {
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
                osg::MatrixList matrixList = obj->getWorldMatrices();
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
            for (unsigned int i = 0; i < childData->getNumChildren(); ++i) {
                osg::ref_ptr<osg::Geode> geode = childData->getChild(i)->asGeode();
                osg::ref_ptr<osg::Transform> transform = childData->getChild(i)->asTransform();
                if (geode) {
                    TriangleNumberNodeVisitor tnnv;
                    geode->accept(tnnv);
                    triangleNumber += tnnv.count;
                }
                else if (transform) {
                    for (unsigned int j = 0; j < transform->getNumChildren(); ++j) {
                        osg::ref_ptr<osg::Geode> geode = transform->getChild(j)->asGeode();
                        if (geode) {
                            TriangleNumberNodeVisitor tnnv;
                            geode->accept(tnnv);
                            triangleNumber += tnnv.count;
                        }
                    }
                }
            }
            if (triangleNumber == 0) {
                return NULL;
            }
            bool isLeafNode = false;
            if (triangleNumber <= _maxTriangleNumber || depth > _maxTreeDepth)
            {
                isLeafNode = true;
            }


            osg::ref_ptr<TreeNode> root = new TreeNode;
            root->parentTreeNode = parent;
            root->level = depth;
            root->x = parentX;
            root->y = parentY;
            root->z = parentZ;

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
                        osg::ref_ptr<TreeNode> childTreeNode = buildTree(osg::BoundingBox(min, max), childData, root->x + s[0], root->y * 2 + s[1], root->z + 1, root, depth + 1);
                        if (childTreeNode != NULL)
                            childNodes->addChild(childTreeNode);
                    }
                }

                for (unsigned int i = 0; i < childNodes->getNumChildren(); ++i)
                {
                    osg::ref_ptr<TreeNode> childTreeNode = dynamic_cast<TreeNode *>(childNodes->getChild(i));
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
