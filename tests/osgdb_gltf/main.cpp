#include <iostream>
#include <osgDB/ReadFile>
#include "osgdb_gltf/Extensions.h"
using namespace std;
int main() {
    GltfExtension t1;
    int tt1 = t1.Get<int>("test1");
    t1.Set("test1", 10);
    int tt2 = t1.Get<int>("test1");


    std::string tt3 = t1.Get<std::string>("test2");
    std::string s = "10";
    t1.Set("test2", s);
    std::string tt4 = t1.Get<std::string>("test2");

    KHR_materials_clearcoat2 t2;
    t2.clearcoatRoughnessTexture = 100;
    t2.Value();
    auto val = t2.value;
    return 1;
}
