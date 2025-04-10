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
#include <type_traits>
#define BASECOLOR_TEXTURE_FILENAME "osgGisPlugins-filename"

#define ORIGIN_WIDTH "origin-width"
#define ORIGIN_HEIGHT "origin-height"

#define TEX_TRANSFORM_BASECOLOR_TEXTURE_NAME "osgGisPlugins-basecolor-KHR_texture_transform"
#define TEX_TRANSFORM_BASECOLOR_OFFSET_X "osgGisPlugins-basecolor-offsetX"
#define TEX_TRANSFORM_BASECOLOR_OFFSET_Y "osgGisPlugins-basecolor-offsetY"
#define TEX_TRANSFORM_BASECOLOR_SCALE_X "osgGisPlugins-basecolor-scaleX"
#define TEX_TRANSFORM_BASECOLOR_SCALE_Y "osgGisPlugins-basecolor-scaleY"
#define TEX_TRANSFORM_BASECOLOR_TEXCOORD "osgGisPlugins-basecolor-texCoord"

#define TEX_TRANSFORM_NORMAL_TEXTURE_NAME "osgGisPlugins-normal-KHR_texture_transform"
#define TEX_TRANSFORM_NORMAL_OFFSET_X "osgGisPlugins-normal-offsetX"
#define TEX_TRANSFORM_NORMAL_OFFSET_Y "osgGisPlugins-normal-offsetY"
#define TEX_TRANSFORM_NORMAL_SCALE_X "osgGisPlugins-normal-scaleX"
#define TEX_TRANSFORM_NORMAL_SCALE_Y "osgGisPlugins-normal-scaleY"
#define TEX_TRANSFORM_NORMAL_TEXCOORD "osgGisPlugins-normal-texCoord"

#define TEX_TRANSFORM_OCCLUSION_TEXTURE_NAME "osgGisPlugins-occlusion-KHR_texture_transform"
#define TEX_TRANSFORM_OCCLUSION_OFFSET_X "osgGisPlugins-occlusion-offsetX"
#define TEX_TRANSFORM_OCCLUSION_OFFSET_Y "osgGisPlugins-occlusion-offsetY"
#define TEX_TRANSFORM_OCCLUSION_SCALE_X "osgGisPlugins-occlusion-scaleX"
#define TEX_TRANSFORM_OCCLUSION_SCALE_Y "osgGisPlugins-occlusion-scaleY"
#define TEX_TRANSFORM_OCCLUSION_TEXCOORD "osgGisPlugins-occlusion-texCoord"

#define TEX_TRANSFORM_EMISSIVE_TEXTURE_NAME "osgGisPlugins-emissive-KHR_texture_transform"
#define TEX_TRANSFORM_EMISSIVE_OFFSET_X "osgGisPlugins-emissive-offsetX"
#define TEX_TRANSFORM_EMISSIVE_OFFSET_Y "osgGisPlugins-emissive-offsetY"
#define TEX_TRANSFORM_EMISSIVE_SCALE_X "osgGisPlugins-emissive-scaleX"
#define TEX_TRANSFORM_EMISSIVE_SCALE_Y "osgGisPlugins-emissive-scaleY"
#define TEX_TRANSFORM_EMISSIVE_TEXCOORD "osgGisPlugins-emissive-texCoord"

#define TEX_TRANSFORM_MR_TEXTURE_NAME "osgGisPlugins-MR-KHR_texture_transform"
#define TEX_TRANSFORM_MR_OFFSET_X "osgGisPlugins-MR-offsetX"
#define TEX_TRANSFORM_MR_OFFSET_Y "osgGisPlugins-MR-offsetY"
#define TEX_TRANSFORM_MR_SCALE_X "osgGisPlugins-MR-scaleX"
#define TEX_TRANSFORM_MR_SCALE_Y "osgGisPlugins-MR-scaleY"
#define TEX_TRANSFORM_MR_TEXCOORD "osgGisPlugins-MR-texCoord"

#define TEX_TRANSFORM_DIFFUSE_TEXTURE_NAME "osgGisPlugins-diffuse-KHR_texture_transform"
#define TEX_TRANSFORM_DIFFUSE_OFFSET_X "osgGisPlugins-diffuse-offsetX"
#define TEX_TRANSFORM_DIFFUSE_OFFSET_Y "osgGisPlugins-diffuse-offsetY"
#define TEX_TRANSFORM_DIFFUSE_SCALE_X "osgGisPlugins-diffuse-scaleX"
#define TEX_TRANSFORM_DIFFUSE_SCALE_Y "osgGisPlugins-diffuse-scaleY"
#define TEX_TRANSFORM_DIFFUSE_TEXCOORD "osgGisPlugins-diffuse-texCoord"

#define TEX_TRANSFORM_SG_TEXTURE_NAME "osgGisPlugins-SG-KHR_texture_transform"
#define TEX_TRANSFORM_SG_OFFSET_X "osgGisPlugins-SG-offsetX"
#define TEX_TRANSFORM_SG_OFFSET_Y "osgGisPlugins-SG-offsetY"
#define TEX_TRANSFORM_SG_SCALE_X "osgGisPlugins-SG-scaleX"
#define TEX_TRANSFORM_SG_SCALE_Y "osgGisPlugins-SG-scaleY"
#define TEX_TRANSFORM_SG_TEXCOORD "osgGisPlugins-SG-texCoord"

namespace osgGISPlugins
{
    struct GltfExtension
    {
    protected:
        static bool compareValue(const tinygltf::Value::Object& val1,
            const tinygltf::Value::Object& val2) {
            if (val1.size() != val2.size()) return false;

            for (const auto& pair : val1) {
                auto it = val2.find(pair.first);
                if (it == val2.end()) return false;

                const tinygltf::Value& v1 = pair.second;
                const tinygltf::Value& v2 = it->second;

                if (v1.Type() != v2.Type()) return false;

                switch (v1.Type()) {
                case tinygltf::Type::REAL_TYPE:
                    if (!osg::equivalent(v1.Get<double>(), v2.Get<double>())) return false;
                    break;
                case tinygltf::Type::INT_TYPE:
                    if (v1.Get<int>() != v2.Get<int>()) return false;
                    break;
                case tinygltf::Type::BOOL_TYPE:
                    if (v1.Get<bool>() != v2.Get<bool>()) return false;
                    break;
                case tinygltf::Type::STRING_TYPE:
                    if (v1.Get<std::string>() != v2.Get<std::string>()) return false;
                    break;
                case tinygltf::Type::ARRAY_TYPE:
                    if (!compareArray(v1.Get<tinygltf::Value::Array>(),
                        v2.Get<tinygltf::Value::Array>())) return false;
                    break;
                case tinygltf::Type::OBJECT_TYPE:
                    if (!compareValue(v1.Get<tinygltf::Value::Object>(),
                        v2.Get<tinygltf::Value::Object>())) return false;
                    break;
                default:
                    return false;
                }
            }
            return true;
        }

        static bool compareArray(const tinygltf::Value::Array& arr1,
            const tinygltf::Value::Array& arr2) {
            if (arr1.size() != arr2.size()) return false;

            for (size_t i = 0; i < arr1.size(); ++i) {
                const tinygltf::Value& v1 = arr1[i];
                const tinygltf::Value& v2 = arr2[i];

                if (v1.Type() != v2.Type()) return false;

                switch (v1.Type()) {
                case tinygltf::Type::REAL_TYPE:
                    if (!osg::equivalent(v1.Get<double>(), v2.Get<double>())) return false;
                    break;
                case tinygltf::Type::INT_TYPE:
                    if (v1.Get<int>() != v2.Get<int>()) return false;
                    break;
default: break;
                // ... 其他类型的比较
                }
            }
            return true;
        }

        static bool compareTexture2D(const osg::ref_ptr<osg::Texture2D>& texture1, const osg::ref_ptr<osg::Texture2D>& texture2)
        {
            if (texture1 == texture2)
            {
                return true;
            }
            if (texture1.valid() != texture2.valid())
            {
                return false;
            }
            if (!texture1)
            {
                return true;
            }

            if (texture1->getWrap(osg::Texture::WRAP_S) != texture2->getWrap(osg::Texture::WRAP_S))
            {
                return false;
            }
            if (texture1->getWrap(osg::Texture::WRAP_T) != texture2->getWrap(osg::Texture::WRAP_T))
            {
                return false;
            }
            if (texture1->getWrap(osg::Texture::WRAP_R) != texture2->getWrap(osg::Texture::WRAP_R))
            {
                return false;
            }
            if (texture1->getFilter(osg::Texture::MIN_FILTER) != texture2->getFilter(osg::Texture::MIN_FILTER))
            {
                return false;
            }
            if (texture1->getFilter(osg::Texture::MAG_FILTER) != texture2->getFilter(osg::Texture::MAG_FILTER))
            {
                return false;
            }

            if (texture1->getNumImages() != texture2->getNumImages())
            {
                return false;
            }

            const osg::ref_ptr<osg::Image> img1 = texture1->getImage();
            const osg::ref_ptr<osg::Image> img2 = texture2->getImage();
            if (img1->getFileName() != img2->getFileName())
            {
                return false;
            }
            if (img1->s() != img2->s())
            {
                return false;
            }
            if (img1->r() != img2->r())
            {
                return false;
            }
            if (img1->t() != img2->t())
            {
                return false;
            }
            if (img1->getTotalDataSize() != img2->getTotalDataSize())
            {
                return false;
            }
            return true;
        }
    public:
        tinygltf::Value::Object value;
        std::string name;

        GltfExtension(const std::string& name) : name(name) {}

        GltfExtension() = default;
        virtual ~GltfExtension() = default;
        virtual bool compare(const GltfExtension& other) const {
            if (this == &other) return true;
            if (name != other.name) return false;
            return compareValue(value, other.value);
        }

        virtual GltfExtension* clone() = 0;

        template <typename T>
        T Get(const std::string& key) const
        {
            const tinygltf::Value::Object::const_iterator item = value.find(key);
            if (item != value.end())
            {
                return item->second.Get<T>();
            }
            return getDefault<T>();
        }

        virtual tinygltf::Value GetValue()
        {
            return tinygltf::Value(value);
        }

        virtual tinygltf::Value::Object& GetValueObject()
        {
            return value;
        }

        virtual void SetValue(const tinygltf::Value::Object& val)
        {
            value = val;
        }

        template <typename T, size_t N>
        std::array<T, N> GetArray(const std::string& key) const
        {
            std::array<T, N> result = { 0.0 }; // 初始化为 0
            const tinygltf::Value::Object::const_iterator item = value.find(key);
            if (item != value.end())
            {
                tinygltf::Value::Array tinygltfArray = item->second.Get<tinygltf::Value::Array>();
                if (tinygltfArray.size() == N)
                {
                    for (size_t i = 0; i < N; ++i)
                    {
                        result[i] = tinygltfArray[i].Get<T>();
                    }
                }
            }
            return result;
        }

        template <typename T>
        typename std::enable_if<!std::is_same<T, int>::value>::type
            checkAndSet(const T& val, tinygltf::Value& tinygltfVal)
        {
            tinygltfVal = tinygltf::Value(val);
        }

        template <typename T>
        typename std::enable_if<std::is_same<T, int>::value>::type
            checkAndSet(const T& val, tinygltf::Value& tinygltfVal)
        {
            if (val != -1)
            {
                tinygltfVal = tinygltf::Value(val);
            }
        }

        template <typename T>
        void Set(const std::string& key, const T& val)
        {
            tinygltf::Value tinygltfVal;
            checkAndSet(val, tinygltfVal);
            value[key] = std::move(tinygltfVal);
        }

        static tinygltf::Value::Object TextureInfo2Object(const tinygltf::TextureInfo& textureInfo)
        {
            tinygltf::Value::Object obj;

            obj["index"] = tinygltf::Value(textureInfo.index);
            obj["texCoord"] = tinygltf::Value(textureInfo.texCoord);
            if (textureInfo.extensions.size() > 0)
            {
                tinygltf::Value::Object extensionsObj;
                for (const auto& ext : textureInfo.extensions)
                {
                    extensionsObj[ext.first] = ext.second;
                }
                obj["extensions"] = tinygltf::Value(extensionsObj);
            }
            if (textureInfo.extras.Type() != tinygltf::NULL_TYPE)
            {
                obj["extras"] = textureInfo.extras;
            }

            return obj;
        }

        static tinygltf::TextureInfo Object2TextureInfo(const tinygltf::Value::Object& obj)
        {
            tinygltf::TextureInfo textureInfo;

            // 提取 index 字段
            const auto indexIt = obj.find("index");
            if (indexIt != obj.end() && indexIt->second.IsInt())
            {
                textureInfo.index = indexIt->second.Get<int>();
            }

            // 提取 texCoord 字段
            const auto texCoordIt = obj.find("texCoord");
            if (texCoordIt != obj.end() && texCoordIt->second.IsInt())
            {
                textureInfo.texCoord = texCoordIt->second.Get<int>();
            }

            // 提取 extensions 字段
            const auto extensionsIt = obj.find("extensions");
            if (extensionsIt != obj.end() && extensionsIt->second.IsObject())
            {
                const tinygltf::Value::Object& extensionsObj = extensionsIt->second.Get<tinygltf::Value::Object>();
                for (const auto& ext : extensionsObj)
                {
                    textureInfo.extensions[ext.first] = ext.second;
                }
            }

            // 提取 extras 字段
            const auto extrasIt = obj.find("extras");
            if (extrasIt != obj.end())
            {
                textureInfo.extras = extrasIt->second;
            }

            return textureInfo;
        }

    private:
        template <typename T>
        struct Default;

        template <typename T>
        T getDefault() const
        {
            return Default<T>::value();
        }
    };

    template <>
    struct GltfExtension::Default<int>
    {
        static int value() { return -1; }
    };

    template <>
    struct  GltfExtension::Default<std::string>
    {
        static std::string value() { return ""; }
    };

    template <>
    struct  GltfExtension::Default<double>
    {
        static double value() { return 0.0; }
    };

    template <typename T, size_t N>
    struct  GltfExtension::Default<std::array<T, N>>
    {
        static std::array<T, N> value() { return {0.0}; }
    };


#pragma region material
#pragma region common
    // cesium does not support
    struct KHR_materials_clearcoat : GltfExtension
    {
        tinygltf::TextureInfo clearcoatTexture, clearcoatNormalTexture, clearcoatRoughnessTexture;
        KHR_materials_clearcoat() : GltfExtension("KHR_materials_clearcoat")
        {
            setClearcoatFactor(0.0);
            setClearcoatRoughnessFactor(0.0);
        }
        osg::ref_ptr<osg::Texture2D> osgClearcoatTexture, osgClearcoatRoughnessTexture, osgClearcoatNormalTexture;
        double getClearcoatFactor() const
        {
            return Get<double>("clearcoatFactor");
        }

        void setClearcoatFactor(const double val)
        {
            Set("clearcoatFactor", val);
        }

        double getClearcoatRoughnessFactor() const
        {
            return Get<double>("clearcoatRoughnessFactor");
        }

        void setClearcoatRoughnessFactor(const double val)
        {
            Set("clearcoatRoughnessFactor", val);
        }

        tinygltf::Value GetValue() override
        {
            Set("clearcoatTexture", TextureInfo2Object(clearcoatTexture));
            Set("clearcoatNormalTexture", TextureInfo2Object(clearcoatNormalTexture));
            Set("clearcoatRoughnessTexture", TextureInfo2Object(clearcoatRoughnessTexture));
            return tinygltf::Value(value);
        }

        tinygltf::Value::Object &GetValueObject() override
        {
            Set("clearcoatTexture", TextureInfo2Object(clearcoatTexture));
            Set("clearcoatNormalTexture", TextureInfo2Object(clearcoatNormalTexture));
            Set("clearcoatRoughnessTexture", TextureInfo2Object(clearcoatRoughnessTexture));
            return value;
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* clearcoat = dynamic_cast<const KHR_materials_clearcoat*>(&other);
            if (!clearcoat) return false;

            if (!GltfExtension::compare(other)) return false;

            if (compareTexture2D(osgClearcoatTexture, clearcoat->osgClearcoatTexture)) return false;
            if (compareTexture2D(osgClearcoatRoughnessTexture, clearcoat->osgClearcoatRoughnessTexture)) return false;
            if (compareTexture2D(osgClearcoatNormalTexture, clearcoat->osgClearcoatNormalTexture)) return false;


            return true;
        }

        GltfExtension *clone() override
        {
            KHR_materials_clearcoat *newExtension = new KHR_materials_clearcoat;
            newExtension->setClearcoatFactor(this->getClearcoatFactor());
            newExtension->setClearcoatRoughnessFactor(this->getClearcoatRoughnessFactor());
            newExtension->clearcoatTexture = this->clearcoatTexture;
            newExtension->clearcoatNormalTexture = this->clearcoatNormalTexture;
            newExtension->clearcoatRoughnessTexture = this->clearcoatRoughnessTexture;
            if (this->osgClearcoatTexture.valid())
            {
                newExtension->osgClearcoatTexture = dynamic_cast<osg::Texture2D*>(this->osgClearcoatTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            if (this->osgClearcoatRoughnessTexture.valid())
            {
                newExtension->osgClearcoatRoughnessTexture = dynamic_cast<osg::Texture2D*>(this->osgClearcoatRoughnessTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            if (this->osgClearcoatNormalTexture.valid())
            {
                newExtension->osgClearcoatNormalTexture = dynamic_cast<osg::Texture2D*>(this->osgClearcoatNormalTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            return newExtension;
        }
    };

    // cesium does not support
    struct KHR_materials_anisotropy : GltfExtension
    {
        tinygltf::TextureInfo anisotropyTextureInfo;
        osg::ref_ptr<osg::Texture2D> osgAnisotropyTexture;

        KHR_materials_anisotropy() : GltfExtension("KHR_materials_anisotropy")
        {
            setAnisotropyStrength(0.0);
            setAnisotropyRotation(0.0);
        }

        double getAnisotropyStrength() const
        {
            return Get<double>("anisotropyStrength");
        }

        void setAnisotropyStrength(const double val)
        {
            Set("anisotropyStrength", val);
        }

        double getAnisotropyRotation() const
        {
            return Get<double>("anisotropyRotation");
        }

        void setAnisotropyRotation(const double val)
        {
            Set("anisotropyRotation", val);
        }

        tinygltf::Value GetValue() override
        {
            Set("anisotropyTexture", TextureInfo2Object(anisotropyTextureInfo));
            return tinygltf::Value(value);
        }

        tinygltf::Value::Object &GetValueObject() override
        {
            Set("anisotropyTexture", TextureInfo2Object(anisotropyTextureInfo));
            return value;
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* anisotropy = dynamic_cast<const KHR_materials_anisotropy*>(&other);
            if (!anisotropy) return false;

            if (!GltfExtension::compare(other)) return false;

            if (compareTexture2D(osgAnisotropyTexture, anisotropy->osgAnisotropyTexture)) return false;

            return true;
        }

        GltfExtension *clone() override
        {
            KHR_materials_anisotropy *newExtension = new KHR_materials_anisotropy;
            newExtension->setAnisotropyStrength(this->getAnisotropyStrength());
            newExtension->setAnisotropyRotation(this->getAnisotropyRotation());
            newExtension->anisotropyTextureInfo = this->anisotropyTextureInfo;
            if (this->osgAnisotropyTexture.valid())
            {
                newExtension->osgAnisotropyTexture = dynamic_cast<osg::Texture2D*>(this->osgAnisotropyTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            return newExtension;
        }
    };

    // conflict KHR_materials_unlit,and cesium does not support
    struct KHR_materials_emissive_strength : GltfExtension
    {
        KHR_materials_emissive_strength() : GltfExtension("KHR_materials_emissive_strength")
        {
            setEmissiveStrength(0.0);
        }
        double getEmissiveStrength() const
        {
            return Get<double>("emissiveStrength");
        }

        void setEmissiveStrength(const double val)
        {
            Set("emissiveStrength", val);
        }

        GltfExtension *clone() override
        {
            KHR_materials_emissive_strength *newExtension = new KHR_materials_emissive_strength;
            newExtension->setEmissiveStrength(this->getEmissiveStrength());
            return newExtension;
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* strength = dynamic_cast<const KHR_materials_emissive_strength*>(&other);
            if (!strength) return false;

            if (!GltfExtension::compare(other)) return false;

            return true;
        }
    };

    struct KHR_materials_variants : GltfExtension
    {
        KHR_materials_variants() : GltfExtension("KHR_materials_variants") {}
        GltfExtension *clone() override
        {
            return new KHR_materials_variants;
        }
    };

    struct KHR_materials_unlit : GltfExtension
    {
        KHR_materials_unlit() : GltfExtension("KHR_materials_unlit") {}
        GltfExtension *clone() override
        {
            return new KHR_materials_unlit;
        }
    };
#pragma endregion

#pragma region SpecularGlossiness
    struct KHR_materials_pbrSpecularGlossiness : GltfExtension
    {
        KHR_materials_pbrSpecularGlossiness() : GltfExtension("KHR_materials_pbrSpecularGlossiness")
        {
            setDiffuseFactor({1.0, 1.0, 1.0, 1.0});
            setSpecularFactor({1.0, 1.0, 1.0});
            setGlossinessFactor(0.0);
        }

        tinygltf::TextureInfo specularGlossinessTexture, diffuseTexture;
        osg::ref_ptr<osg::Texture2D> osgSpecularGlossinessTexture, osgDiffuseTexture;

        std::array<double, 4> getDiffuseFactor() const
        {
            return GetArray<double, 4>("diffuseFactor");
        }

        void setDiffuseFactor(const std::array<double, 4> &val)
        {
            Set("diffuseFactor", tinygltf::Value::Array(val.begin(), val.end()));
        }

        std::array<double, 3> getSpecularFactor() const
        {
            return GetArray<double, 3>("specularFactor");
        }

        void setSpecularFactor(const std::array<double, 3> &val)
        {
            Set("specularFactor", tinygltf::Value::Array(val.begin(), val.end()));
        }

        double getGlossinessFactor() const
        {
            return Get<double>("glossinessFactor");
        }

        void setGlossinessFactor(const double val)
        {
            Set("glossinessFactor", val);
        }

        tinygltf::Value GetValue() override
        {
			if (specularGlossinessTexture.index != -1)
				Set("specularGlossinessTexture", TextureInfo2Object(specularGlossinessTexture));
			if (diffuseTexture.index != -1)
				Set("diffuseTexture", TextureInfo2Object(diffuseTexture));
            return tinygltf::Value(value);
        }

        tinygltf::Value::Object &GetValueObject() override
        {
            Set("specularGlossinessTexture", TextureInfo2Object(specularGlossinessTexture));
            Set("diffuseTexture", TextureInfo2Object(diffuseTexture));
            return value;
        }

        void SetValue(const tinygltf::Value::Object &val) override
        {
            specularGlossinessTexture = Object2TextureInfo(val.at("specularGlossinessTexture").Get<tinygltf::Value::Object>());
            diffuseTexture = Object2TextureInfo(val.at("diffuseTexture").Get<tinygltf::Value::Object>());

            value = val;
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* pbrSpecularGlossiness = dynamic_cast<const KHR_materials_pbrSpecularGlossiness*>(&other);
            if (!pbrSpecularGlossiness) return false;

            if (!GltfExtension::compare(other)) return false;

            if (compareTexture2D(osgSpecularGlossinessTexture, pbrSpecularGlossiness->osgSpecularGlossinessTexture)) return false;
            if (compareTexture2D(osgDiffuseTexture, pbrSpecularGlossiness->osgDiffuseTexture)) return false;


            return true;
        }

        GltfExtension *clone() override
        {
            KHR_materials_pbrSpecularGlossiness *newExtension = new KHR_materials_pbrSpecularGlossiness;
            newExtension->setDiffuseFactor(this->getDiffuseFactor());
            newExtension->setSpecularFactor(this->getSpecularFactor());
            newExtension->setGlossinessFactor(this->getGlossinessFactor());
            newExtension->specularGlossinessTexture = this->specularGlossinessTexture;
            newExtension->diffuseTexture = this->diffuseTexture;
            if (this->osgDiffuseTexture.valid())
            {
                newExtension->osgDiffuseTexture = dynamic_cast<osg::Texture2D*>(this->osgDiffuseTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            if (this->osgSpecularGlossinessTexture.valid())
            {
                newExtension->osgSpecularGlossinessTexture = dynamic_cast<osg::Texture2D*>(this->osgSpecularGlossinessTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            return newExtension;
        }
    };
#pragma endregion

#pragma region MetallicRoughness
    // conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
    struct KHR_materials_ior : GltfExtension
    {
        KHR_materials_ior() : GltfExtension("KHR_materials_ior")
        {
            setIor(1.5);
        }
        double getIor() const
        {
            return Get<double>("ior");
        }

        void setIor(const double val)
        {
            Set("ior", val);
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* ior = dynamic_cast<const KHR_materials_ior*>(&other);
            if (!ior) return false;

            if (!GltfExtension::compare(other)) return false;

            return true;
        }

        GltfExtension* clone() override
        {
            KHR_materials_ior* newExtension = new KHR_materials_ior;
            newExtension->setIor(this->getIor());
            return newExtension;
        }
    };

    // conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness
    struct KHR_materials_sheen : GltfExtension
    {
        KHR_materials_sheen() : GltfExtension("KHR_materials_sheen")
        {
            setSheenColorFactor({1.0, 1.0, 1.0});
            setSheenRoughnessFactor(0.0);
        }

        tinygltf::TextureInfo sheenColorTexture, sheenRoughnessTexture;
        osg::ref_ptr<osg::Texture2D> osgSheenColorTexture, osgSheenRoughnessTexture;

        std::array<double, 3> getSheenColorFactor() const
        {
            return GetArray<double, 3>("sheenColorFactor");
        }

        void setSheenColorFactor(const std::array<double, 3> &val)
        {
            Set("sheenColorFactor", tinygltf::Value::Array(val.begin(), val.end()));
        }

        double getSheenRoughnessFactor() const
        {
            return Get<double>("sheenRoughnessFactor");
        }

        void setSheenRoughnessFactor(const double val)
        {
            Set("sheenRoughnessFactor", val);
        }

        tinygltf::Value GetValue() override
        {
            Set("sheenColorTexture", TextureInfo2Object(sheenColorTexture));
            Set("sheenRoughnessTexture", TextureInfo2Object(sheenRoughnessTexture));
            return tinygltf::Value(value);
        }

        tinygltf::Value::Object &GetValueObject() override
        {
            Set("sheenColorTexture", TextureInfo2Object(sheenColorTexture));
            Set("sheenRoughnessTexture", TextureInfo2Object(sheenRoughnessTexture));
            return value;
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* sheen = dynamic_cast<const KHR_materials_sheen*>(&other);
            if (!sheen) return false;

            if (!GltfExtension::compare(other)) return false;

            if (!compareTexture2D(osgSheenColorTexture, sheen->osgSheenColorTexture)) return false;

            if (!compareTexture2D(osgSheenRoughnessTexture, sheen->osgSheenRoughnessTexture)) return false;


            return true;
        }

        GltfExtension *clone() override
        {
            KHR_materials_sheen *newExtension = new KHR_materials_sheen;
            newExtension->setSheenColorFactor(this->getSheenColorFactor());
            newExtension->setSheenRoughnessFactor(this->getSheenRoughnessFactor());
            newExtension->sheenColorTexture = this->sheenColorTexture;
            newExtension->sheenRoughnessTexture = this->sheenRoughnessTexture;
            if (this->osgSheenColorTexture.valid())
            {
                newExtension->osgSheenColorTexture = dynamic_cast<osg::Texture2D*>(this->osgSheenColorTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            if (this->osgSheenRoughnessTexture.valid())
            {
                newExtension->osgSheenRoughnessTexture = dynamic_cast<osg::Texture2D*>(this->osgSheenRoughnessTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            return newExtension;
        }
    };

    // conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness
    struct KHR_materials_volume : GltfExtension
    {
        KHR_materials_volume() : GltfExtension("KHR_materials_volume")
        {
            setAttenuationColor({1.0, 1.0, 1.0});
            setThicknessFactor(0.0);
            setAttenuationDistance(std::numeric_limits<double>::infinity());
        }

        tinygltf::TextureInfo thicknessTexture;
        osg::ref_ptr<osg::Texture2D> osgThicknessTexture;

        std::array<double, 3> getAttenuationColor() const
        {
            return GetArray<double, 3>("attenuationColor");
        }

        void setAttenuationColor(const std::array<double, 3> &val)
        {
            Set("attenuationColor", tinygltf::Value::Array(val.begin(), val.end()));
        }

        double getThicknessFactor() const
        {
            return Get<double>("thicknessFactor");
        }

        void setThicknessFactor(const double val)
        {
            Set("thicknessFactor", val);
        }

        double getAttenuationDistance() const
        {
            return Get<double>("attenuationDistance");
        }

        void setAttenuationDistance(const double val)
        {
            Set("attenuationDistance", val);
        }

        tinygltf::Value GetValue() override
        {
            Set("thicknessTexture", TextureInfo2Object(thicknessTexture));
            return tinygltf::Value(value);
        }

        tinygltf::Value::Object &GetValueObject() override
        {
            Set("thicknessTexture", TextureInfo2Object(thicknessTexture));
            return value;
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* volume = dynamic_cast<const KHR_materials_volume*>(&other);
            if (!volume) return false;

            if (!GltfExtension::compare(other)) return false;

            if (!compareTexture2D(osgThicknessTexture, volume->osgThicknessTexture)) return false;

            return true;
        }

        GltfExtension *clone() override
        {
            KHR_materials_volume *newExtension = new KHR_materials_volume;
            newExtension->setAttenuationColor(this->getAttenuationColor());
            newExtension->setThicknessFactor(this->getThicknessFactor());
            newExtension->setAttenuationDistance(this->getAttenuationDistance());
            newExtension->thicknessTexture = this->thicknessTexture;
            if (this->osgThicknessTexture.valid())
            {
                newExtension->osgThicknessTexture = dynamic_cast<osg::Texture2D*>(this->osgThicknessTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            return newExtension;
        }
    };

    // conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
    struct KHR_materials_specular : GltfExtension
    {
        KHR_materials_specular() : GltfExtension("KHR_materials_specular")
        {
            setSpecularColorFactor({1.0, 1.0, 1.0});
            setSpecularFactor(0.0);
        }

        tinygltf::TextureInfo specularTexture, specularColorTexture;
        osg::ref_ptr<osg::Texture2D> osgSpecularTexture, osgSpecularColorTexture;

        std::array<double, 3> getSpecularColorFactor() const
        {
            return GetArray<double, 3>("specularColorFactor");
        }

        void setSpecularColorFactor(const std::array<double, 3> &val)
        {
            Set("specularColorFactor", tinygltf::Value::Array(val.begin(), val.end()));
        }

        double getSpecularFactor() const
        {
            return Get<double>("specularFactor");
        }

        void setSpecularFactor(const double val)
        {
            Set("specularFactor", val);
        }

        tinygltf::Value GetValue() override
        {
            Set("specularTexture", TextureInfo2Object(specularTexture));
            Set("specularColorTexture", TextureInfo2Object(specularColorTexture));
            return tinygltf::Value(value);
        }

        tinygltf::Value::Object &GetValueObject() override
        {
            Set("specularTexture", TextureInfo2Object(specularTexture));
            Set("specularColorTexture", TextureInfo2Object(specularColorTexture));
            return value;
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* specular = dynamic_cast<const KHR_materials_specular*>(&other);
            if (!specular) return false;

            if (!GltfExtension::compare(other)) return false;

            if (!compareTexture2D(osgSpecularTexture, specular->osgSpecularTexture)) return false;

            if (!compareTexture2D(osgSpecularColorTexture, specular->osgSpecularColorTexture)) return false;


            return true;
        }

        GltfExtension *clone() override
        {
            KHR_materials_specular *newExtension = new KHR_materials_specular;
            newExtension->setSpecularColorFactor(this->getSpecularColorFactor());
            newExtension->setSpecularFactor(this->getSpecularFactor());
            newExtension->specularTexture = this->specularTexture;
            newExtension->specularColorTexture = this->specularColorTexture;
            if (this->osgSpecularTexture.valid())
            {
                newExtension->osgSpecularTexture = dynamic_cast<osg::Texture2D*>(this->osgSpecularTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }
            if (this->osgSpecularColorTexture.valid())
            {
                newExtension->osgSpecularColorTexture = dynamic_cast<osg::Texture2D*>(this->osgSpecularColorTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }

            return newExtension;
        }
    };

    // conflict KHR_materials_unlit and KHR_materials_pbrSpecularGlossiness,and cesium does not support
    struct KHR_materials_transmission : GltfExtension
    {
        KHR_materials_transmission() : GltfExtension("KHR_materials_transmission")
        {
            setTransmissionFactor(0.0);
        }

        tinygltf::TextureInfo transmissionTexture;
        osg::ref_ptr<osg::Texture2D> osgTransmissionTexture;

        double getTransmissionFactor() const
        {
            return Get<double>("transmissionFactor");
        }

        void setTransmissionFactor(const double val)
        {
            Set("transmissionFactor", val);
        }

        tinygltf::Value GetValue() override
        {
            Set("transmissionTexture", TextureInfo2Object(transmissionTexture));
            return tinygltf::Value(value);
        }

        tinygltf::Value::Object &GetValueObject() override
        {
            Set("transmissionTexture", TextureInfo2Object(transmissionTexture));
            return value;
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* transmission = dynamic_cast<const KHR_materials_transmission*>(&other);
            if (!transmission) return false;

            if (!GltfExtension::compare(other)) return false;

            if (!compareTexture2D(osgTransmissionTexture, transmission->osgTransmissionTexture)) return false;

            return true;
        }

        GltfExtension *clone() override
        {
            KHR_materials_transmission *newExtension = new KHR_materials_transmission;
            newExtension->setTransmissionFactor(this->getTransmissionFactor());
            newExtension->transmissionTexture = this->transmissionTexture;
            if (this->osgTransmissionTexture.valid())
            {
                newExtension->osgTransmissionTexture = dynamic_cast<osg::Texture2D*>(this->osgTransmissionTexture->clone(osg::CopyOp::DEEP_COPY_ALL));
            }

            return newExtension;
        }
    };
#pragma endregion
#pragma endregion

#pragma region mesh
    struct KHR_draco_mesh_compression : GltfExtension
    {
        KHR_draco_mesh_compression() : GltfExtension("KHR_draco_mesh_compression")
        {
            setPosition(-1);
            setNormal(-1);
            setTexCoord0(-1);
            setTexCoord1(-1);
            setWeights0(-1);
            setJoints0(-1);
            setBatchId(-1);
        }
        int getPosition() const
        {
            return Get<int>("POSITION");
        }
        void setPosition(const int val)
        {
            Set("POSITION", val);
        }
        int getNormal() const
        {
            return Get<int>("NORMAL");
        }
        void setNormal(const int val)
        {
            Set("NORMAL", val);
        }
        int getTexCoord0() const
        {
            return Get<int>("TEXCOORD_0");
        }
        void setTexCoord0(const int val)
        {
            Set("TEXCOORD_0", val);
        }
        int getTexCoord1() const
        {
            return Get<int>("TEXCOORD_1");
        }
        void setTexCoord1(const int val)
        {
            Set("TEXCOORD_1", val);
        }
        int getWeights0() const
        {
            return Get<int>("WEIGHTS_0");
        }
        void setWeights0(const int val)
        {
            Set("WEIGHTS_0", val);
        }
        int getJoints0() const
        {
            return Get<int>("JOINTS_0");
        }
        void setJoints0(const int val)
        {
            Set("JOINTS_0", val);
        }
        int getBatchId() const
        {
            return Get<int>("_BATCHID");
        }
        void setBatchId(const int val)
        {
            Set("_BATCHID", val);
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* compression = dynamic_cast<const KHR_draco_mesh_compression*>(&other);
            if (!compression) return false;

            if (!GltfExtension::compare(other)) return false;

            return true;
        }

        GltfExtension *clone() override
        {
            KHR_draco_mesh_compression *newExtension = new KHR_draco_mesh_compression;
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

    struct KHR_mesh_quantization : GltfExtension
    {
        KHR_mesh_quantization() : GltfExtension("KHR_mesh_quantization") {}

        GltfExtension* clone() override
        {
            KHR_mesh_quantization* newExtension = new KHR_mesh_quantization;
            return newExtension;
        }
    };

    struct EXT_meshopt_compression : GltfExtension
    {
        EXT_meshopt_compression() : GltfExtension("EXT_meshopt_compression")
        {
            setBuffer(-1);
            setByteLength(-1);
            setByteStride(-1);
            setCount(-1);
            setMode("");
        }
        void setBuffer(const int val)
        {
            Set("buffer", val);
        }
        int getBuffer() const {
            return Get<int>("buffer");
        }
        void setByteLength(const int val)
        {
            Set("byteLength", val);
        }
        int getByteLength() const {
            return Get<int>("byteLength");
        }
        void setByteStride(const int val)
        {
            Set("byteStride", val);
        }
        int getByteStride() const {
            return Get<int>("byteStride");
        }
        void setCount(const int val)
        {
            Set("count", val);
        }
        int getCount() const {
            return Get<int>("count");
        }
        void setMode(const std::string& val)
        {
            Set("mode", val);
        }
        std::string getMode() const {
            return Get<std::string>("mode");
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* compression = dynamic_cast<const EXT_meshopt_compression*>(&other);
            if (!compression) return false;

            if (!GltfExtension::compare(other)) return false;

            return true;
        }

        GltfExtension* clone() override
        {
            EXT_meshopt_compression* newExtension = new EXT_meshopt_compression;
            newExtension->setBuffer(this->getBuffer());
            newExtension->setByteLength(this->getByteLength());
            newExtension->setByteStride(this->getByteStride());
            newExtension->setCount(this->getCount());
            newExtension->setMode(this->getMode());
            return newExtension;
        }
    };

    struct EXT_mesh_gpu_instancing : GltfExtension
    {
        EXT_mesh_gpu_instancing() : GltfExtension("EXT_mesh_gpu_instancing") {}

        GltfExtension* clone() override
        {
            EXT_mesh_gpu_instancing* newExtension = new EXT_mesh_gpu_instancing;
            return newExtension;
        }
    };
#pragma endregion

#pragma region texture
    struct KHR_texture_basisu : GltfExtension
    {
        KHR_texture_basisu() : GltfExtension("KHR_texture_basisu")
        {
            setSource(-1);
        }
        int getSource() const
        {
            return Get<int>("source");
        }
        void setSource(const int val)
        {
            Set("source", val);
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* basisu = dynamic_cast<const KHR_texture_basisu*>(&other);
            if (!basisu) return false;

            if (!GltfExtension::compare(other)) return false;

            return true;
        }

        GltfExtension *clone() override
        {
            KHR_texture_basisu *newExtension = new KHR_texture_basisu;
            newExtension->setSource(this->getSource());

            return newExtension;
        }
    };

    struct KHR_texture_transform : GltfExtension
    {
        KHR_texture_transform() : GltfExtension("KHR_texture_transform")
        {
            setOffset({0.0, 0.0});
            setScale({1.0, 1.0});
            setTexCoord(0);
            setRotation(0.0);
        }
        std::array<double, 2> getOffset() const
        {
            return GetArray<double, 2>("offset");
        }
        void setOffset(const std::array<double, 2> &val)
        {
            Set("offset", tinygltf::Value::Array(val.begin(), val.end()));
        }
        std::array<double, 2> getScale() const
        {
            return GetArray<double, 2>("scale");
        }
        void setScale(const std::array<double, 2> &val)
        {
            Set("scale", tinygltf::Value::Array(val.begin(), val.end()));
        }
        int getTexCoord() const
        {
            return Get<int>("texCoord");
        }
        void setTexCoord(const int val)
        {
            Set("texCoord", val);
        }
        double getRotation() const
        {
            return Get<double>("rotation");
        }
        void setRotation(const double val)
        {
            Set("rotation", val);
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* transform = dynamic_cast<const KHR_texture_transform*>(&other);
            if (!transform) return false;

            if (!GltfExtension::compare(other)) return false;

            return true;
        }

        GltfExtension *clone() override
        {
            KHR_texture_transform *newExtension = new KHR_texture_transform;
            newExtension->setOffset(this->getOffset());
            newExtension->setScale(this->getScale());
            newExtension->setTexCoord(this->getTexCoord());
            newExtension->setRotation(this->getRotation());

            return newExtension;
        }
    };

    struct EXT_texture_webp : GltfExtension
    {
        EXT_texture_webp() : GltfExtension("EXT_texture_webp")
        {
            setSource(-1);
        }
        int getSource() const
        {
            return Get<int>("source");
        }
        void setSource(const int val)
        {
            Set("source", val);
        }

        bool compare(const GltfExtension& other) const override {
            if (this == &other) return true;

            const auto* webp = dynamic_cast<const EXT_texture_webp*>(&other);
            if (!webp) return false;

            if (!GltfExtension::compare(other)) return false;

            return true;
        }


        GltfExtension *clone() override
        {
            EXT_texture_webp *newExtension = new EXT_texture_webp;
            newExtension->setSource(this->getSource());

            return newExtension;
        }
    };
#pragma endregion
}
#endif // !OSG_GIS_PLUGINS_GLTF_EXTENSIONS_H
