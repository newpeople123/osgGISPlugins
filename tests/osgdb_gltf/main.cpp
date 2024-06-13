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
    return 1;
}
