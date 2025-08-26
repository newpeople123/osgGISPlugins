#include <osgdb_fbx/fbxMaterialToOsgStateSet.h>
#include <osgdb_gltf/material/GltfMaterial.h>
#include <osgdb_gltf/material/GltfPbrMRMaterial.h>
#include <osgdb_gltf/material/GltfPbrSGMaterial.h>
#include <sstream>
#include <osg/TexMat>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osgDB/ConvertUTF>

using namespace osgGISPlugins;
TextureDetails::TextureDetails() :
	factor(1.0),
	scale(1.0, 1.0)
{
}

bool TextureDetails::transparent() const
{
	const osg::Image* image = texture.valid() ? texture->getImage() : 0;
	return image != 0 ? image->isImageTranslucent() : false;
}

void TextureDetails::assignTextureIfRequired(osg::StateSet* stateSet, unsigned int unit)
{
	if (!texture) return;

	stateSet->setTextureAttributeAndModes(unit, texture.get());
}

void TextureDetails::assignTexMatIfRequired(osg::StateSet* stateSet, unsigned int unit)
{
	if (scale.x() != 1.0 || scale.y() != 1.0)
	{
		// set UV scaling...
		stateSet->setTextureAttributeAndModes(unit, new osg::TexMat(osg::Matrix::scale(scale.x(), scale.y(), 1.0)), osg::StateAttribute::ON);
	}
}

static osg::Texture::WrapMode convertWrap(FbxFileTexture::EWrapMode wrap)
{
	return wrap == FbxFileTexture::eRepeat ?
		osg::Texture2D::REPEAT : osg::Texture2D::CLAMP_TO_EDGE;
}

template <typename T>
T getValue(const FbxProperty& props, std::string propName, const T& def) {
	const FbxProperty prop = props.FindHierarchical(propName.c_str());
	return prop.IsValid() ? prop.Get<T>() : def;
}

// Helpers to reduce duplication while preserving behavior
static void assignTextureDetails(osg::ref_ptr<TextureDetails>& dst,
	const osg::ref_ptr<osg::Texture2D>& texture,
	const FbxFileTexture* fileTexture)
{
	if (!texture) return;
	if (!dst) dst = new TextureDetails;
	dst->texture = texture;
	if (fileTexture) {
		dst->channel = fileTexture->UVSet.Get();
		dst->scale.set(fileTexture->GetScaleU(), fileTexture->GetScaleV());
	}
}

static void applyLightmapEmissionOverride(osg::Material* material,
	const osg::ref_ptr<TextureDetails>& emissive,
	bool lightmapTextures)
{
	if (!material || !lightmapTextures) return;
	if (emissive.valid())
	{
		osg::Vec4 diffuse = material->getDiffuse(osg::Material::FRONT_AND_BACK);
		material->setEmission(osg::Material::FRONT_AND_BACK, diffuse);
		material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0, 0, 0, diffuse.a()));
		material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0, 0, 0, diffuse.a()));
	}
}

StateSetContent FbxMaterialToOsgStateSet::convert(const FbxSurfaceMaterial* pFbxMat)
{
	FbxMaterialMap::const_iterator it = _fbxMaterialMap.find(pFbxMat);
	if (it != _fbxMaterialMap.end())
		return it->second;


	StateSetContent result;


	FbxString shadingModel = pFbxMat->ShadingModel.Get();

	const FbxSurfaceLambert* pFbxLambert = FbxCast<FbxSurfaceLambert>(pFbxMat);

	// diffuse map...
	result.diffuse = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse));

	// opacity map...
	double transparencyColorFactor = 1.0;
	bool useTransparencyColorFactor = false;

	const FbxProperty lOpacityProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sTransparentColor);
	if (lOpacityProperty.IsValid())
	{
		FbxDouble3 transparentColor = lOpacityProperty.Get<FbxDouble3>();
		// If transparent color is defined set the transparentFactor to gray scale value of transparentColor
		if (transparentColor[0] < 1.0 || transparentColor[1] < 1.0 || transparentColor[2] < 1.0) {
			transparencyColorFactor = transparentColor[0] * 0.30 + transparentColor[1] * 0.59 + transparentColor[2] * 0.11;
			useTransparencyColorFactor = true;
		}

		result.opacity = selectTextureDetails(lOpacityProperty);
	}

	// reflection map...
	result.reflection = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sReflection));

	// emissive map...
	result.emissive = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sEmissive));

	// ambient map...
	result.ambient = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sAmbient));

	// normal map...
	result.normalMap = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sNormalMap));

	// specular map...
	result.specular = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sSpecular));

	// shininess map...
	result.shininess = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sShininess));

	osg::ref_ptr<osg::Material> pOsgMat = new GltfPbrMRMaterial;
	result.material = pOsgMat;
	const FbxProperty topProp = pFbxMat->FindProperty("3dsMax", false);
	if (topProp.IsValid() && topProp.GetPropertyDataType() == FbxCompoundDT) {
		const FbxProperty physicalProps = topProp.Find("Parameters", false);
		const FbxProperty pbrProps = topProp.Find("main", false);
		if (physicalProps.IsValid()) {
			OSG_INFO << "3dsMax Physical Material." << std::endl;
			pOsgMat->setName(pFbxMat->GetName());

			osg::ref_ptr<GltfPbrMRMaterial> mat = dynamic_cast<GltfPbrMRMaterial*>(pOsgMat.get());

			if (!shadingModel.IsEmpty() && shadingModel != "unknown") {
				OSG_INFO <<
					"Warning: Material %s has surprising shading model: %s\n" <<
					pFbxMat->GetName() <<
					shadingModel << std::endl;
			}
			FbxDouble4 baseColor = getValue(physicalProps, "base_color", FbxDouble4(1.0, 1.0, 1.0, 1.0));
			double roughness = getValue(physicalProps, "roughness", 1.0);
			double metalness = getValue(physicalProps, "metalness", 1.0);
			bool invertRoughness = getValue(physicalProps, "inv_roughness", false);
			if (invertRoughness) {
				roughness = 1.0f - roughness;
			}
			FbxDouble4 emissiveColor = getValue(physicalProps, "emit_color", FbxDouble4(0.0, 0.0, 0.0, 1.0));
			mat->metallicFactor = metalness;
			mat->roughnessFactor = roughness;

			auto getTex = [&](std::string propName) -> const osg::ref_ptr<osg::Texture2D> {
				osg::ref_ptr<osg::Texture2D> ptr = nullptr;
				const FbxProperty texProp = physicalProps.Find((propName + "_map").c_str(), false);
				if (texProp.IsValid()) {
					const FbxProperty useProp = physicalProps.Find((propName + "_map_on").c_str(), false);
					if (useProp.IsValid() && !useProp.Get<FbxBool>()) {
						// skip this texture if the _on property exists *and* is explicitly false
						return ptr;
					}
					FbxFileTexture* fileTexture = selectFbxFileTexture(texProp);
					if (fileTexture)
					{
						ptr = fbxTextureToOsgTexture(fileTexture);
					}

				}
				return ptr;
				};

			auto getFbxFileTextureTex = [&](std::string propName) -> const FbxFileTexture* {
				FbxFileTexture* ptr = nullptr;
				const FbxProperty texProp = physicalProps.Find((propName + "_map").c_str(), false);
				if (texProp.IsValid()) {
					const FbxProperty useProp = physicalProps.Find((propName + "_map_on").c_str(), false);
					if (useProp.IsValid() && !useProp.Get<FbxBool>()) {
						// skip this texture if the _on property exists *and* is explicitly false
						return ptr;
					}
					FbxFileTexture* fileTexture = selectFbxFileTexture(texProp);
					if (fileTexture)
					{
						ptr = fileTexture;
					}

				}
				return ptr;
				};

			const FbxFileTexture* baseColorFileTexture = getFbxFileTextureTex("base_color");
			if (baseColorFileTexture) {
				mat->baseColorTexture = fbxTextureToOsgTexture(baseColorFileTexture);
				assignTextureDetails(result.diffuse, mat->baseColorTexture, baseColorFileTexture);
			}
			mat->baseColorFactor = { baseColor[0],baseColor[1],baseColor[2],baseColor[3] };

			osg::ref_ptr<osg::Texture2D> roughnessMap = getTex("roughness");
			osg::ref_ptr<osg::Texture2D> metalnessMap = getTex("metalness");
			if (roughnessMap && metalnessMap) {

				osg::ref_ptr<osg::Image> roughnessImageData = roughnessMap->getImage();
				osg::ref_ptr<osg::Image> metalnessImageData = metalnessMap->getImage();
				osg::ref_ptr<osg::Image> metallicRoughnessImage = mat->mergeImages(metalnessImageData, roughnessImageData);
				osg::ref_ptr<osg::Texture2D> metallicRoughnessTexture = new osg::Texture2D;

				metallicRoughnessTexture->setImage(metallicRoughnessImage);

				mat->metallicRoughnessTexture = metallicRoughnessTexture;
			}
			else if (roughnessMap && !metalnessMap) {
				mat->metallicRoughnessTexture = roughnessMap;
			}
			else if (!roughnessMap && metalnessMap) {
				mat->metallicRoughnessTexture = metalnessMap;
			}

			mat->emissiveFactor = { emissiveColor[0],emissiveColor[1],emissiveColor[2] };
			mat->emissiveTexture = getTex("emit_color");
			mat->normalTexture = result.normalMap ? result.normalMap->texture : NULL;
			mat->occlusionTexture = result.ambient ? result.ambient->texture : NULL;

			KHR_materials_emissive_strength* emissive_strength_extension = new KHR_materials_emissive_strength;
			emissive_strength_extension->setEmissiveStrength(getValue(physicalProps, "emission", 0.0));
			mat->materialExtensions.push_back(emissive_strength_extension);

			KHR_materials_anisotropy* anisotropy_extension = new KHR_materials_anisotropy;
			anisotropy_extension->setAnisotropyStrength(getValue(physicalProps, "anisotropy", 0.0));
			anisotropy_extension->osgAnisotropyTexture = getTex("anisotropy");
			mat->materialExtensions.push_back(anisotropy_extension);

			KHR_materials_clearcoat* clearcoat_extension = new KHR_materials_clearcoat;
			clearcoat_extension->setClearcoatFactor(getValue(physicalProps, "coating", 0.0));
			clearcoat_extension->osgClearcoatNormalTexture = getTex("coat");
			clearcoat_extension->setClearcoatRoughnessFactor(getValue(physicalProps, "coat_roughness", 0.0));
			bool invertCoatRoughness = getValue(physicalProps, "coat_roughness_inv", false);
			if (invertCoatRoughness) {
				clearcoat_extension->setClearcoatRoughnessFactor(1 - getValue(physicalProps, "coat_roughness", 0.0));
			}
			clearcoat_extension->osgClearcoatRoughnessTexture = getTex("coat_rough");
			mat->materialExtensions.push_back(clearcoat_extension);

			KHR_materials_ior* ior_extension = new KHR_materials_ior;
			ior_extension->setIor(getValue(physicalProps, "trans_ior", 1.5));
			mat->materialExtensions.push_back(ior_extension);

			FbxProperty specularProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sSpecular);
			FbxDouble3 specularColorFactor = specularProperty.Get<FbxDouble3>();
			KHR_materials_specular* specular_extension = new KHR_materials_specular;
			specular_extension->setSpecularColorFactor({ specularColorFactor[0],specularColorFactor[1],specularColorFactor[2] });
			specular_extension->osgSpecularTexture = result.specular ? result.specular->texture : NULL;
			mat->materialExtensions.push_back(specular_extension);
		}
		else if (pbrProps.IsValid()) {
			const FbxProperty metalnessProps = pbrProps.Find("metalness");
			if (metalnessProps.IsValid()) {
				OSG_INFO << "3dsMax Metalness/Roughness Material." << std::endl;
				pOsgMat->setName(pFbxMat->GetName());
				osg::ref_ptr<GltfPbrMRMaterial> mat = dynamic_cast<GltfPbrMRMaterial*>(pOsgMat.get());
				auto getTex = [&](std::string propName) -> const FbxFileTexture* {
					FbxFileTexture* ptr = nullptr;
					const FbxProperty texProp = pbrProps.Find((propName + "_map").c_str(), false);
					if (texProp.IsValid()) {
						FbxFileTexture* fileTexture = selectFbxFileTexture(texProp);
						if (fileTexture)
						{
							//ptr = fbxTextureToOsgTexture(fileTexture);
							ptr = fileTexture;
						}

					}
					return ptr;
					};
				const FbxFileTexture* normalFileTexture = getTex("norm");
				if (normalFileTexture) {
					mat->normalTexture = fbxTextureToOsgTexture(normalFileTexture);
					assignTextureDetails(result.normalMap, mat->normalTexture, normalFileTexture);
				}

				const FbxFileTexture* aoFileTexture = getTex("ao");
				if (aoFileTexture) {
					mat->occlusionTexture = fbxTextureToOsgTexture(aoFileTexture);
					assignTextureDetails(result.ambient, mat->occlusionTexture, aoFileTexture);
				}

				const FbxFileTexture* emissiveFileTexture = getTex("emit_color");
				if (emissiveFileTexture) {
					mat->emissiveTexture = fbxTextureToOsgTexture(emissiveFileTexture);
					assignTextureDetails(result.emissive, mat->emissiveTexture, emissiveFileTexture);
				}
				FbxDouble4 emissiveColor = getValue(pbrProps, "emit_color", FbxDouble4(0.0, 0.0, 0.0, 1.0));
				mat->emissiveFactor = { emissiveColor[0],emissiveColor[1],emissiveColor[2] };

				const FbxFileTexture* baseColorFileTexture = getTex("base_color");
				if (baseColorFileTexture) {
					mat->baseColorTexture = fbxTextureToOsgTexture(baseColorFileTexture);
					assignTextureDetails(result.diffuse, mat->baseColorTexture, baseColorFileTexture);
				}
				FbxDouble4 baseColor = getValue(pbrProps, "basecolor", FbxDouble4(1.0, 1.0, 1.0, 1.0));
				mat->baseColorFactor = { baseColor[0],baseColor[1],baseColor[2],baseColor[3] };

				mat->metallicFactor = getValue(pbrProps, "metalness", 1.0);
				mat->roughnessFactor = getValue(pbrProps, "roughness", 1.0);

				const FbxFileTexture* roughnessFileTexture = getTex("roughness");
				const FbxFileTexture* metalnessFileTexture = getTex("metalness");
				osg::ref_ptr<osg::Texture2D> roughnessMap = roughnessFileTexture ? fbxTextureToOsgTexture(roughnessFileTexture) : NULL;
				osg::ref_ptr<osg::Texture2D> metalnessMap = metalnessFileTexture ? fbxTextureToOsgTexture(metalnessFileTexture) : NULL;
				if (roughnessMap && metalnessMap) {

					osg::ref_ptr<osg::Image> roughnessImageData = roughnessMap->getImage();
					osg::ref_ptr<osg::Image> metalnessImageData = metalnessMap->getImage();
					osg::ref_ptr<osg::Image> metallicRoughnessImage = mat->mergeImages(metalnessImageData, roughnessImageData);
					osg::ref_ptr<osg::Texture2D> metallicRoughnessTexture = new osg::Texture2D;
					metallicRoughnessTexture->setImage(metallicRoughnessImage);

					mat->metallicRoughnessTexture = metallicRoughnessTexture;
				}
				else if (roughnessMap && !metalnessMap) {
					mat->metallicRoughnessTexture = roughnessMap;
				}
				else if (!roughnessMap && metalnessMap) {
					mat->metallicRoughnessTexture = metalnessMap;
				}



			}
			else {
				OSG_INFO << "3dsMax Specular/Glossiness Material." << std::endl;
				pOsgMat = new GltfPbrSGMaterial;
				pOsgMat->setName(pFbxMat->GetName());
				result.material = pOsgMat;
				osg::ref_ptr<GltfPbrSGMaterial> mat = dynamic_cast<GltfPbrSGMaterial*>(pOsgMat.get());

				auto getTex = [&](std::string propName) -> const FbxFileTexture* {
					FbxFileTexture* ptr = nullptr;
					const FbxProperty texProp = pbrProps.Find((propName + "_map").c_str(), false);
					if (texProp.IsValid()) {
						FbxFileTexture* fileTexture = selectFbxFileTexture(texProp);
						if (fileTexture)
						{
							//ptr = fbxTextureToOsgTexture(fileTexture);
							ptr = fileTexture;
						}

					}
					return ptr;
					};
				const FbxFileTexture* normalFileTexture = getTex("norm");
				if (normalFileTexture) {
					mat->normalTexture = fbxTextureToOsgTexture(normalFileTexture);
					assignTextureDetails(result.normalMap, mat->normalTexture, normalFileTexture);
				}

				const FbxFileTexture* aoFileTexture = getTex("ao");
				if (aoFileTexture) {
					mat->occlusionTexture = fbxTextureToOsgTexture(aoFileTexture);
					assignTextureDetails(result.ambient, mat->occlusionTexture, aoFileTexture);
				}

				const FbxFileTexture* emissiveFileTexture = getTex("emit_color");
				if (emissiveFileTexture) {
					mat->emissiveTexture = fbxTextureToOsgTexture(emissiveFileTexture);
					assignTextureDetails(result.emissive, mat->emissiveTexture, emissiveFileTexture);
				}
				FbxDouble4 emissiveColor = getValue(pbrProps, "emit_color", FbxDouble4(0.0, 0.0, 0.0, 1.0));
				mat->emissiveFactor = { emissiveColor[0],emissiveColor[1],emissiveColor[2] };

				KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = new KHR_materials_pbrSpecularGlossiness;
				FbxDouble4 Specular = getValue(pbrProps, "Specular", FbxDouble4(1.0, 1.0, 1.0, 1.0));
				pbrSpecularGlossiness_extension->setSpecularFactor({ Specular[0],Specular[1],Specular[2] });
				pbrSpecularGlossiness_extension->setGlossinessFactor(getValue(pbrProps, "glossiness", 0.0));

				const FbxFileTexture* baseColorFileTexture = getTex("base_color");
				if (baseColorFileTexture) {
					osg::ref_ptr<osg::Texture2D> baseColorMap = fbxTextureToOsgTexture(baseColorFileTexture);
					pbrSpecularGlossiness_extension->osgDiffuseTexture = baseColorMap;
					assignTextureDetails(result.diffuse, baseColorMap, baseColorFileTexture);
				}
				FbxDouble4 baseColor = getValue(pbrProps, "basecolor", FbxDouble4(1.0, 1.0, 1.0, 1.0));
				pbrSpecularGlossiness_extension->setDiffuseFactor({ baseColor[0],baseColor[1],baseColor[2],baseColor[3] });

				const FbxFileTexture* glossinessFileTexture = getTex("glossiness");
				const FbxFileTexture* specularFileTexture = getTex("specular");
				osg::ref_ptr<osg::Texture2D> specularMap = specularFileTexture ? fbxTextureToOsgTexture(specularFileTexture) : NULL;
				osg::ref_ptr<osg::Texture2D> glossinessMap = glossinessFileTexture ? fbxTextureToOsgTexture(glossinessFileTexture) : NULL;
				if (specularMap && glossinessMap) {
					osg::ref_ptr<osg::Image> specularImageData = specularMap->getImage();
					osg::ref_ptr<osg::Image> glossinessImageData = glossinessMap->getImage();
					osg::ref_ptr<osg::Image> specularGlossinessImage = mat->mergeImages(specularImageData, glossinessImageData);
					osg::ref_ptr<osg::Texture2D> specularGlossinessTexture = new osg::Texture2D;
					specularGlossinessTexture->setImage(specularGlossinessImage);

					pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture = specularGlossinessTexture;
					osg::ref_ptr<TextureDetails> temp = new TextureDetails;
					result.specular = result.specular ? result.specular : temp;
					result.specular->texture = specularMap;
					result.specular->channel = specularFileTexture->UVSet.Get();
					result.specular->scale.set(specularFileTexture->GetScaleU(), specularFileTexture->GetScaleV());
					result.shininess = result.shininess ? result.shininess : temp;
					result.shininess->texture = glossinessMap;
					result.shininess->channel = glossinessFileTexture->UVSet.Get();
					result.shininess->scale.set(glossinessFileTexture->GetScaleU(), glossinessFileTexture->GetScaleV());

				}
				else if (specularMap && !glossinessMap) {
					pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture = specularMap;
					osg::ref_ptr<TextureDetails> temp = new TextureDetails;
					result.specular = result.specular ? result.specular : temp;
					result.specular->texture = pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture;
					result.specular->channel = specularFileTexture->UVSet.Get();
					result.specular->scale.set(specularFileTexture->GetScaleU(), specularFileTexture->GetScaleV());
				}
				else if (!specularMap && glossinessMap) {
					pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture = glossinessMap;
					osg::ref_ptr<TextureDetails> temp = new TextureDetails;
					result.shininess = result.shininess ? result.shininess : temp;
					result.shininess->texture = pbrSpecularGlossiness_extension->osgSpecularGlossinessTexture;
					result.shininess->channel = glossinessFileTexture->UVSet.Get();
					result.shininess->scale.set(glossinessFileTexture->GetScaleU(), glossinessFileTexture->GetScaleV());
				}
				//添加到materialExtensionsByCesiumSupport就不要添加到materialExtensions
				mat->materialExtensionsByCesiumSupport.push_back(pbrSpecularGlossiness_extension);
			}
		}

	}
	else {
		if (pFbxLambert)
		{
			FbxDouble3 color = pFbxLambert->Diffuse.Get();
			double factor = pFbxLambert->DiffuseFactor.Get();
			double transparencyFactor = useTransparencyColorFactor ? transparencyColorFactor : pFbxLambert->TransparencyFactor.Get();
			const osg::Vec4 baseColor = osg::Vec4(
				static_cast<float>(color[0] * factor),
				static_cast<float>(color[1] * factor),
				static_cast<float>(color[2] * factor),
				static_cast<float>(1.0 - transparencyFactor));
			pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, baseColor);

			color = pFbxLambert->Ambient.Get();
			factor = pFbxLambert->AmbientFactor.Get();
			pOsgMat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(
				static_cast<float>(color[0] * factor),
				static_cast<float>(color[1] * factor),
				static_cast<float>(color[2] * factor),
				1.0f));

			color = pFbxLambert->Emissive.Get();
			factor = pFbxLambert->EmissiveFactor.Get();
			pOsgMat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(
				static_cast<float>(color[0] * factor),
				static_cast<float>(color[1] * factor),
				static_cast<float>(color[2] * factor),
				1.0f));

			// get maps factors...
			if (result.diffuse.valid()) result.diffuse->factor = pFbxLambert->DiffuseFactor.Get();
			if (result.emissive.valid()) result.emissive->factor = pFbxLambert->EmissiveFactor.Get();
			if (result.ambient.valid()) result.ambient->factor = pFbxLambert->AmbientFactor.Get();
			if (result.normalMap.valid()) result.normalMap->factor = pFbxLambert->BumpFactor.Get();

			if (const FbxSurfacePhong* pFbxPhong = FbxCast<FbxSurfacePhong>(pFbxLambert))
			{
				color = pFbxPhong->Specular.Get();
				factor = pFbxPhong->SpecularFactor.Get();
				pOsgMat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(
					static_cast<float>(color[0] * factor),
					static_cast<float>(color[1] * factor),
					static_cast<float>(color[2] * factor),
					1.0f));
				// Since Maya and 3D studio Max stores their glossiness values in exponential format (2^(log2(x))
				// We need to linearize to values between 0-100 and then scale to values between 0-128.
				// Glossiness values above 100 will result in shininess larger than 128.0 and will be clamped
				double shininess = (64.0 * log(pFbxPhong->Shininess.Get())) / (5.0 * log(2.0));
				pOsgMat->setShininess(osg::Material::FRONT_AND_BACK,
					static_cast<float>(shininess));

				// get maps factors...
				if (result.reflection.valid()) result.reflection->factor = pFbxPhong->ReflectionFactor.Get();
				if (result.specular.valid()) result.specular->factor = pFbxPhong->SpecularFactor.Get();
				// get more factors here...
			}

			OSG_INFO << "3dsMax Traditional Material." << std::endl;
			osg::ref_ptr<GltfPbrMRMaterial> mat = dynamic_cast<GltfPbrMRMaterial*>(pOsgMat.get());
			mat->baseColorFactor = { baseColor.x(),baseColor.y(),baseColor.z(),baseColor.w() };
			FbxFileTexture* diffuseTexture = selectFbxFileTexture(pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse));
			if (diffuseTexture)
			{
				mat->baseColorTexture = fbxTextureToOsgTexture(diffuseTexture);
			}
			KHR_materials_emissive_strength* emissive_strength_extension = new KHR_materials_emissive_strength;
			emissive_strength_extension->setEmissiveStrength(1.0);
			mat->materialExtensions.push_back(emissive_strength_extension);

			FbxDouble3 emissiveColor = pFbxLambert->Emissive.Get();
			mat->emissiveFactor = { emissiveColor[0],emissiveColor[1],emissiveColor[2] };
			if (shadingModel.Lower() == "blinn" || shadingModel.Lower() == "phong") {
				pOsgMat->setName(pFbxMat->GetName());

				auto getRoughness = [&](float shininess) { return sqrtf(2.0f / (2.0f + shininess)); };
				mat->metallicFactor = 0.4;
				const FbxProperty shininessProp = pFbxMat->FindProperty("Shininess");
				if (shininessProp.IsValid()) {
					FbxDouble roughness = shininessProp.Get<FbxDouble>();
					mat->roughnessFactor = getRoughness(roughness);
				}
				else {
					const FbxProperty shininessExponentProp = pFbxMat->FindProperty("ShininessExponent");
					if (shininessExponentProp.IsValid()) {
						FbxDouble roughness = shininessExponentProp.Get<FbxDouble>();
						mat->roughnessFactor = getRoughness(roughness);
					}
				}
			}
		}
		else
		{
			FbxProperty specularProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sSpecular);
			FbxProperty shininessProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sShininess);
			if (specularProperty.IsValid() || shininessProperty.IsValid())
			{
				pOsgMat = new GltfPbrSGMaterial;
				pOsgMat->setName(pFbxMat->GetName());
				result.material = pOsgMat;
				osg::ref_ptr<GltfPbrSGMaterial> mat = dynamic_cast<GltfPbrSGMaterial*>(pOsgMat.get());
				KHR_materials_pbrSpecularGlossiness* pbrSpecularGlossiness_extension = new KHR_materials_pbrSpecularGlossiness;
				if (specularProperty.IsValid()) {
					FbxDouble3 specularColor = specularProperty.Get<FbxDouble3>();
					FbxProperty specularFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sSpecularFactor);
					FbxDouble specularFactor = 1.0;
					if (specularFactorProperty.IsValid())
					{
						specularFactor = specularFactorProperty.Get<FbxDouble>();
					}
					osg::Vec4 color(
						static_cast<float>(specularColor[0] * specularFactor),
						static_cast<float>(specularColor[1] * specularFactor),
						static_cast<float>(specularColor[2] * specularFactor),
						1.0f);
					pOsgMat->setSpecular(osg::Material::FRONT_AND_BACK, color);
					pbrSpecularGlossiness_extension->setSpecularFactor({ color.x(), color.y(), color.z() });
				}

				if (shininessProperty.IsValid()) {
					double shininess = shininessProperty.Get<FbxDouble>();
					shininess = (64.0 * log(shininess)) / (5.0 * log(2.0));
					pOsgMat->setShininess(osg::Material::FRONT_AND_BACK, shininess);
					pbrSpecularGlossiness_extension->setGlossinessFactor(osg::clampBetween(shininess / 128.0, 0.0, 1.0));
				}
				FbxProperty diffuseProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse);
				if (diffuseProperty.IsValid()) {
					FbxFileTexture* diffuseTexture = selectFbxFileTexture(diffuseProperty);
					if (diffuseTexture)
					{
						pbrSpecularGlossiness_extension->osgDiffuseTexture = fbxTextureToOsgTexture(diffuseTexture);
					}

					FbxDouble3 diffuseColor = diffuseProperty.Get<FbxDouble3>();
					FbxProperty diffuseFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
					FbxDouble diffuseFactor = 1.0;
					if (diffuseFactorProperty.IsValid())
					{
						diffuseFactor = diffuseFactorProperty.Get<FbxDouble>();
					}
					osg::Vec4 baseColor(
						static_cast<float>(diffuseColor[0] * diffuseFactor),
						static_cast<float>(diffuseColor[1] * diffuseFactor),
						static_cast<float>(diffuseColor[2] * diffuseFactor),
						1.0f);
					pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, baseColor);
					pbrSpecularGlossiness_extension->setDiffuseFactor({ baseColor.x(),baseColor.y(),baseColor.z(),baseColor.w() });
				}

				mat->materialExtensionsByCesiumSupport.push_back(pbrSpecularGlossiness_extension);

				FbxProperty ambientProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sAmbient);
				if (ambientProperty.IsValid()) {
					FbxDouble3 ambientColor = ambientProperty.Get<FbxDouble3>();
					FbxProperty ambientFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sAmbientFactor);
					FbxDouble ambientFactor = 1.0;
					if (ambientFactorProperty.IsValid())
					{
						ambientFactor = ambientFactorProperty.Get<FbxDouble>();
					}
					osg::Vec4 color(
						static_cast<float>(ambientColor[0] * ambientFactor),
						static_cast<float>(ambientColor[1] * ambientFactor),
						static_cast<float>(ambientColor[2] * ambientFactor),
						1.0f);
					pOsgMat->setAmbient(osg::Material::FRONT_AND_BACK, color);
				}

				FbxProperty emissionProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sEmissive);
				if (emissionProperty.IsValid()) {
					FbxDouble3 emissionColor = emissionProperty.Get<FbxDouble3>();
					FbxProperty emissionFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sEmissiveFactor);
					FbxDouble emissionFactor = 1.0;
					if (emissionFactorProperty.IsValid())
					{
						emissionFactor = emissionFactorProperty.Get<FbxDouble>();
					}
					osg::Vec4 color(
						static_cast<float>(emissionColor[0] * emissionFactor),
						static_cast<float>(emissionColor[1] * emissionFactor),
						static_cast<float>(emissionColor[2] * emissionFactor),
						1.0f);
					pOsgMat->setEmission(osg::Material::FRONT_AND_BACK, color);
					mat->emissiveFactor = { color.x(),color.y(),color.z() };
				}
			}
			else {

				osg::ref_ptr<GltfPbrMRMaterial> mat = dynamic_cast<GltfPbrMRMaterial*>(pOsgMat.get());
				FbxProperty diffuseProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse);
				if (diffuseProperty.IsValid()) {
					FbxDouble3 diffuseColor = diffuseProperty.Get<FbxDouble3>();
					FbxProperty diffuseFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
					FbxDouble diffuseFactor = 1.0;
					if (diffuseFactorProperty.IsValid())
					{
						diffuseFactor = diffuseFactorProperty.Get<FbxDouble>();
					}
					osg::Vec4 baseColor(
						static_cast<float>(diffuseColor[0] * diffuseFactor),
						static_cast<float>(diffuseColor[1] * diffuseFactor),
						static_cast<float>(diffuseColor[2] * diffuseFactor),
						1.0f);
					pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, baseColor);
					mat->baseColorFactor = { baseColor.x(),baseColor.y(),baseColor.z(),baseColor.w() };
				}

				FbxProperty ambientProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sAmbient);
				if (ambientProperty.IsValid()) {
					FbxDouble3 ambientColor = ambientProperty.Get<FbxDouble3>();
					FbxProperty ambientFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sAmbientFactor);
					FbxDouble ambientFactor = 1.0;
					if (ambientFactorProperty.IsValid())
					{
						ambientFactor = ambientFactorProperty.Get<FbxDouble>();
					}
					osg::Vec4 color(
						static_cast<float>(ambientColor[0] * ambientFactor),
						static_cast<float>(ambientColor[1] * ambientFactor),
						static_cast<float>(ambientColor[2] * ambientFactor),
						1.0f);
					pOsgMat->setAmbient(osg::Material::FRONT_AND_BACK, color);
				}

				FbxProperty emissionProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sEmissive);
				if (emissionProperty.IsValid()) {
					FbxDouble3 emissionColor = emissionProperty.Get<FbxDouble3>();
					FbxProperty emissionFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sEmissiveFactor);
					FbxDouble emissionFactor = 1.0;
					if (emissionFactorProperty.IsValid())
					{
						emissionFactor = emissionFactorProperty.Get<FbxDouble>();
					}
					osg::Vec4 color(
						static_cast<float>(emissionColor[0] * emissionFactor),
						static_cast<float>(emissionColor[1] * emissionFactor),
						static_cast<float>(emissionColor[2] * emissionFactor),
						1.0f);
					pOsgMat->setEmission(osg::Material::FRONT_AND_BACK, color);
					mat->emissiveFactor = { color.x(),color.y(),color.z() };
				}
			}


		}
	}
	applyLightmapEmissionOverride(pOsgMat.get(), result.emissive, _lightmapTextures);




	_fbxMaterialMap.insert(FbxMaterialMap::value_type(pFbxMat, result));
	return result;
}

osg::ref_ptr<osg::Texture2D> FbxMaterialToOsgStateSet::fbxTextureToOsgTexture(const FbxFileTexture* fbx)
{
	// Try to find image in cache
	ImageMap::iterator it = _imageMap.find(fbx->GetFileName());
	if (it != _imageMap.end())
		return it->second;


	// Try to locate valid filename
	std::string filename = "";

#ifdef OSG_USE_UTF8_FILENAME
	const std::string& fbxFilename(osgDB::convertStringFromCurrentCodePageToUTF8(fbx->GetFileName()));
#else
	std::string fbxFilename(osgDB::convertStringFromUTF8toCurrentCodePage(fbx->GetFileName()));
#endif

	// Warning: fbx->GetRelativeFileName() is relative TO EXECUTION DIR
	//          fbx->GetFileName() is as stored initially in the FBX
	if (osgDB::fileExists(osgDB::concatPaths(_dir, fbxFilename))) // First try "export dir/name"
	{
		filename = osgDB::concatPaths(_dir, fbxFilename);
	}
	else if (osgDB::fileExists(fbxFilename)) // Then try "name" (if absolute)
	{
		filename = fbxFilename;
	}
	else if (osgDB::fileExists(osgDB::concatPaths(_dir, fbx->GetRelativeFileName()))) // Else try  "current dir + relative filename"
	{
		filename = osgDB::concatPaths(_dir, fbx->GetRelativeFileName());
	}
	else if (osgDB::fileExists(osgDB::concatPaths(_dir, osgDB::getSimpleFileName(fbxFilename)))) // Else try "current dir + simple filename"
	{
		filename = osgDB::concatPaths(_dir, osgDB::getSimpleFileName(fbxFilename));
	}
	else
	{
		OSG_WARN << "Could not find valid file for " << fbxFilename << std::endl;
		return NULL;
	}
	if (osgDB::FileType::REGULAR_FILE == osgDB::fileType(filename))
	{
		osg::ref_ptr<osg::Image> pImage = osgDB::readRefImageFile(filename, _options);
		if (pImage.valid())
		{
			osg::ref_ptr<osg::Texture2D> pOsgTex = new osg::Texture2D;
			pOsgTex->setImage(pImage.get());
			pOsgTex->setWrap(osg::Texture2D::WRAP_S, convertWrap(fbx->GetWrapModeU()));
			pOsgTex->setWrap(osg::Texture2D::WRAP_T, convertWrap(fbx->GetWrapModeV()));
			_imageMap.insert(std::make_pair(fbx->GetFileName(), pOsgTex.get()));
			return pOsgTex;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}

FbxFileTexture* FbxMaterialToOsgStateSet::selectFbxFileTexture(const FbxProperty& lProperty)
{
	if (lProperty.IsValid())
	{
		// check if layered textures are used...
		int layeredTextureCount = lProperty.GetSrcObjectCount<FbxLayeredTexture>();
		if (layeredTextureCount)
		{
			// find the first valud FileTexture
			for (int layeredTextureIndex = 0; layeredTextureIndex < layeredTextureCount; ++layeredTextureIndex)
			{
				FbxLayeredTexture* layered_texture = FbxCast<FbxLayeredTexture>(lProperty.GetSrcObject<FbxLayeredTexture>(layeredTextureIndex));
				int lNbTex = layered_texture->GetSrcObjectCount<FbxFileTexture>();
				for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
				{
					FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(layered_texture->GetSrcObject<FbxFileTexture>(lTextureIndex));
					if (lTexture) return lTexture;
				}
			}
		}
		else
		{
			// find the first valud FileTexture
			int lNbTex = lProperty.GetSrcObjectCount<FbxFileTexture>();
			for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
			{
				FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
				if (lTexture) return lTexture;
			}
		}
	}
	return 0;
}

TextureDetails* FbxMaterialToOsgStateSet::selectTextureDetails(const FbxProperty& lProperty)
{
	if (lProperty.IsValid())
	{
		FbxFileTexture* fileTexture = selectFbxFileTexture(lProperty);
		if (fileTexture)
		{
			TextureDetails* textureDetails = new TextureDetails();
			textureDetails->texture = fbxTextureToOsgTexture(fileTexture);
			textureDetails->channel = fileTexture->UVSet.Get();
			textureDetails->scale.set(fileTexture->GetScaleU(), fileTexture->GetScaleV());
			return textureDetails;
		}
	}
	return 0;
}

void FbxMaterialToOsgStateSet::checkInvertTransparency()
{
	int zeroAlpha = 0, oneAlpha = 0;
	for (FbxMaterialMap::const_iterator it = _fbxMaterialMap.begin(); it != _fbxMaterialMap.end(); ++it)
	{
		const osg::Material* pMaterial = it->second.material.get();
		float alpha = pMaterial->getDiffuse(osg::Material::FRONT).a();
		if (alpha > 0.999f)
		{
			++oneAlpha;
		}
		else if (alpha < 0.001f)
		{
			++zeroAlpha;
		}
	}

	if (zeroAlpha > oneAlpha)
	{
		//Transparency values seem to be back to front so invert them.

		for (FbxMaterialMap::const_iterator it = _fbxMaterialMap.begin(); it != _fbxMaterialMap.end(); ++it)
		{
			osg::Material* pMaterial = it->second.material.get();
			osg::Vec4 diffuse = pMaterial->getDiffuse(osg::Material::FRONT);
			diffuse.a() = 1.0f - diffuse.a();
			pMaterial->setDiffuse(osg::Material::FRONT_AND_BACK, diffuse);
		}
	}
}
