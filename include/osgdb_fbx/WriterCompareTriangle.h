#ifndef _3DS_WRITER_COMPARE_TRIANGLE_HEADER__
#define _3DS_WRITER_COMPARE_TRIANGLE_HEADER__

#include <osg/Geometry>

struct Triangle
{
    unsigned int t1;
    unsigned int t2;
    unsigned int t3;
    unsigned int material;
};

class WriterCompareTriangle {
public:
    WriterCompareTriangle(const osg::Geode& geode,
                          unsigned int nbVertices);

    bool operator()(const std::pair<Triangle, int>& t1,
                    const std::pair<Triangle, int>& t2) const;
private:
    static void // This function prevents the scene being cut into too many boxes
        setMaxMin(unsigned int& nbVerticesX,
                  unsigned int& nbVerticesY,
                  unsigned int& nbVerticesZ);

    /**
    *  Cut the scene in different box to sort.
    *  \param nbVertices is the number of vertices in mesh.
    *  \param sceneBox contain the size of the scene.
    */
    void
    cutscene(int                     nbVertices,
             const osg::BoundingBox& sceneBox);

    /**
    *  Find in which box those points are.
    *  \return the place of the box in the vector.
    *  \sa See cutScene() about the definition of the boxes for faces sorting.
    */
    int inWhichBox(osg::Vec3::value_type x,
                   osg::Vec3::value_type y,
                   osg::Vec3::value_type z) const;

    const osg::Geode&                    geode;
    std::vector<osg::BoundingBox>        boxList;
};

#endif // _3DS_WRITER_COMPARE_TRIANGLE_HEADER__
