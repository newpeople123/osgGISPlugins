#include <string>
#include <array>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
#include <osg/Texture>
template <typename Base>
struct GltfExtension {
	const std::string name;

	tinygltf::Value::Object value;

	GltfExtension(const std::string& name) : name(name) {}

	GltfExtension() = default;
};

#pragma region material
#pragma region common
//cesium does not support
struct KHR_materials_clearcoat : GltfExtension<KHR_materials_clearcoat> {
	KHR_materials_clearcoat() : GltfExtension("KHR_materials_clearcoat"), clearcoatFactor(0.0), clearcoatRoughnessFactor(0.0) {}
	osg::ref_ptr<osg::Texture> osgClearcoatTexture, osgClearcoatRoughnessTexture, osgClearcoatNormalTexture;
	double getClearcoatFactor() const {
		return value.at("clearcoatFactor").Get<double>();
	}

	void setClearcoatFactor(double val) {
		value["clearcoatFactor"] = tinygltf::Value(val);
	}

	double getClearcoatRoughnessFactor() const {
		return value.at("clearcoatRoughnessFactor").Get<double>();
	}

	void setClearcoatRoughnessFactor(double val) {
		value["clearcoatRoughnessFactor"] = tinygltf::Value(val);
	}

	int getClearcoatTexture() const {
		return value.at("clearcoatTexture").Get<int>();
	}

	void setClearcoatTexture(int val) {
		value["clearcoatTexture"] = tinygltf::Value(val);
	}

	int getClearcoatRoughnessTexture() const {
		return value.at("clearcoatRoughnessTexture").Get<int>();
	}

	void setClearcoatRoughnessTexture(int val) {
		value["clearcoatRoughnessTexture"] = tinygltf::Value(val);
	}

	int getClearcoatNormalTexture() const {
		return value.at("clearcoatNormalTexture").Get<int>();
	}

	void setClearcoatNormalTexture(int val) {
		value["clearcoatNormalTexture"] = tinygltf::Value(val);
	}
private:
	double clearcoatFactor, clearcoatRoughnessFactor;
	int clearcoatTexture = -1, clearcoatRoughnessTexture = -1, clearcoatNormalTexture = -1;
};

//cesium does not support
struct KHR_materials_anisotropy :GltfExtension<KHR_materials_anisotropy>
{
	KHR_materials_anisotropy() :GltfExtension("KHR_materials_anisotropy"), anisotropyStrength(0.0), anisotropyRotation(0.0) {}
	osg::ref_ptr<osg::Texture> anisotropyOsgTexture;
	double getAnisotropyStrength() const {
		return value.at("anisotropyStrength").Get<double>();
	}
	void setAnisotropyStrength(double val) {
		value["anisotropyStrength"] = tinygltf::Value(val);
	}
	double getAnisotropyRotation() const {
		return value.at("anisotropyRotation").Get<double>();
	}
	void setAnisotropyRotation(double val) {
		value["anisotropyRotation"] = tinygltf::Value(val);
	}
	int getAnisotropyTexture() const {
		return value.at("anisotropyTexture").Get<int>();
	}
	void setAnisotropyTexture(int val) {
		value["anisotropyTexture"] = tinygltf::Value(val);
	}
private:
	double anisotropyStrength, anisotropyRotation;
	int anisotropyTexture = -1;
};

//conflict KHR_materials_unlit,and cesium does not support
struct KHR_materials_emissive_strength :GltfExtension<KHR_materials_emissive_strength>
{
	KHR_materials_emissive_strength() :GltfExtension("KHR_materials_emissive_strength"), emissiveStrength(1.0) {}
	double getEmissiveStrength() const {
		return value.at("emissiveStrength").Get<double>();
	}
	void setEmissiveStrength(double val) {
		value["emissiveStrength"] = tinygltf::Value(val);
	}
private:
	double emissiveStrength;
};

struct KHR_materials_variants :GltfExtension
{
	KHR_materials_variants() :GltfExtension("KHR_materials_variants") {}
};

struct KHR_materials_unlit :GltfExtension
{
	KHR_materials_unlit() :GltfExtension("KHR_materials_unlit") {}
};
#pragma endregion

#pragma region SpecularGlossiness
struct KHR_materials_pbrSpecularGlossiness :GltfExtension
{
	KHR_materials_pbrSpecularGlossiness() :GltfExtension("KHR_materials_pbrSpecularGlossiness"), diffuseFactor({ 1.0, 1.0, 1.0, 1.0 }), specularFactor({ 1.0, 1.0, 1.0 }), glossinessFactor(1.0), specularGlossinessTexture(-1), diffuseTexture(-1){}
	std::array<double, 4> diffuseFactor;
	std::array<double, 3> specularFactor;
	double glossinessFactor;
	osg::ref_ptr<osg::Texture> specularGlossinessOsgTexture;
	osg::ref_ptr<osg::Texture> diffuseOsgTexture;
	int specularGlossinessTexture;
	int diffuseTexture;
	auto Value() {
		tinygltf::Value::Array tinygltfArrayDiffuseFactor, tinygltfArraySpecularFactor;
		tinygltfArrayDiffuseFactor.push_back(tinygltf::Value(diffuseFactor[0]));
		tinygltfArrayDiffuseFactor.push_back(tinygltf::Value(diffuseFactor[1]));
		tinygltfArrayDiffuseFactor.push_back(tinygltf::Value(diffuseFactor[2]));
		tinygltfArrayDiffuseFactor.push_back(tinygltf::Value(diffuseFactor[3]));
		tinygltfArraySpecularFactor.push_back(tinygltf::Value(specularFactor[0]));
		tinygltfArraySpecularFactor.push_back(tinygltf::Value(specularFactor[1]));
		tinygltfArraySpecularFactor.push_back(tinygltf::Value(specularFactor[2]));

		Set("glossinessFactor", glossinessFactor);
		Set("diffuseFactor", tinygltfArrayDiffuseFactor);
		Set("specularFactor", tinygltfArraySpecularFactor);
		Set("specularGlossinessTexture", specularGlossinessTexture);
		Set("diffuseTexture", diffuseTexture);

		return std::make_pair(name, tinygltf::Value(value));
	}
};
#pragma endregion

#pragma region MetallicRoughness
//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
struct KHR_materials_ior :GltfExtension
{
	KHR_materials_ior() :GltfExtension("KHR_materials_ior"), ior(1.5) {}
	double ior;
	auto Value() {
		Set("ior", ior);
		return std::make_pair(name, tinygltf::Value(value));
	}
};

//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness
struct KHR_materials_sheen :GltfExtension
{
	KHR_materials_sheen() :GltfExtension("KHR_materials_sheen"), sheenRoughnessFactor(0.0), sheenColorFactor({ 0.0,0.0,0.0 }), sheenColorTexture(-1), sheenRoughnessTexture(-1) {}
	double sheenRoughnessFactor;
	std::array<double, 3> sheenColorFactor;
	osg::ref_ptr<osg::Texture> sheenColorOsgTexture;
	osg::ref_ptr<osg::Texture> sheenRoughnessOsgTexture;
	int sheenColorTexture;
	int sheenRoughnessTexture;
	auto Value() {
		tinygltf::Value::Array tinygltfArraySheenColorFactor;
		tinygltfArraySheenColorFactor.push_back(tinygltf::Value(sheenColorFactor[0]));
		tinygltfArraySheenColorFactor.push_back(tinygltf::Value(sheenColorFactor[1]));
		tinygltfArraySheenColorFactor.push_back(tinygltf::Value(sheenColorFactor[2]));

		Set("sheenRoughnessFactor", sheenRoughnessFactor);
		Set("sheenColorFactor", tinygltfArraySheenColorFactor);
		Set("sheenColorTexture", sheenColorTexture);
		Set("sheenRoughnessTexture", sheenRoughnessTexture);
		return std::make_pair(name, tinygltf::Value(value));
	}
};

//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness
struct KHR_materials_volume :GltfExtension
{
	KHR_materials_volume() :GltfExtension("KHR_materials_volume"), thicknessFactor(0.0), attenuationDistance(std::numeric_limits<double>::infinity()), attenuationColor({ 1.0,1.0,1.0 }), thicknessTexture(-1){}
	double thicknessFactor, attenuationDistance;
	std::array<double, 3> attenuationColor;
	osg::ref_ptr<osg::Texture> thicknessOsgTexture;
	int thicknessTexture;

	auto Value() {
		tinygltf::Value::Array tinygltfArrayAttenuationColor;
		tinygltfArrayAttenuationColor.push_back(tinygltf::Value(attenuationColor[0]));
		tinygltfArrayAttenuationColor.push_back(tinygltf::Value(attenuationColor[1]));
		tinygltfArrayAttenuationColor.push_back(tinygltf::Value(attenuationColor[2]));

		Set("attenuationColor", tinygltfArrayAttenuationColor);
		Set("thicknessFactor", thicknessFactor);
		Set("attenuationDistance", attenuationDistance);
		Set("thicknessTexture", thicknessTexture);


		return std::make_pair(name, tinygltf::Value(value));
	}
};

//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
struct KHR_materials_specular :GltfExtension
{
	KHR_materials_specular() :GltfExtension("KHR_materials_specular"), specularFactor(1.0), specularColorFactor({ 1.0,1.0,1.0 }), specularTexture(-1), specularColorTexture(-1){}
	double specularFactor;
	std::array<double, 3> specularColorFactor;
	osg::ref_ptr<osg::Texture> specularOsgTexture;
	osg::ref_ptr<osg::Texture> specularColorOsgTexture;
	int specularTexture;
	int specularColorTexture;
	auto Value() {
		tinygltf::Value::Array tinygltfArraySpecularColorFactor;
		tinygltfArraySpecularColorFactor.push_back(tinygltf::Value(specularColorFactor[0]));
		tinygltfArraySpecularColorFactor.push_back(tinygltf::Value(specularColorFactor[1]));
		tinygltfArraySpecularColorFactor.push_back(tinygltf::Value(specularColorFactor[2]));

		Set("specularColorFactor", tinygltfArraySpecularColorFactor);
		Set("specularFactor", specularFactor);
		Set("specularTexture", specularTexture);
		Set("specularColorTexture", specularColorTexture);

		return std::make_pair(name, tinygltf::Value(value));
	}
};

//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
struct KHR_materials_transmission :GltfExtension
{
	KHR_materials_transmission() :GltfExtension("KHR_materials_transmission"), transmissionFactor(0.0), transmissionTexture(-1){}
	double transmissionFactor;
	osg::ref_ptr<osg::Texture> transmissionOsgTexture;
	int transmissionTexture;

	auto Value() {
		Set("transmissionFactor", transmissionFactor);
		Set("transmissionTexture", transmissionTexture);
		return std::make_pair(name, tinygltf::Value(value));
	}
};
#pragma endregion
#pragma endregion

#pragma region mesh
struct KHR_draco_mesh_compression :GltfExtension
{
	KHR_draco_mesh_compression() :GltfExtension("KHR_draco_mesh_compression"), bufferView(-1), POSITION(-1), NORMAL(-1), TEXCOORD_0(-1), TEXCOORD_1(-1), WEIGHTS_0(-1), JOINTS_0(-1), _BATCHID(-1) {}
	int bufferView;
	int POSITION;
	int NORMAL;
	int TEXCOORD_0;
	int TEXCOORD_1;
	int WEIGHTS_0;
	int JOINTS_0;
	int _BATCHID;
	auto Value() {
		GltfExtension attributes;
		attributes.Set("POSITION", POSITION);
		attributes.Set("NORMAL", NORMAL);
		attributes.Set("TEXCOORD_0", TEXCOORD_0);
		attributes.Set("TEXCOORD_1", TEXCOORD_1);
		attributes.Set("WEIGHTS_0", WEIGHTS_0);
		attributes.Set("JOINTS_0", JOINTS_0);
		attributes.Set("_BATCHID", _BATCHID);

		Set("bufferView", bufferView);
		Set("attributes", attributes.value);

		return std::make_pair(name, tinygltf::Value(value));
	}
};

const std::string KHR_mesh_quantization = "KHR_mesh_quantization";
const std::string EXT_meshopt_compression = "EXT_meshopt_compression";
const std::string EXT_mesh_gpu_instancing = "EXT_mesh_gpu_instancing";
#pragma endregion

#pragma region texture
struct KHR_texture_basisu :GltfExtension
{
	KHR_texture_basisu() :GltfExtension("KHR_texture_basisu"), source(-1) {}
	int source;
	auto Value() {
		Set("source", source);
		return std::make_pair(name, tinygltf::Value(value));
	}
};

struct KHR_texture_transform :GltfExtension
{
	KHR_texture_transform() :GltfExtension("KHR_texture_transform"), offset({ 0.0,0.0 }), scale({ 1.0,1.0 }), texCoord(-1), rotation(0.0) {}
	std::array<double, 2>offset, scale;
	int texCoord;
	double rotation;
	auto Value() {
		tinygltf::Value::Array tinygltfArrayOffset, tinygltfArrayScale;
		tinygltfArrayOffset.push_back(tinygltf::Value(offset[0]));
		tinygltfArrayOffset.push_back(tinygltf::Value(offset[1]));
		tinygltfArrayScale.push_back(tinygltf::Value(scale[0]));
		tinygltfArrayScale.push_back(tinygltf::Value(scale[1]));
		Set("texCoord", texCoord);
		Set("rotation", rotation);
		Set("offset", tinygltfArrayOffset);
		Set("scale", tinygltfArrayScale);
		return std::make_pair(name, tinygltf::Value(value));
	}
};

struct EXT_texture_webp :GltfExtension
{
	EXT_texture_webp() :GltfExtension("EXT_texture_webp"), source(-1) {}
	int source;
	auto Value() {
		Set("source", source);
		return std::make_pair(name, tinygltf::Value(value));
	}
};
#pragma endregion




