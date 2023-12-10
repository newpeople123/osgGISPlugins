#include <osgdb_fbx/fbxMaterialToOsgStateSet.h>
#include <utils/GltfPbrMetallicRoughnessMaterial.h>
#include <utils/GltfPbrSpecularGlossinessMaterial.h>
#include <sstream>
#include <osg/TexMat>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osgDB/ConvertUTF>
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

	osg::ref_ptr<osg::Material> pOsgMat = new GltfPbrMetallicRoughnessMaterial;
	result.material = pOsgMat;
	const FbxProperty topProp = pFbxMat->FindProperty("3dsMax", false);
	if (topProp.GetPropertyDataType() == FbxCompoundDT) {
		const FbxProperty physicalProps = topProp.Find("Parameters", false);
		if (physicalProps.IsValid()) {
			OSG_INFO << ("3dsMax Physical Material.") << std::endl;
			pOsgMat->setName(pFbxMat->GetName());

			osg::ref_ptr<GltfPbrMetallicRoughnessMaterial> mat = dynamic_cast<GltfPbrMetallicRoughnessMaterial*>(pOsgMat.get());

			if (!shadingModel.IsEmpty() && shadingModel != "unknown") {
				std::cout <<
					"Warning: Material %s has surprising shading model: %s\n" <<
					pFbxMat->GetName() <<
					shadingModel << std::endl;
			}
			FbxDouble4 baseCol = getValue(physicalProps, "base_color", FbxDouble4(0.5, 0.5, 0.5, 1.0));
			double roughness = getValue(physicalProps, "roughness", 0.0);
			double metalness = getValue(physicalProps, "metalness", 0.0);
			bool invertRoughness = getValue(physicalProps, "inv_roughness", false);
			if (invertRoughness) {
				roughness = 1.0f - roughness;
			}
			FbxDouble4 emissiveColor = getValue(physicalProps, "emit_color", FbxDouble4(1, 1, 1, 1));

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
				result.diffuse = result.diffuse ? result.diffuse : new TextureDetails;
				result.diffuse->texture = mat->baseColorTexture;
				result.diffuse->channel = baseColorFileTexture->UVSet.Get();
				result.diffuse->scale.set(baseColorFileTexture->GetScaleU(), baseColorFileTexture->GetScaleV());
			}
			mat->baseColorFactor = { baseCol[0],baseCol[1],baseCol[2],baseCol[3] };

			osg::ref_ptr<osg::Texture2D> roughnessMap = getTex("roughness");
			osg::ref_ptr<osg::Texture2D> metalnessMap = getTex("metalness");
			if (roughnessMap && metalnessMap) {

				osg::ref_ptr<osg::Image> roughnessImageData = roughnessMap->getImage();
				osg::ref_ptr<osg::Image> metalnessImageData = metalnessMap->getImage();
				osg::ref_ptr<osg::Image> metallicRoughnessImage = mat->mergeImagesToMetallicRoughnessImage(metalnessImageData, roughnessImageData);
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
			mat->emissiveStrength = getValue(physicalProps, "emission", 0.0);
			mat->emissiveTexture = getTex("emit_color");
			mat->enable_KHR_materials_emissive_strength = false;//cesium does not support

			mat->normalTexture = result.normalMap ? result.normalMap->texture : NULL;
			mat->occlusionTexture = result.ambient ? result.ambient->texture : NULL;

			mat->anisotropyStrength = getValue(physicalProps, "anisotropy", 1.0);
			mat->anisotropyTexture = getTex("anisotropy");
			mat->enable_KHR_materials_anisotropy = false;//cesium does not support

			mat->clearcoatFactor = getValue(physicalProps, "coating", 0.0);
			mat->clearcoatTexture = getTex("coat");
			mat->clearcoatRoughnessFactor = getValue(physicalProps, "coat_roughness", 0.0);
			bool invertCoatRoughness = getValue(physicalProps, "coat_roughness_inv", false);
			if (invertCoatRoughness) {
				mat->clearcoatRoughnessFactor = 1.0f - mat->clearcoatRoughnessFactor;
			}
			mat->clearcoatRoughnessTexture = getTex("coat_rough");
			mat->enable_KHR_materials_clearcoat = false;//cesium does not support

			mat->ior = getValue(physicalProps, "trans_ior", 1.5);
			mat->enable_KHR_materials_ior = false;//cesium does not support

			FbxProperty specularProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sSpecular);
			FbxDouble3 specularColorFactor = specularProperty.Get<FbxDouble3>();
			mat->specularColorFactor = { specularColorFactor[0],specularColorFactor[1],specularColorFactor[2] };
			mat->specularTexture = result.ambient ? result.specular->texture : NULL;
			mat->enable_KHR_materials_specular = false;//cesium does not support
		}
		const FbxProperty pbrProps = topProp.Find("main", false);
		if (pbrProps.IsValid()) {
			const FbxProperty metalnessProps = pbrProps.Find("metalness");
			if (metalnessProps.IsValid()) {
				OSG_INFO << ("3dsMax Metalness/Roughness Material.") << std::endl;
				pOsgMat->setName(pFbxMat->GetName());
				osg::ref_ptr<GltfPbrMetallicRoughnessMaterial> mat = dynamic_cast<GltfPbrMetallicRoughnessMaterial*>(pOsgMat.get());

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
					result.normalMap = result.normalMap ? result.normalMap : new TextureDetails;
					result.normalMap->texture = mat->normalTexture;
					result.normalMap->channel = normalFileTexture->UVSet.Get();
					result.normalMap->scale.set(normalFileTexture->GetScaleU(), normalFileTexture->GetScaleV());
				}

				const FbxFileTexture* aoFileTexture = getTex("ao");
				if (aoFileTexture) {
					mat->occlusionTexture = fbxTextureToOsgTexture(aoFileTexture);
					result.ambient = result.ambient ? result.ambient : new TextureDetails;
					result.ambient->texture = mat->occlusionTexture;
					result.ambient->channel = aoFileTexture->UVSet.Get();
					result.ambient->scale.set(aoFileTexture->GetScaleU(), aoFileTexture->GetScaleV());
				}

				const FbxFileTexture* emissiveFileTexture = getTex("emit_color");
				if (emissiveFileTexture) {
					mat->emissiveTexture = fbxTextureToOsgTexture(emissiveFileTexture);
					result.emissive = result.emissive ? result.emissive : new TextureDetails;
					result.emissive->texture = mat->occlusionTexture;
					result.emissive->channel = emissiveFileTexture->UVSet.Get();
					result.emissive->scale.set(emissiveFileTexture->GetScaleU(), emissiveFileTexture->GetScaleV());
				}
				FbxDouble4 emissiveColor = getValue(pbrProps, "emit_color", FbxDouble4(0.5, 0.5, 0.5, 1.0));
				mat->emissiveFactor = { emissiveColor[0],emissiveColor[1],emissiveColor[2] };

				const FbxFileTexture* baseColorFileTexture = getTex("base_color");
				if (baseColorFileTexture) {
					mat->baseColorTexture = fbxTextureToOsgTexture(baseColorFileTexture);
					result.diffuse = result.diffuse ? result.diffuse : new TextureDetails;
					result.diffuse->texture = mat->baseColorTexture;
					result.diffuse->channel = baseColorFileTexture->UVSet.Get();
					result.diffuse->scale.set(baseColorFileTexture->GetScaleU(), baseColorFileTexture->GetScaleV());
				}
				FbxDouble4 baseColor = getValue(pbrProps, "basecolor", FbxDouble4(0.5, 0.5, 0.5, 1.0));
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
					osg::ref_ptr<osg::Image> metallicRoughnessImage = mat->mergeImagesToMetallicRoughnessImage(metalnessImageData, roughnessImageData);
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
				OSG_INFO << ("3dsMax Specular/Glossiness Material.") << std::endl;
				pOsgMat = new GltfPbrSpecularGlossinessMaterial;
				pOsgMat->setName(pFbxMat->GetName());
				result.material = pOsgMat;
				osg::ref_ptr<GltfPbrSpecularGlossinessMaterial> mat = dynamic_cast<GltfPbrSpecularGlossinessMaterial*>(pOsgMat.get());

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
					result.normalMap = result.normalMap ? result.normalMap : new TextureDetails;
					result.normalMap->texture = mat->normalTexture;
					result.normalMap->channel = normalFileTexture->UVSet.Get();
					result.normalMap->scale.set(normalFileTexture->GetScaleU(), normalFileTexture->GetScaleV());
				}

				const FbxFileTexture* aoFileTexture = getTex("ao");
				if (aoFileTexture) {
					mat->occlusionTexture = fbxTextureToOsgTexture(aoFileTexture);
					result.ambient = result.ambient ? result.ambient : new TextureDetails;
					result.ambient->texture = mat->occlusionTexture;
					result.ambient->channel = aoFileTexture->UVSet.Get();
					result.ambient->scale.set(aoFileTexture->GetScaleU(), aoFileTexture->GetScaleV());
				}

				const FbxFileTexture* emissiveFileTexture = getTex("emit_color");
				if (emissiveFileTexture) {
					mat->emissiveTexture = fbxTextureToOsgTexture(emissiveFileTexture);
					result.emissive = result.emissive ? result.emissive : new TextureDetails;
					result.emissive->texture = mat->occlusionTexture;
					result.emissive->channel = emissiveFileTexture->UVSet.Get();
					result.emissive->scale.set(emissiveFileTexture->GetScaleU(), emissiveFileTexture->GetScaleV());
				}
				FbxDouble4 emissiveColor = getValue(pbrProps, "emit_color", FbxDouble4(0.5, 0.5, 0.5, 1.0));
				mat->emissiveFactor = { emissiveColor[0],emissiveColor[1],emissiveColor[2] };

				FbxDouble4 Specular = getValue(pbrProps, "Specular", FbxDouble4(1.0, 1.0, 1.0, 1.0));
				mat->specularFactor = { Specular[0],Specular[1],Specular[2] };
				mat->glossinessFactor = getValue(pbrProps, "glossiness", 1.0);

				const FbxFileTexture* baseColorFileTexture = getTex("base_color");
				if (baseColorFileTexture) {
					osg::ref_ptr<osg::Texture2D> baseColorMap = fbxTextureToOsgTexture(baseColorFileTexture);
					mat->diffuseTexture = baseColorMap;
					result.diffuse = result.diffuse ? result.diffuse : new TextureDetails;
					result.diffuse->texture = baseColorMap;
					result.diffuse->channel = baseColorFileTexture->UVSet.Get();
					result.diffuse->scale.set(baseColorFileTexture->GetScaleU(), baseColorFileTexture->GetScaleV());
				}
				FbxDouble4 baseColor = getValue(pbrProps, "basecolor", FbxDouble4(1.0, 1.0, 1.0, 1.0));
				mat->diffuseFactor = { baseColor[0],baseColor[1],baseColor[2],baseColor[3] };

				//const FbxFileTexture* specularFileTexture = getTex("specular");
				//const FbxFileTexture* glossinessFileTexture = getTex("glossiness");
				const FbxFileTexture* glossinessFileTexture = getTex("specular");
				const FbxFileTexture* specularFileTexture = getTex("glossiness");
				osg::ref_ptr<osg::Texture2D> specularMap = specularFileTexture ? fbxTextureToOsgTexture(specularFileTexture) : NULL;
				osg::ref_ptr<osg::Texture2D> glossinessMap = glossinessFileTexture ? fbxTextureToOsgTexture(glossinessFileTexture) : NULL;
				if (specularMap && glossinessMap) {
					osg::ref_ptr<osg::Image> specularImageData = specularMap->getImage();
					osg::ref_ptr<osg::Image> glossinessImageData = glossinessMap->getImage();
					osg::ref_ptr<osg::Image> specularGlossinessImage = mat->mergeImagesToSpecularGlossinessImage(specularImageData, glossinessImageData);
					osg::ref_ptr<osg::Texture2D> specularGlossinessTexture = new osg::Texture2D;
					specularGlossinessTexture->setImage(specularGlossinessImage);

					mat->specularGlossinessTexture = specularGlossinessTexture;

					result.specular = result.specular ? result.specular : new TextureDetails;
					result.specular->texture = specularMap;
					result.specular->channel = specularFileTexture->UVSet.Get();
					result.specular->scale.set(specularFileTexture->GetScaleU(), specularFileTexture->GetScaleV());
					result.shininess = result.shininess ? result.shininess : new TextureDetails;
					result.shininess->texture = glossinessMap;
					result.shininess->channel = glossinessFileTexture->UVSet.Get();
					result.shininess->scale.set(glossinessFileTexture->GetScaleU(), glossinessFileTexture->GetScaleV());

				}
				else if (specularMap && !glossinessMap) {
					mat->specularGlossinessTexture = specularMap;
					result.specular = result.specular ? result.specular : new TextureDetails;
					result.specular->texture = mat->specularGlossinessTexture;
					result.specular->channel = specularFileTexture->UVSet.Get();
					result.specular->scale.set(specularFileTexture->GetScaleU(), specularFileTexture->GetScaleV());
				}
				else if (!specularMap && glossinessMap) {
					mat->specularGlossinessTexture = glossinessMap;
					result.shininess = result.shininess ? result.shininess : new TextureDetails;
					result.shininess->texture = mat->specularGlossinessTexture;
					result.shininess->channel = glossinessFileTexture->UVSet.Get();
					result.shininess->scale.set(glossinessFileTexture->GetScaleU(), glossinessFileTexture->GetScaleV());
				}

			}
		}

	}

	if (pFbxLambert)
	{
		FbxDouble3 color = pFbxLambert->Diffuse.Get();
		double factor = pFbxLambert->DiffuseFactor.Get();
		double transparencyFactor = useTransparencyColorFactor ? transparencyColorFactor : pFbxLambert->TransparencyFactor.Get();
		pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(
			static_cast<float>(color[0] * factor),
			static_cast<float>(color[1] * factor),
			static_cast<float>(color[2] * factor),
			static_cast<float>(1.0 - transparencyFactor)));

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
		
		OSG_INFO << ("3dsMax Traditional Material.") << std::endl;
		osg::ref_ptr<GltfPbrMetallicRoughnessMaterial> mat = dynamic_cast<GltfPbrMetallicRoughnessMaterial*>(pOsgMat.get());
	
		FbxFileTexture* diffuseTexture = selectFbxFileTexture(pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse));
		if (diffuseTexture)
		{
			mat->baseColorTexture = fbxTextureToOsgTexture(diffuseTexture);
		}
		mat->emissiveStrength = 1.0f;
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
		else {
			mat->metallicFactor = 0.2;
			mat->roughnessFactor = 0.8;
		}
	}
	else
	{
		FbxProperty diffuseProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse);
		if (diffuseProperty.IsValid()) {
			FbxDouble3 diffuseColor = diffuseProperty.Get<FbxDouble3>();
			FbxProperty diffuseFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
			osg::ref_ptr<GltfPbrMetallicRoughnessMaterial> mat = dynamic_cast<GltfPbrMetallicRoughnessMaterial*>(pOsgMat.get());
			if(diffuseFactorProperty.IsValid())
			{
				FbxDouble diffuseFactor = diffuseFactorProperty.Get<FbxDouble>();
				osg::Vec4 color(
					static_cast<float>(diffuseColor[0] * diffuseFactor),
					static_cast<float>(diffuseColor[1] * diffuseFactor),
					static_cast<float>(diffuseColor[2] * diffuseFactor),
					static_cast<float>(1.0 * diffuseFactor));
				pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, color);
				mat->baseColorFactor = { color[0],color[1],color[2],color[3] };
			}else
			{
				osg::Vec4 color(
					static_cast<float>(diffuseColor[0]),
					static_cast<float>(diffuseColor[1]),
					static_cast<float>(diffuseColor[2]),
					static_cast<float>(1.0));
				pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, color);
				mat->baseColorFactor = { color[0],color[1],color[2],color[3] };

			}
		}
	}

	if (_lightmapTextures)
	{
		// if using an emission map then adjust material properties accordingly...
		if (result.emissive.valid())
		{
			osg::Vec4 diffuse = pOsgMat->getDiffuse(osg::Material::FRONT_AND_BACK);
			pOsgMat->setEmission(osg::Material::FRONT_AND_BACK, diffuse);
			pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0, 0, 0, diffuse.a()));
			pOsgMat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0, 0, 0, diffuse.a()));
		}
	}




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
	const std::string& fbxFilename(fbx->GetFileName());
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
