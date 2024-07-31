#ifndef OSG_GIS_PLUGINS_B3DM_USERDATA_H
#define OSG_GIS_PLUGINS_B3DM_USERDATA_H 1
#include <osg/ValueObject>
#include <osg/io_utils>

class UserDataVisitor : public osg::ValueObject::GetValueVisitor
{
public:
    virtual void apply(bool value) { _ss << value; }
    virtual void apply(char value) { _ss << value; }
    virtual void apply(unsigned char value) { _ss << value; }
    virtual void apply(short value) { _ss << value; }
    virtual void apply(unsigned short value) { _ss << value; }
    virtual void apply(int value) { _ss << value; }
    virtual void apply(unsigned int value) { _ss << value; }
    virtual void apply(float value) { _ss << value; }
    virtual void apply(double value) { _ss << value; }
    virtual void apply(const std::string& value) { _ss << value; }
    virtual void apply(const osg::Vec2f& value) { _ss << value; }
    virtual void apply(const osg::Vec3f& value) { _ss << value; }
    virtual void apply(const osg::Vec4f& value) { _ss << value; }
    virtual void apply(const osg::Vec2d& value) { _ss << value; }
    virtual void apply(const osg::Vec3d& value) { _ss << value; }
    virtual void apply(const osg::Vec4d& value) { _ss << value; }
    virtual void apply(const osg::Quat& value) { _ss << value; }
    virtual void apply(const osg::Plane& value) { _ss << value; }
    virtual void apply(const osg::Matrixf& value) { _ss << value; }
    virtual void apply(const osg::Matrixd& value) { _ss << value; }
    std::string value() const { return _ss.str(); }
    void clear() { 
        _ss.str(""); // 将流的内容重置为空字符串
        _ss.clear(); // 重置流的状态标志
    }
private:
    std::ostringstream _ss;
};
#endif // !OSG_GIS_PLUGINS_B3DM_USERDATA_H
