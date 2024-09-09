#ifndef OSG_GIS_PLUGINS_TREE_BUILDER_H
#define OSG_GIS_PLUGINS_TREE_BUILDER_H
#include <osg/NodeVisitor>
#include "3dtiles/Tileset.h"
#include <osg/PrimitiveSet>
#include <osg/Geometry>
namespace osgGISPlugins
{
    class TriangleCountVisitor : public osg::NodeVisitor
    {
    public:
        META_NodeVisitor(osgGISPlugins, TriangleCountVisitor)

        unsigned int count = 0;

        TriangleCountVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

        void apply(osg::Drawable& drawable) override
        {
            osg::Geometry* geom = drawable.asGeometry();
            if (geom)
            {
                for (unsigned int i = 0; i < geom->getNumPrimitiveSets(); ++i)
                {
                    osg::PrimitiveSet* primSet = geom->getPrimitiveSet(i);

                    if (auto* drawArrays = dynamic_cast<osg::DrawArrays*>(primSet))
                    {
                        count += calculateTriangleCount(drawArrays->getMode(), drawArrays->getCount());
                    }
                    else if (auto* drawElements = dynamic_cast<osg::DrawElements*>(primSet))
                    {
                        count += drawElements->getNumIndices() / 3;
                    }
                }
            }
        }

    private:
        unsigned int calculateTriangleCount(const GLenum mode, const unsigned int count)
        {
            switch (mode)
            {
            case GL_TRIANGLES:
                return count / 3;
            case GL_TRIANGLE_STRIP:
            case GL_TRIANGLE_FAN:
            case GL_POLYGON:
                return count - 2;
            case GL_QUADS:
                return (count / 4) * 2;
            case GL_QUAD_STRIP:
                return count;
            default:
                return 0;
            }
        }
    };

    class TextureCountVisitor :public osg::NodeVisitor
    {
    public:
        META_NodeVisitor(osgGISPlugins, TextureCountVisitor)

        unsigned int count = 0;

        TextureCountVisitor():osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

        void apply(osg::Drawable& drawable) override
        {
            osg::StateSet* stateSet = drawable.getStateSet();
            if (stateSet)
            {
                countTextures(stateSet);
            }
        }
    private:
        std::set<std::string> _names;
        void countTextures(osg::StateSet* stateSet)
        {
            for (unsigned int i = 0; i < stateSet->getTextureAttributeList().size(); ++i)
            {
                const osg::Texture* osgTexture = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
                if (osgTexture)
                {
                    const osg::Image* img = osgTexture->getImage(0);
                    const std::string name = img->getFileName();
                    if (_names.find(name) == _names.end())
                    {
                        _names.insert(name);
                        ++count;
                    }
                }
            }
        }
    };

    class TreeBuilder :public osg::NodeVisitor
	{
    public:
        META_NodeVisitor(osgGISPlugins, TreeBuilder)

        TreeBuilder() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {};

        virtual osg::ref_ptr<Tile> build();

        std::map<osg::Geode*, std::vector<osg::Matrixd>> test;

        static bool compareStateSet(osg::ref_ptr<osg::StateSet> stateSet1, osg::ref_ptr<osg::StateSet> stateSet2);

        static bool comparePrimitiveSet(osg::ref_ptr<osg::PrimitiveSet> pSet1, osg::ref_ptr<osg::PrimitiveSet> pSet2);

        static bool compareGeometry(osg::ref_ptr<osg::Geometry> geom1, osg::ref_ptr<osg::Geometry> geom2);

        static bool compareGeode(osg::Geode& geode1, osg::Geode& geode2);
    protected:
        typedef std::set<osg::Group*> GroupsToDivideList;
        GroupsToDivideList _groupsToDivideList;

        typedef std::set<osg::Geode*> GeodesToDivideList;
        GeodesToDivideList _geodesToDivideList;

        unsigned int _maxLevel = -1;

        size_t _maxTriangleCount = 12e4;

        unsigned int _maxTextureCount = 35;

        osg::Matrixd _currentMatrix;

        virtual void apply(osg::Group& group) override;

        virtual void apply(osg::Geode& geode) override;

        virtual osg::ref_ptr<Tile> divide(osg::ref_ptr<osg::Group> group, const osg::BoundingBox& bounds, osg::ref_ptr<Tile> parent = nullptr, const unsigned int x = 0, const unsigned int y = 0, const unsigned int z = 0, const unsigned int level = 0) = 0;
	};
}

#endif // !OSG_GIS_PLUGINS_TREE_BUILDER_H
