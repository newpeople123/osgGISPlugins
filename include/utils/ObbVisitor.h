#ifndef OSG_GIS_PLUGINS_OBB_VISITOR_H
#define OSG_GIS_PLUGINS_OBB_VISITOR_H
#include <osg/NodeVisitor>
#include <osg/Geometry>
#include <osg/Matrix>
#include <osg/Vec3>
#include <osg/Node>
#define ROTATE(a,i,j,k,l) g=a(i,j); h=a(k, l); a(i, j)=(float)(g-s*(h+g*tau)); a(k, l)=(float)(h+s*(g-h*tau));
namespace osgGISPlugins
{
    class OBBVisitor : public osg::NodeVisitor
    {
    public:
        OBBVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

        // 覆盖 apply 方法，处理 Geode 节点
        void apply(osg::Geode& geode) override;

        void apply(osg::Group& group) override;

        // 获取收集到的顶点数据
        osg::ref_ptr<osg::Vec3Array> getVertices() const { return _vertices; }

        osg::ref_ptr<osg::Vec3Array> getOBBCorners()
        {
            calculateOBB();
            corners->push_back(_center + _extentX + _extentY + _extentZ);
            corners->push_back(_center + _extentX + _extentY - _extentZ);
            corners->push_back(_center + _extentX - _extentY + _extentZ);
            corners->push_back(_center + _extentX - _extentY - _extentZ);
            corners->push_back(_center - _extentX + _extentY + _extentZ);
            corners->push_back(_center - _extentX + _extentY - _extentZ);
            corners->push_back(_center - _extentX - _extentY + _extentZ);
            corners->push_back(_center - _extentX - _extentY - _extentZ);

            return corners;
        }


        osg::ref_ptr<osg::Vec3Array> corners = new osg::Vec3Array;
    private:
        osg::ref_ptr<osg::Vec3Array> _vertices = new osg::Vec3Array;

        osg::Matrixd _currentMatrix;
        osg::Vec3 _center;   // obb center
        osg::Vec3 _xAxis;    // x axis of obb, unit vector
        osg::Vec3 _yAxis;    // y axis of obb, unit vecotr
        osg::Vec3 _zAxis;    // z axis of obb, unit vector
        osg::Vec3 _extentX;  // _xAxis * _extents.x
        osg::Vec3 _extentY;  // _yAxis * _extents.y
        osg::Vec3 _extentZ;  // _zAxis * _extents.z
        osg::Vec3 _extents;  // obb length along each axis

        void calculateOBB();

        void computeExtAxis();

        static osg::Matrix getOBBOrientation(const osg::ref_ptr<osg::Vec3Array>& vertices);

        static osg::Matrix getConvarianceMatrix(const osg::ref_ptr<osg::Vec3Array>& vertices);

        static void getEigenVectors(osg::Matrix* vout, osg::Vec3* dout, osg::Matrix a);

        static float& getElement(osg::Vec3& point, int index);

        static osg::Matrix transpose(const osg::Matrix& b);
    };
}
#endif // !OSG_GIS_PLUGINS_OBB_VISITOR_H
