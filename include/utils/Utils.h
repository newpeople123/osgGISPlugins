#ifndef OSG_GIS_PLUGINS_UTILS_H
#define OSG_GIS_PLUGINS_UTILS_H
#include <osg/Node>
#include <osg/NodeVisitor>
#include <osg/Geometry>
#include <osg/PrimitiveSet>
#include <osg/StateSet>
#include <osg/Material>
#include <osg/Geode>
#include <osg/Texture2D>
#include <set>
#include "osgdb_gltf/material/GltfPbrMRMaterial.h"
#include "osgdb_gltf/material/GltfPbrSGMaterial.h"

namespace osgGISPlugins
{
    const static double EPSILON = 0.001;

    class Utils {
    public:
        static bool compareMatrix(const osg::Matrixd& lhs, const osg::Matrixd& rhs);

        static bool compareMatrixs(const std::vector<osg::Matrixd>& lhses, const std::vector<osg::Matrixd>& rhses);

        static bool compareStateSet(osg::ref_ptr<osg::StateSet> stateSet1, osg::ref_ptr<osg::StateSet> stateSet2);

        static bool comparePrimitiveSet(osg::ref_ptr<osg::PrimitiveSet> pSet1, osg::ref_ptr<osg::PrimitiveSet> pSet2);

        static bool compareGeometry(osg::ref_ptr<osg::Geometry> geom1, osg::ref_ptr<osg::Geometry> geom2);

        static bool compareGeode(osg::Geode& geode1, osg::Geode& geode2);
    };

    struct MatrixEqual {
        bool operator()(const osg::Matrixd& lhs, const osg::Matrixd& rhs) const {
            return Utils::compareMatrix(lhs, rhs);
        }
    };

    struct MatrixsEqual {
        bool operator()(const std::vector<osg::Matrixd>& lhses, const std::vector<osg::Matrixd>& rhses) const {
            return Utils::compareMatrixs(lhses, rhses);
        }
    };

    struct MatrixHash {
        std::size_t operator()(const osg::Matrixd& matrix) const {
            std::size_t seed = 0;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    seed ^= std::hash<double>()(matrix(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                }
            }
            return seed;
        }
    };

    struct MatrixsHash {
        std::size_t operator()(const std::vector<osg::Matrixd>& matrixs) const {
            std::size_t seed = 0;
            for (const auto matrix : matrixs)
            {
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        seed ^= std::hash<double>()(matrix(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                    }
                }
            }
            return seed;
        }
    };

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

                    if (osg::DrawArrays* drawArrays = dynamic_cast<osg::DrawArrays*>(primSet))
                    {
                        count += calculateTriangleCount(drawArrays->getMode(), drawArrays->getCount());
                    }
                    else if (osg::DrawElements* drawElements = dynamic_cast<osg::DrawElements*>(primSet))
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

        TextureCountVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

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

}
#endif // !OSG_GIS_PLUGINS_UTILS_H
