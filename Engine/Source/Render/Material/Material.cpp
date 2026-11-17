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
		return d;
	}
}
