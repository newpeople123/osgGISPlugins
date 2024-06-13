#include <iostream>
#include <osgDB/ReadFile>
#include "gltf/Extensions.h"
#include "gltf/material/GltfPbrSGMaterial.h"
using namespace std;
int main() {
    KHR_materials_clearcoat clearcoat_extension;
    clearcoat_extension.setClearcoatFactor(2.0);
    clearcoat_extension.setClearcoatNormalTexture(1);
    
    osg::ref_ptr<osg::Material> material = new GltfPbrSGMaterial;
    osg::ref_ptr<GltfPbrSGMaterial> mat = dynamic_cast<GltfPbrSGMaterial*>(material.get());


    std::vector<GltfExtension*> t2;
    t2.push_back(&clearcoat_extension);
    GltfExtension* t3 = t2[0];
    if (typeid(*t3) == typeid(KHR_materials_clearcoat)) {
        std::cout << std::endl;
    }
    std::cout << typeid(t3).name() << std::endl;
    std::cout << typeid(KHR_materials_clearcoat).name() << std::endl;
    return 1;
}
