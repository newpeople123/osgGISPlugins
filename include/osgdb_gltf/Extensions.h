#include <string>
#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <osg/Texture2D>
using namespace std;
struct GltfExtension
{
	const string name;
	tinygltf::Value value;
	GltfExtension() :value(tinygltf::Value::Object){

	}
	auto toExtensionValue() {
		return std::make_pair(name, value);
	}
};
#pragma region material
#pragma region common
//cesium does not support
struct KHR_materials_clearcoat:GltfExtension
{
	KHR_materials_clearcoat() :name("KHR_materials_clearcoat"),clearcoatFactor(0.0),clearcoatRoughnessFactor(0.0) {}
	double clearcoatFactor, clearcoatRoughnessFactor;
	osg::ref_ptr<osg::Texture2D> clearcoatTexture;
	osg::ref_ptr<osg::Texture2D> clearcoatRoughnessTexture;
	osg::ref_ptr<osg::Texture2D> clearcoatNormalTexture;
}

//cesium does not support
struct KHR_materials_anisotropy :GltfExtension
{
	KHR_materials_anisotropy() :name("KHR_materials_anisotropy"), anisotropyStrength(0.0), anisotropyRotation(0.0) {}
	double anisotropyStrength, anisotropyRotation;
	osg::ref_ptr<osg::Texture2D> anisotropyTexture;
}

//conflict KHR_materials_unlit,and cesium does not support
struct KHR_materials_emissive_strength :GltfExtension
{
	KHR_materials_emissive_strength() :name("KHR_materials_emissive_strength"), emissiveStrength(1.0){}
	double emissiveStrength;
}

struct KHR_materials_variants :GltfExtension
{
	KHR_materials_variants() :name("KHR_materials_variants") {}
}

struct KHR_materials_unlit :GltfExtension
{
	KHR_materials_unlit() :name("KHR_materials_unlit") {}
}
#pragma endregion

#pragma region SpecularGlossiness
struct KHR_materials_pbrSpecularGlossiness :GltfExtension
{
	KHR_materials_pbrSpecularGlossiness() :name("KHR_materials_pbrSpecularGlossiness"), diffuseFactor([1.0,1.0,1.0,1.0]), specularFactor([1.0, 1.0, 1.0]), glossinessFactor(1.0){}
	array<double, 4> diffuseFactor;
	array<double, 3> specularFactor;
	double glossinessFactor;
	osg::ref_ptr<osg::Texture2D> specularGlossinessTexture;
	osg::ref_ptr<osg::Texture2D> diffuseTexture;
}
#pragma endregion

#pragma region MetallicRoughness
const string KHR_materials_ior = "KHR_materials_ior"; //conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
const string KHR_materials_sheen = "KHR_materials_sheen"; //conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness
const string KHR_materials_volume = "KHR_materials_volume"; //conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness
const string KHR_materials_specular = "KHR_materials_specular"; //conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
const string KHR_materials_transmission = "KHR_materials_transmission";
#pragma endregion
#pragma endregion

#pragma region mesh
const string KHR_draco_mesh_compression = "KHR_draco_mesh_compression";
const string KHR_mesh_quantization = "KHR_mesh_quantization";
const string EXT_meshopt_compression = "EXT_meshopt_compression";
const string EXT_mesh_gpu_instancing = "EXT_mesh_gpu_instancing";
#pragma endregion

#pragma region texture
const string KHR_texture_basisu = "KHR_texture_basisu";
const string KHR_texture_transform = "KHR_texture_transform";
const string EXT_texture_webp = "EXT_texture_webp";
#pragma endregion




