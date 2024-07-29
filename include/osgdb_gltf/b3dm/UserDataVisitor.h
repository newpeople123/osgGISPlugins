#ifndef OSG_GIS_PLUGINS_B3DM_USERDATA_H
#define OSG_GIS_PLUGINS_B3DM_USERDATA_H 1
#include <osg/ValueObject>
#include <nlohmann/json.hpp>
using namespace nlohmann;
class UserDataVisitor : public osg::ValueObject::GetValueVisitor
{
public:
    UserDataVisitor(json& item) :_item(item) {};
    void apply(bool value) override { _item.push_back(value); }
    void apply(char value) override { _item.push_back(value); }
    void apply(unsigned char value) override { _item.push_back(value); }
    void apply(short value) override { _item.push_back(value); }
    void apply(unsigned short value) override { _item.push_back(value); }
    void apply(int value) override { _item.push_back(value); }
    void apply(unsigned int value) override { _item.push_back(value); }
    void apply(float value) override { _item.push_back(value); }
    void apply(double value) override { _item.push_back(value); }
    void apply(const std::string& value) override { _item.push_back(value); }
private:
    json& _item;
};
#endif // !OSG_GIS_PLUGINS_B3DM_USERDATA_H
