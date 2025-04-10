#ifndef OSG_GIS_PLUGINS_B3DM_USERDATA_H
#define OSG_GIS_PLUGINS_B3DM_USERDATA_H 1
#include <osg/ValueObject>
#include <osg/io_utils>
namespace osgGISPlugins {
    class UserDataVisitor : public osg::ValueObject::GetValueVisitor
    {
    public:
	    void apply(const bool value) override { _ss << value; }
	    void apply(const char value) override { _ss << value; }
	    void apply(const unsigned char value) override { _ss << value; }
	    void apply(const short value) override { _ss << value; }
	    void apply(const unsigned short value) override { _ss << value; }
	    void apply(const int value) override { _ss << value; }
	    void apply(const unsigned int value) override { _ss << value; }
	    void apply(const float value) override { _ss << value; }
	    void apply(const double value) override { _ss << value; }
	    void apply(const std::string& value) override { _ss << value; }
	    void apply(const osg::Vec2f& value) override { _ss << value; }
	    void apply(const osg::Vec3f& value) override { _ss << value; }
	    void apply(const osg::Vec4f& value) override { _ss << value; }
	    void apply(const osg::Vec2d& value) override { _ss << value; }
	    void apply(const osg::Vec3d& value) override { _ss << value; }
	    void apply(const osg::Vec4d& value) override { _ss << value; }
	    void apply(const osg::Quat& value) override { _ss << value; }
	    void apply(const osg::Plane& value) override { _ss << value; }
	    void apply(const osg::Matrixf& value) override { _ss << value; }
	    void apply(const osg::Matrixd& value) override { _ss << value; }
        std::string value() const { return _ss.str(); }
        void clear() {
            _ss.str(""); // 将流的内容重置为空字符串
            _ss.clear(); // 重置流的状态标志
        }
    private:
        std::ostringstream _ss;
    };
}
#endif // !OSG_GIS_PLUGINS_B3DM_USERDATA_H
