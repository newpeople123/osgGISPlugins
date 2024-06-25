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
	virtual ~GltfExtension() = default;
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

	virtual GltfExtension* clone() {
		return new GltfExtension;
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

	GltfExtension* clone() override {
		KHR_materials_clearcoat* newExtension = new KHR_materials_clearcoat;
		newExtension->setClearcoatFactor(this->getClearcoatFactor());
		newExtension->setClearcoatRoughnessFactor(this->getClearcoatRoughnessFactor());
		newExtension->setClearcoatTexture(this->getClearcoatTexture());
		newExtension->setClearcoatRoughnessTexture(this->getClearcoatRoughnessTexture());
		newExtension->setClearcoatNormalTexture(this->getClearcoatNormalTexture());
		if(this->osgClearcoatTexture.valid())
			newExtension->osgClearcoatTexture = (osg::Texture2D*)this->osgClearcoatTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		if (this->osgClearcoatRoughnessTexture.valid())
			newExtension->osgClearcoatRoughnessTexture = (osg::Texture2D*)this->osgClearcoatRoughnessTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		if (this->osgClearcoatNormalTexture.valid())
			newExtension->osgClearcoatNormalTexture = (osg::Texture2D*)this->osgClearcoatNormalTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		return newExtension;
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

	GltfExtension* clone() override {
		KHR_materials_anisotropy* newExtension = new KHR_materials_anisotropy;
		newExtension->setAnisotropyStrength(this->getAnisotropyStrength());
		newExtension->setAnisotropyRotation(this->getAnisotropyRotation());
		newExtension->setAnisotropyTexture(this->getAnisotropyTexture());
		if(this->osgAnisotropyTexture.valid())
			newExtension->osgAnisotropyTexture = (osg::Texture2D*)this->osgAnisotropyTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		return newExtension;
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

	GltfExtension* clone() override {
		KHR_materials_emissive_strength* newExtension = new KHR_materials_emissive_strength;
		newExtension->setEmissiveStrength(this->getEmissiveStrength());
		return newExtension;
	}
};

struct KHR_materials_variants :GltfExtension
{
	KHR_materials_variants() :GltfExtension("KHR_materials_variants") {}
	GltfExtension* clone() override {
		return new KHR_materials_emissive_strength;
	}
};

struct KHR_materials_unlit :GltfExtension
{
	KHR_materials_unlit() :GltfExtension("KHR_materials_unlit") {}
	GltfExtension* clone() override {
		return new KHR_materials_unlit;
	}
};
#pragma endregion

#pragma region SpecularGlossiness
struct KHR_materials_pbrSpecularGlossiness :GltfExtension {
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

	GltfExtension* clone() override {
		KHR_materials_pbrSpecularGlossiness* newExtension = new KHR_materials_pbrSpecularGlossiness;
		newExtension->setDiffuseFactor(this->getDiffuseFactor());
		newExtension->setSpecularFactor(this->getSpecularFactor());
		newExtension->setGlossinessFactor(this->getGlossinessFactor());
		newExtension->setSpecularGlossinessTexture(this->getSpecularGlossinessTexture());
		newExtension->setDiffuseTexture(this->getDiffuseTexture());
		if(this->osgDiffuseTexture.valid())
			newExtension->osgDiffuseTexture = (osg::Texture2D*)this->osgDiffuseTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		if (this->osgSpecularGlossinessTexture.valid())
			newExtension->osgSpecularGlossinessTexture = (osg::Texture2D*)this->osgSpecularGlossinessTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		return newExtension;
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
	GltfExtension* clone() override {
		KHR_materials_sheen* newExtension = new KHR_materials_sheen;
		newExtension->setSheenColorFactor(this->getSheenColorFactor());
		newExtension->setSheenRoughnessFactor(this->getSheenRoughnessFactor());
		newExtension->setSheenColorTexture(this->getSheenColorTexture());
		newExtension->setSheenRoughnessTexture(this->getSheenRoughnessTexture());
		if (this->osgSheenColorTexture.valid())
			newExtension->osgSheenColorTexture = (osg::Texture2D*)this->osgSheenColorTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		if (this->osgSheenRoughnessTexture.valid())
			newExtension->osgSheenRoughnessTexture = (osg::Texture2D*)this->osgSheenRoughnessTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		return newExtension;
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
	GltfExtension* clone() override {
		KHR_materials_volume* newExtension = new KHR_materials_volume;
		newExtension->setAttenuationColor(this->getAttenuationColor());
		newExtension->setThicknessFactor(this->getThicknessFactor());
		newExtension->setAttenuationDistance(this->getAttenuationDistance());
		newExtension->getThicknessTexture(this->getThicknessTexture());
		if (this->osgThicknessTexture.valid())
			newExtension->osgThicknessTexture = (osg::Texture2D*)this->osgThicknessTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		return newExtension;
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
	GltfExtension* clone() override {
		KHR_materials_specular* newExtension = new KHR_materials_specular;
		newExtension->setSpecularColorFactor(this->getSpecularColorFactor());
		newExtension->setSpecularFactor(this->getSpecularFactor());
		newExtension->setSpecularTexture(this->getSpecularTexture());
		newExtension->setSpecularColorTexture(this->getSpecularColorTexture());
		if (this->osgSpecularTexture.valid())
			newExtension->osgSpecularTexture = (osg::Texture2D*)this->osgSpecularTexture->clone(osg::CopyOp::DEEP_COPY_ALL);
		if (this->osgSpecularColorTexture.valid())
			newExtension->osgSpecularColorTexture = (osg::Texture2D*)this->osgSpecularColorTexture->clone(osg::CopyOp::DEEP_COPY_ALL);

		return newExtension;
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
	GltfExtension* clone() override {
		KHR_materials_transmission* newExtension = new KHR_materials_transmission;
		newExtension->setTransmissionFactor(this->getTransmissionFactor());
		newExtension->setTransmissionTexture(this->getTransmissionTexture());
		if (this->osgTransmissionTexture.valid())
			newExtension->osgTransmissionTexture = (osg::Texture2D*)this->osgTransmissionTexture->clone(osg::CopyOp::DEEP_COPY_ALL);

		return newExtension;
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

	GltfExtension* clone() override {
		KHR_draco_mesh_compression* newExtension = new KHR_draco_mesh_compression;
		newExtension->setBufferView(this->getBufferView());
		newExtension->setPosition(this->getPosition());
		newExtension->setNormal(this->getNormal());
		newExtension->setTexCoord0(this->getTexCoord0());
		newExtension->setTexCoord1(this->getTexCoord1());
		newExtension->setWeights0(this->getWeights0());
		newExtension->setJoints0(this->getJoints0());
		newExtension->setBatchId(this->getBatchId());
		return newExtension;
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
	GltfExtension* clone() override {
		KHR_texture_basisu* newExtension = new KHR_texture_basisu;
		newExtension->setSource(this->getSource());

		return newExtension;
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
	GltfExtension* clone() override {
		KHR_texture_transform* newExtension = new KHR_texture_transform;
		newExtension->setOffset(this->getOffset());
		newExtension->setScale(this->getScale());
		newExtension->setTexCoord(this->getTexCoord());
		newExtension->setRotation(this->getRotation());

		return newExtension;
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
	GltfExtension* clone() override {
		EXT_texture_webp* newExtension = new EXT_texture_webp;
		newExtension->setSource(this->getSource());

		return newExtension;
	}
};
#pragma endregion
#endif // !OSG_GIS_PLUGINS_GLTF_EXTENSIONS_H
