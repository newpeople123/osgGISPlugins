#include <iostream>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osg/NodeVisitor>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;
//const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.27.0\html\test\gltf\)";
//const std::string INPUT_BASE_PATH = R"(E:\Data\data\)";

const std::string OUTPUT_BASE_PATH = R"(D:\nginx-1.22.1\html\gltf\)";
const std::string INPUT_BASE_PATH = R"(E:\Code\2023\Other\data\)";

enum class SpatialTreeType
{
    QUADTREE,
    OCTREE
};
class SpatializeGroupsVisitor :public osg::NodeVisitor
{
private:
    SpatialTreeType _treeType;

    bool divide(osg::Group* group, unsigned int maxNumTreesPerCell);
    bool divide(osg::Geode* geode, unsigned int maxNumTreesPerCell);

    void apply(osg::Group& group) override;
    void apply(osg::Geode& geode) override;

    typedef std::set<osg::Group*> GroupsToDivideList;
    GroupsToDivideList _groupsToDivideList;

    typedef std::set<osg::Geode*> GeodesToDivideList;
    GeodesToDivideList _geodesToDivideList;
public:

    SpatializeGroupsVisitor(SpatialTreeType treeType=SpatialTreeType::OCTREE):osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN), _treeType(treeType) {}
    /// <summary>
    /// 
    /// </summary>
    /// <param name="maxNumTreesPerCell">group和geode节点的子节点最大数量</param>
    /// <returns></returns>
    bool divide(unsigned int maxNumTreesPerCell = 4);
};

void SpatializeGroupsVisitor::apply(osg::Group& group)
{
    if (typeid(group) == typeid(osg::Group) || group.asTransform())
    {
        _groupsToDivideList.insert(&group);
    }
    traverse(group);
}

void SpatializeGroupsVisitor::apply(osg::Geode& geode)
{
    if (typeid(geode) == typeid(osg::Geode))
    {
        _geodesToDivideList.insert(&geode);
    }
    traverse(geode);
}

bool SpatializeGroupsVisitor::divide(unsigned int maxNumTreesPerCell)
{
    bool divided = false;
    for (GroupsToDivideList::iterator itr = _groupsToDivideList.begin();
        itr != _groupsToDivideList.end();
        ++itr)
    {
        if (divide(*itr, maxNumTreesPerCell)) divided = true;
    }

    for (GeodesToDivideList::iterator geode_itr = _geodesToDivideList.begin();
        geode_itr != _geodesToDivideList.end();
        ++geode_itr)
    {
        if (divide(*geode_itr, maxNumTreesPerCell)) divided = true;
    }

    return divided;
}

bool SpatializeGroupsVisitor::divide(osg::Group* group, unsigned int maxNumTreesPerCell)
{
    if (group->getNumChildren() <= maxNumTreesPerCell) return false;

    // create the original box.
    osg::BoundingBox bb;
    for (unsigned int i = 0; i < group->getNumChildren(); ++i)
    {
        bb.expandBy(group->getChild(i)->getBound().center());
    }

    float radius = bb.radius();
    float divide_distance = radius * 0.7f;
    bool xAxis = (bb.xMax() - bb.xMin()) > divide_distance;
    bool yAxis = (bb.yMax() - bb.yMin()) > divide_distance;
    bool zAxis = (bb.zMax() - bb.zMin()) > divide_distance;

    OSG_INFO << "Dividing " << group->className() << "  num children = " << group->getNumChildren() << "  xAxis=" << xAxis << "  yAxis=" << yAxis << "   zAxis=" << zAxis << std::endl;
    if (_treeType == SpatialTreeType::OCTREE)
    {
        if (!xAxis && !yAxis && !zAxis)
        {
            OSG_INFO << "  No axis to divide, stopping division." << std::endl;
            return false;
        }
    }
    else
    {
        if (!xAxis && !yAxis)
        {
            OSG_INFO << "  No axis to divide, stopping division." << std::endl;
            return false;
        }
    }
    unsigned int numChildrenOnEntry = group->getNumChildren();

    typedef std::pair< osg::BoundingBox, osg::ref_ptr<osg::Group> > BoxGroupPair;
    typedef std::vector< BoxGroupPair > Boxes;
    Boxes boxes;
    boxes.push_back(BoxGroupPair(bb, new osg::Group));

    // divide up on each axis
    if (xAxis)
    {
        unsigned int numCellsToDivide = boxes.size();
        for (unsigned int i = 0; i < numCellsToDivide; ++i)
        {
            osg::BoundingBox& orig_cell = boxes[i].first;
            osg::BoundingBox new_cell = orig_cell;

            float xCenter = (orig_cell.xMin() + orig_cell.xMax()) * 0.5f;
            orig_cell.xMax() = xCenter;
            new_cell.xMin() = xCenter;

            boxes.push_back(BoxGroupPair(new_cell, new osg::Group));
        }
    }

    if (yAxis)
    {
        unsigned int numCellsToDivide = boxes.size();
        for (unsigned int i = 0; i < numCellsToDivide; ++i)
        {
            osg::BoundingBox& orig_cell = boxes[i].first;
            osg::BoundingBox new_cell = orig_cell;

            float yCenter = (orig_cell.yMin() + orig_cell.yMax()) * 0.5f;
            orig_cell.yMax() = yCenter;
            new_cell.yMin() = yCenter;

            boxes.push_back(BoxGroupPair(new_cell, new osg::Group));
        }
    }
    if (_treeType == SpatialTreeType::OCTREE)
    {
        if (zAxis)
        {
            unsigned int numCellsToDivide = boxes.size();
            for (unsigned int i = 0; i < numCellsToDivide; ++i)
            {
                osg::BoundingBox& orig_cell = boxes[i].first;
                osg::BoundingBox new_cell = orig_cell;

                float zCenter = (orig_cell.zMin() + orig_cell.zMax()) * 0.5f;
                orig_cell.zMax() = zCenter;
                new_cell.zMin() = zCenter;

                boxes.push_back(BoxGroupPair(new_cell, new osg::Group));
            }
        }
    }


    // create the groups to drop the children into


    // bin each child into associated bb group
    typedef std::vector< osg::ref_ptr<osg::Node> > NodeList;
    NodeList unassignedList;
    for (unsigned int i = 0; i < group->getNumChildren(); ++i)
    {
        bool assigned = false;
        osg::Vec3 center = group->getChild(i)->getBound().center();
        for (Boxes::iterator itr = boxes.begin();
            itr != boxes.end() && !assigned;
            ++itr)
        {
            if (itr->first.contains(center))
            {
                // move child from main group into bb group.
                (itr->second)->addChild(group->getChild(i));
                assigned = true;
            }
        }
        if (!assigned)
        {
            unassignedList.push_back(group->getChild(i));
        }
    }


    // now transfer nodes across, by :
    //      first removing from the original group,
    //      add in the bb groups
    //      add then the unassigned children.


    // first removing from the original group,
    group->removeChildren(0, group->getNumChildren());

    // add in the bb groups
    typedef std::vector< osg::ref_ptr<osg::Group> > GroupList;
    GroupList groupsToDivideList;
    for (Boxes::iterator itr = boxes.begin();
        itr != boxes.end();
        ++itr)
    {
        // move child from main group into bb group.
        osg::Group* bb_group = (itr->second).get();
        if (bb_group->getNumChildren() > 0)
        {
            if (bb_group->getNumChildren() == 1)
            {
                group->addChild(bb_group->getChild(0));
            }
            else
            {
                group->addChild(bb_group);
                if (bb_group->getNumChildren() > maxNumTreesPerCell)
                {
                    groupsToDivideList.push_back(bb_group);
                }
            }
        }
    }


    // add then the unassigned children.
    for (NodeList::iterator nitr = unassignedList.begin();
        nitr != unassignedList.end();
        ++nitr)
    {
        group->addChild(nitr->get());
    }

    // now call divide on all groups that require it.
    for (GroupList::iterator gitr = groupsToDivideList.begin();
        gitr != groupsToDivideList.end();
        ++gitr)
    {
        divide(gitr->get(), maxNumTreesPerCell);
    }

    return (numChildrenOnEntry < group->getNumChildren());
}

bool SpatializeGroupsVisitor::divide(osg::Geode* geode, unsigned int maxNumTreesPerCell)
{
    if (geode->getNumDrawables() <= maxNumTreesPerCell) return false;

    // create the original box.
    osg::BoundingBox bb;
    unsigned int i;
    for (i = 0; i < geode->getNumDrawables(); ++i)
    {
        bb.expandBy(geode->getDrawable(i)->getBoundingBox().center());
    }

    float radius = bb.radius();
    float divide_distance = radius * 0.7f;
    bool xAxis = (bb.xMax() - bb.xMin()) > divide_distance;
    bool yAxis = (bb.yMax() - bb.yMin()) > divide_distance;
    bool zAxis = (bb.zMax() - bb.zMin()) > divide_distance;

    OSG_INFO << "INFO " << geode->className() << "  num drawables = " << geode->getNumDrawables() << "  xAxis=" << xAxis << "  yAxis=" << yAxis << "   zAxis=" << zAxis << std::endl;


    if (_treeType == SpatialTreeType::OCTREE)
    {
        if (!xAxis && !yAxis && !zAxis)
        {
            OSG_INFO << "  No axis to divide, stopping division." << std::endl;
            return false;
        }
    }
    else
    {
        if (!xAxis && !yAxis)
        {
            OSG_INFO << "  No axis to divide, stopping division." << std::endl;
            return false;
        }
    }

    osg::Node::ParentList parents = geode->getParents();
    if (parents.empty())
    {
        OSG_INFO << "  Cannot perform spatialize on root Geode, add a Group above it to allow subdivision." << std::endl;
        return false;
    }

    osg::ref_ptr<osg::Group> group = new osg::Group;
    group->setName(geode->getName());
    group->setStateSet(geode->getStateSet());
    for (i = 0; i < geode->getNumDrawables(); ++i)
    {
        osg::Geode* newGeode = new osg::Geode;
        newGeode->addDrawable(geode->getDrawable(i));
        group->addChild(newGeode);
    }

    divide(group.get(), maxNumTreesPerCell);

    // keep reference around to prevent it being deleted.
    osg::ref_ptr<osg::Geode> keepRefGeode = geode;

    for (osg::Node::ParentList::iterator itr = parents.begin();
        itr != parents.end();
        ++itr)
    {
        (*itr)->replaceChild(geode, group.get());
    }

    return true;
}

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


void buildTree(const std::string& filename)
{
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(INPUT_BASE_PATH + filename + R"(.fbx)");
    GroupCountVisitor gcv;
    node->accept(gcv);
    std::cout << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    SpatializeGroupsVisitor sgv(SpatialTreeType::QUADTREE);
    node->accept(sgv);
    sgv.divide();
    node->accept(gcv);
    osgDB::writeNodeFile(*node.get(), "./test.osg");
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
    buildTree(R"(龙翔桥站厅)");
    //OSG_NOTICE << R"(龙翔桥站厅处理完毕)" << std::endl;
    return 1;
}
