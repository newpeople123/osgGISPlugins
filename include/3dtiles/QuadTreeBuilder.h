#ifndef OSG_GIS_PLUGINS_QUADTREEBUILDER
#define OSG_GIS_PLUGINS_QUADTREEBUILDER 1

#include "TreeBuilder.h"

class QuadTreeBuilder:public TreeBuilder
{
public:
    QuadTreeBuilder(const osg::ref_ptr<osg::Node>& node) {
	    const RebuildDataNodeVisitorProxy* rdnv = new RebuildDataNodeVisitorProxy(node);
	    const osg::BoundingBox totalBoundingBox = GetBoundingBox(rdnv->output);
        rootTreeNode = buildTree(totalBoundingBox, rdnv->output);
        //buildHlod(rootTreeNode);
    }
    QuadTreeBuilder(const osg::ref_ptr<osg::Node>& node,const unsigned int maxTriangleNumber,const int maxTreeDepth) :TreeBuilder(maxTriangleNumber,maxTreeDepth-1) {
    	const RebuildDataNodeVisitorProxy* rdnv = new RebuildDataNodeVisitorProxy(node);
	    const osg::BoundingBox totalBoundingBox = GetBoundingBox(rdnv->output);
        rootTreeNode = buildTree(totalBoundingBox, rdnv->output);
        //buildHlod(rootTreeNode);
    }
    ~QuadTreeBuilder() override = default;

protected:
 
    osg::ref_ptr<TileNode> buildTree(const osg::BoundingBox& total, const osg::ref_ptr<osg::Group>& inputRoot, const int x = 0, const int y = 0, const int z = 0, const osg::ref_ptr<TileNode>& parent = nullptr, const int depth = 0) {

        if (total.valid()) {
            int s[2];
            osg::Vec3f extentSet[3] = {
                total._min,
                (total._max + total._min) * 0.5f,
                total._max
            };
            const osg::ref_ptr<osg::Group> childData = new osg::Group;

            for (unsigned int i = 0; i < inputRoot->getNumChildren(); ++i)
            {
                const osg::ref_ptr<osg::Node>& obj = inputRoot->getChild(i);
                osg::BoundingBox childBox = GetBoundingBox(obj);
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
                return nullptr;
            }

            ComputeTextureSizeVisitor ctsv;
            childData->accept(ctsv);
            bool isLeafNode = false;
            const unsigned int num = childData->getNumChildren();
            if ((triangleNumber <= _maxTriangleNumber && ctsv.size <= 20) || depth >= _maxTreeDepth || num == 1)
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
	            const osg::ref_ptr<osg::Group> childNodes = new osg::Group;

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
                        if (childTreeNode != nullptr)
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
                root->size = ctsv.size;
            }

            return root;
        }
        return nullptr;
    }
};
#endif // !OSG_GIS_PLUGINS_QUADTREEBUILDER
