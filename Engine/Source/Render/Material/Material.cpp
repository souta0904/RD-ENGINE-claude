#include "RdPch.h"
#include "Render/Material/Material.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture/Texture.h"

namespace rd
{
	namespace
	{
		void ResolveTexture(const std::string& path, AssetRef<Texture>& tex, bool srgb)
		{
			if (path.empty()) { tex.reset(); return; }
			std::string key = srgb || path.ends_with("#linear") ? path : path + "#linear";
			if (!tex || tex->GetPath() != key) { tex = AssetManager::Get().LoadTexture(path, srgb); }
		}
	}

	void Material::Reflect(PropertyVisitor& v)
	{
		v.Property("Base Color", baseColor);
		v.Property("Albedo Map", albedoPath, { .flags = PropertyMeta::AssetTexture });
		v.Property("Shading", shading);
		v.Property("Shadow Color", shadowColor);
		v.Property("Rim Color", rimColor);
		v.Property("Rim Power", rimPower, { .speed = 0.05f, .min = 0.5f, .max = 8.0f });
		v.Property("Triplanar", triplanar);
		v.Property("Triplanar Tile", triplanarTile, { .speed = 0.01f, .min = 0.0001f, .max = 100.0f });
		v.Property("Normal Map", normalPath, { .flags = PropertyMeta::AssetTexture });
		v.Property("Metallic", metallic, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Roughness", roughness, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Emissive", emissive);
		v.Property("Emissive Intensity", emissiveIntensity, { .speed = 0.05f, .min = 0.0f, .max = 100.0f });
		v.Property("Emissive Map", emissivePath, { .flags = PropertyMeta::AssetTexture });
		v.Property("Blend", blend);
		v.Property("Double Sided", doubleSided);
		v.Property("Unlit", unlit);
		v.Property("Alpha Clip", alphaClip);
		v.Property("Alpha Cutoff", alphaCutoff, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Cast Shadows", castShadows);
		v.Property("Receive Shadows", receiveShadows);
	}

	void Material::ResolveTextures()
	{
		ResolveTexture(albedoPath, albedo, true);
		ResolveTexture(normalPath, normal, false);
		ResolveTexture(emissivePath, emissiveMap, true);
	}

	GpuMaterialData Material::ToGpu() const
	{
		GpuMaterialData d;
		d.baseColor = baseColor;
		d.emissive = Vector3(emissive.r, emissive.g, emissive.b) * emissiveIntensity;
		d.metallic = metallic;
		d.roughness = roughness;
		d.alphaCutoff = alphaCutoff;
		if (albedo && albedo->IsReady()) { d.flags |= kMatHasAlbedo; }
		if (normal && normal->IsReady()) { d.flags |= kMatHasNormal; }
		if (emissiveMap && emissiveMap->IsReady()) { d.flags |= kMatHasEmissive; }
		if (unlit) { d.flags |= kMatUnlit; }
		if (alphaClip) { d.flags |= kMatAlphaClip; }
		if (receiveShadows) { d.flags |= kMatReceiveShadow; }
		if (shading == ShadingModel::Flat) { d.flags |= kMatFlat; }
		if (triplanar) { d.flags |= kMatTriplanar; }
		d.triplanarTile = triplanarTile;
		d.shadowColor = Vector3(shadowColor.r, shadowColor.g, shadowColor.b);
		d.rimColor = Vector3(rimColor.r, rimColor.g, rimColor.b);
		d.rimPower = rimPower;
		return d;
	}
}
