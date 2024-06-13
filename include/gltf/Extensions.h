#ifndef OSG_GIS_PLUGINS_GLTF_EXTENSIONS_H
#define OSG_GIS_PLUGINS_GLTF_EXTENSIONS_H 1

#include <string>
#include <array>
#include <osg/Texture2D>
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
struct GltfExtension {
	const std::string name;
	tinygltf::Value::Object value;

	GltfExtension(const std::string& name) : name(name) {}

	GltfExtension() = default;

	template <typename T>
	T Get(const std::string& key) const {
		auto item = value.find(key);
		if (item != value.end()) {
			return item->second.Get<T>();
		}
		return getDefault<T>();
	}

	template <typename T, size_t N>
	std::array<T, N> GetArray(const std::string& key) const {
		std::array<T, N> result = { 0.0 };  // 初始化为 0
		auto item = value.find(key);
		if (item != value.end()) {
			auto tinygltfArray = item->second.Get<tinygltf::Value::Array>();
			if (tinygltfArray.size() == N) {
				for (size_t i = 0; i < N; ++i) {
					result[i] = tinygltfArray[i].Get<T>();
				}
			}
		}
		return result;
	}

	template <typename T>
	void Set(const std::string& key, const T& val) {
		tinygltf::Value tinygltfVal(val);
		value[key] = std::move(tinygltfVal);
	}

private:
	template <typename T>
	struct Default;

	template <>
	struct Default<int> {
		static int value() { return -1; }
	};

	template <>
	struct Default<std::string> {
		static std::string value() { return ""; }
	};

	template <>
	struct Default<double> {
		static double value() { return 0.0; }
	};

	template <typename T, size_t N>
	struct Default<std::array<T, N>> {
		static std::array<T, N> value() { return { 0.0 }; }
	};

	template <typename T>
	T getDefault() const {
		return Default<T>::value();
	}
	template <typename T, size_t N>
	T getDefault() const {
		return Default<T, N>::value();
	}
};

#pragma region material
#pragma region common
//cesium does not support
struct KHR_materials_clearcoat : GltfExtension {
	KHR_materials_clearcoat() : GltfExtension("KHR_materials_clearcoat") {
		setClearcoatFactor(0.0);
		setClearcoatRoughnessFactor(0.0);
		setClearcoatTexture(-1);
		setClearcoatRoughnessTexture(-1);
		setClearcoatNormalTexture(-1);
	}
	osg::ref_ptr<osg::Texture2D> osgClearcoatTexture, osgClearcoatRoughnessTexture, osgClearcoatNormalTexture;
	double getClearcoatFactor() const {
		return Get<double>("clearcoatFactor");
	}

	void setClearcoatFactor(double val) {
		Set("clearcoatFactor", val);
	}

	double getClearcoatRoughnessFactor() const {
		return Get<double>("clearcoatRoughnessFactor");
	}

	void setClearcoatRoughnessFactor(double val) {
		Set("clearcoatRoughnessFactor", val);
	}

	int getClearcoatTexture() const {
		return Get<int>("clearcoatTexture");
	}

	void setClearcoatTexture(int val) {
		Set("clearcoatTexture", val);
	}

	int getClearcoatRoughnessTexture() const {
		return Get<int>("clearcoatRoughnessTexture");
	}

	void setClearcoatRoughnessTexture(int val) {
		Set("clearcoatRoughnessTexture", val);
	}

	int getClearcoatNormalTexture() const {
		return Get<int>("clearcoatNormalTexture");
	}

	void setClearcoatNormalTexture(int val) {
		Set("clearcoatNormalTexture", val);
	}
};

//cesium does not support
struct KHR_materials_anisotropy :GltfExtension
{
	KHR_materials_anisotropy() :GltfExtension("KHR_materials_anisotropy") {
		setAnisotropyStrength(0.0);
		setAnisotropyRotation(0.0);
		setAnisotropyTexture(-1);
	}
	osg::ref_ptr<osg::Texture2D> osgAnisotropyTexture;
	double getAnisotropyStrength() const {
		return Get<double>("anisotropyStrength");
	}

	void setAnisotropyStrength(double val) {
		Set("anisotropyStrength", val);
	}

	double getAnisotropyRotation() const {
		return Get<double>("anisotropyRotation");
	}

	void setAnisotropyRotation(double val) {
		Set("anisotropyRotation", val);
	}

	int getAnisotropyTexture() const {
		return Get<int>("anisotropyTexture");
	}

	void setAnisotropyTexture(int val) {
		Set("anisotropyTexture", val);
	}
};

//conflict KHR_materials_unlit,and cesium does not support
struct KHR_materials_emissive_strength :GltfExtension
{
	KHR_materials_emissive_strength() :GltfExtension("KHR_materials_emissive_strength") {
		setEmissiveStrength(0.0);
	}
	double getEmissiveStrength() const {
		return Get<double>("emissiveStrength");
	}

	void setEmissiveStrength(double val) {
		Set("emissiveStrength", val);
	}
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
struct KHR_materials_pbrSpecularGlossiness :GltfExtension{
	KHR_materials_pbrSpecularGlossiness() :GltfExtension("KHR_materials_pbrSpecularGlossiness") {
		setDiffuseFactor({ 1.0, 1.0, 1.0, 1.0 });
		setSpecularFactor({ 1.0, 1.0, 1.0 });
		setGlossinessFactor(0.0);
		setSpecularGlossinessTexture(-1);
		setDiffuseTexture(-1);
	}
	osg::ref_ptr<osg::Texture2D> osgSpecularGlossinessTexture, osgDiffuseTexture;
	std::array<double, 4> getDiffuseFactor() const {
		return GetArray<double, 4>("diffuseFactor");
	}

	void setDiffuseFactor(const std::array<double, 4>& val) {
		Set("diffuseFactor", tinygltf::Value::Array(val.begin(), val.end()));
	}

	std::array<double, 3> getSpecularFactor() const {
		return GetArray<double, 3>("specularFactor");
	}

	void setSpecularFactor(const std::array<double, 3>& val) {
		Set("specularFactor", tinygltf::Value::Array(val.begin(), val.end()));
	}

	double getGlossinessFactor() const {
		return Get<double>("glossinessFactor");
	}

	void setGlossinessFactor(double val) {
		Set("glossinessFactor", val);
	}

	int getSpecularGlossinessTexture() const {
		return Get<int>("specularGlossinessTexture");
	}

	void setSpecularGlossinessTexture(int val) {
		Set("specularGlossinessTexture", val);
	}

	int getDiffuseTexture() const {
		return Get<int>("diffuseTexture");
	}

	void setDiffuseTexture(int val) {
		Set("diffuseTexture", val);
	}
};
#pragma endregion

#pragma region MetallicRoughness
//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
struct KHR_materials_ior :GltfExtension
{
	KHR_materials_ior() :GltfExtension("KHR_materials_ior") {
		setIor(1.5);
	}
	double getIor() const {
		return Get<double>("ior");
	}

	void setIor(double val) {
		Set("ior", val);
	}
};

//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness
struct KHR_materials_sheen :GltfExtension
{
	KHR_materials_sheen() :GltfExtension("KHR_materials_sheen") {
		setSheenColorFactor({ 1.0, 1.0, 1.0 });
		setSheenRoughnessFactor(0.0);
		setSheenColorTexture(-1);
		setSheenRoughnessTexture(-1);
	}
	osg::ref_ptr<osg::Texture2D> osgSheenColorTexture, osgSheenRoughnessTexture;
	std::array<double, 3> getSheenColorFactor() const {
		return GetArray<double, 3>("sheenColorFactor");
	}
	void setSheenColorFactor(const std::array<double, 3>& val) {
		Set("sheenColorFactor", tinygltf::Value::Array(val.begin(), val.end()));
	}
	double getSheenRoughnessFactor() const {
		return Get<double>("sheenRoughnessFactor");
	}
	void setSheenRoughnessFactor(double val) {
		Set("sheenRoughnessFactor", val);
	}
	int getSheenColorTexture() const {
		return Get<int>("sheenColorTexture");
	}
	void setSheenColorTexture(int val) {
		Set("sheenColorTexture", val);
	}
	int getSheenRoughnessTexture() const {
		return Get<int>("sheenRoughnessTexture");
	}
	void setSheenRoughnessTexture(int val) {
		Set("sheenRoughnessTexture", val);
	}
};

//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness
struct KHR_materials_volume :GltfExtension
{
	KHR_materials_volume() :GltfExtension("KHR_materials_volume") {
		setAttenuationColor({ 1.0,1.0,1.0 });
		setThicknessFactor(0.0);
		setAttenuationDistance(std::numeric_limits<double>::infinity());
		getThicknessTexture(-1);

	}
	osg::ref_ptr<osg::Texture2D> osgThicknessTexture;
	std::array<double, 3> getAttenuationColor() const {
		return GetArray<double, 3>("attenuationColor");
	}
	void setAttenuationColor(const std::array<double, 3>& val) {
		Set("attenuationColor", tinygltf::Value::Array(val.begin(), val.end()));
	}
	double getThicknessFactor() const {
		return Get<double>("thicknessFactor");
	}
	void setThicknessFactor(double val) {
		Set("thicknessFactor", val);
	}
	double getAttenuationDistance() const {
		return Get<double>("attenuationDistance");
	}
	void setAttenuationDistance(double val) {
		Set("attenuationDistance", val);
	}
	int getThicknessTexture() const {
		return Get<int>("thicknessTexture");
	}
	void getThicknessTexture(int val) {
		Set("thicknessTexture", val);
	}
};

//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
struct KHR_materials_specular :GltfExtension
{
	KHR_materials_specular() :GltfExtension("KHR_materials_specular") {
		setSpecularColorFactor({ 1.0,1.0,1.0 });
		setSpecularFactor(0.0);
		setSpecularTexture(-1);
		setSpecularColorTexture(-1);

	}
	osg::ref_ptr<osg::Texture2D> osgSpecularTexture, osgSpecularColorTexture;
	std::array<double, 3> getSpecularColorFactor() const {
		return GetArray<double, 3>("specularColorFactor");
	}
	void setSpecularColorFactor(const std::array<double, 3>& val) {
		Set("specularColorFactor", tinygltf::Value::Array(val.begin(), val.end()));
	}
	double getSpecularFactor() const {
		return Get<double>("specularFactor");
	}
	void setSpecularFactor(double val) {
		Set("specularFactor", val);
	}
	int getSpecularTexture() const {
		return Get<int>("specularTexture");
	}
	void setSpecularTexture(int val) {
		Set("specularTexture", val);
	}
	int getSpecularColorTexture() const {
		return Get<int>("specularColorTexture");
	}
	void setSpecularColorTexture(int val) {
		Set("specularColorTexture", val);
	}

};

//conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
struct KHR_materials_transmission :GltfExtension
{
	KHR_materials_transmission() :GltfExtension("KHR_materials_transmission") {
		setTransmissionFactor(0.0);
		setTransmissionTexture(-1);
	}
	osg::ref_ptr<osg::Texture2D> osgTransmissionTexture;
	double getTransmissionFactor() const {
		return Get<double>("transmissionFactor");
	}
	void setTransmissionFactor(double val) {
		Set("transmissionFactor", val);
	}
	int getTransmissionTexture() const {
		return Get<int>("transmissionTexture");
	}
	void setTransmissionTexture(int val) {
		Set("transmissionTexture", val);
	}
};
#pragma endregion
#pragma endregion

#pragma region mesh
struct KHR_draco_mesh_compression :GltfExtension
{
	KHR_draco_mesh_compression() :GltfExtension("KHR_draco_mesh_compression") {
		setBufferView(-1);
		setPosition(-1);
		setNormal(-1);
		setTexCoord0(-1);
		setTexCoord1(-1);
		setWeights0(-1);
		setJoints0(-1);
		setBatchId(-1);

	}
	int getBufferView() const {
		return Get<int>("bufferView");
	}
	void setBufferView(int val) {
		Set("bufferView", val);
	}
	int getPosition() const {
		return Get<int>("POSITION");
	}
	void setPosition(int val) {
		Set("POSITION", val);
	}
	int getNormal() const {
		return Get<int>("NORMAL");
	}
	void setNormal(int val) {
		Set("NORMAL", val);
	}
	int getTexCoord0() const {
		return Get<int>("TEXCOORD_0");
	}
	void setTexCoord0(int val) {
		Set("TEXCOORD_0", val);
	}
	int getTexCoord1() const {
		return Get<int>("TEXCOORD_1");
	}
	void setTexCoord1(int val) {
		Set("TEXCOORD_1", val);
	}
	int getWeights0() const {
		return Get<int>("WEIGHTS_0");
	}
	void setWeights0(int val) {
		Set("WEIGHTS_0", val);
	}
	int getJoints0() const {
		return Get<int>("JOINTS_0");
	}
	void setJoints0(int val) {
		Set("JOINTS_0", val);
	}
	int getBatchId() const {
		return Get<int>("_BATCHID");
	}
	void setBatchId(int val) {
		Set("_BATCHID", val);
	}
};

const std::string KHR_mesh_quantization = "KHR_mesh_quantization";
const std::string EXT_meshopt_compression = "EXT_meshopt_compression";
const std::string EXT_mesh_gpu_instancing = "EXT_mesh_gpu_instancing";
#pragma endregion

#pragma region texture
struct KHR_texture_basisu :GltfExtension
{
	KHR_texture_basisu() :GltfExtension("KHR_texture_basisu") {
		setSource(-1);
	}
	int getSource() const {
		return Get<int>("source");
	}
	void setSource(int val) {
		Set("source", val);
	}
};

struct KHR_texture_transform :GltfExtension
{
	KHR_texture_transform() :GltfExtension("KHR_texture_transform") {
		setOffset({ 0.0,0.0 });
		setScale({ 1.0,1.0 });
		setTexCoord(-1);
		setRotation(0.0);
	}
	std::array<double, 2> getOffset() const {
		return GetArray<double, 2>("offset");
	}
	void setOffset(const std::array<double, 2>& val) {
		Set("offset", tinygltf::Value::Array(val.begin(), val.end()));
	}
	std::array<double, 2> getScale() const {
		return GetArray<double, 2>("scale");
	}
	void setScale(const std::array<double, 2>& val) {
		Set("scale", tinygltf::Value::Array(val.begin(), val.end()));
	}
	int getTexCoord() const {
		return Get<int>("texCoord");
	}
	void setTexCoord(int val) {
		Set("texCoord", val);
	}
	double getRotation() const {
		return Get<double>("rotation");
	}
	void setRotation(double val) {
		Set("rotation", val);
	}
};

struct EXT_texture_webp :GltfExtension
{
	EXT_texture_webp() :GltfExtension("EXT_texture_webp") {
		setSource(-1);
	}
	int getSource() const {
		return Get<int>("source");
	}
	void setSource(int val) {
		Set("source", val);
	}
};
#pragma endregion
#endif // !OSG_GIS_PLUGINS_GLTF_EXTENSIONS_H
