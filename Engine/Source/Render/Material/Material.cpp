#include "RdPch.h"
#include "Render/Material/Material.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture/Texture.h"
#include "Core/Util/StringUtil.h"
#include "Render/Material/ShaderAsset.h"
#include <bit>

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

	// 自作のシェーダーの欄を先頭に置き（Unity と同じ）、その値は最後に「Shader Properties」としてまとめる
	void Material::Reflect(PropertyVisitor& v)
	{
		if (v.Property("Shader", shaderPath, { .flags = PropertyMeta::AssetShader,
			.tooltip = "Surface shader (.hlsl that includes Surface.hlsli). Empty = Standard" }))
		{
			ResolveShader();
		}
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
		ReflectShaderProperties(v);
	}

	// シェーダーの Properties の並びで、種類に合った欄を出す。保存のキーは変数の名前、画面には表示名
	//  シェーダーにない名前の値は保存しない（シェーダーから消した値は、次に保存したときに消える）
	void Material::ReflectShaderProperties(PropertyVisitor& v)
	{
		if (shaderPath.empty()) { return; }
		ResolveShader();
		if (!shader || !shader->IsReady() || shader->GetProperties().empty()) { return; }
		// シェーダーの値はいちばんよく触るので、はじめから開いておく（Unity と同じ）
		v.SetNextGroupOpen();
		if (!v.BeginGroup("Shader Properties")) { return; }
		for (const ShaderProperty& p : shader->GetProperties())
		{
			MaterialPropertyValue& val = GetOrCreateProperty(p);
			const char* key = p.name.c_str();
			PropertyMeta meta;
			meta.label = p.displayName.c_str();
			switch (p.type)
			{
			case ShaderProperty::Type::Color:
			{
				Color c(val.value.x, val.value.y, val.value.z, val.value.w);
				if (p.hdr) { meta.flags = PropertyMeta::HDR; }
				if (v.Property(key, c, meta)) { val.value = Vector4(c.r, c.g, c.b, c.a); }
				break;
			}
			case ShaderProperty::Type::Vector:
				meta.speed = 0.01f;
				v.Property(key, val.value, meta);
				break;
			case ShaderProperty::Type::Float:
				if (p.toggle)
				{
					bool on = val.value.x != 0.0f;
					if (v.Property(key, on, meta)) { val.value.x = on ? 1.0f : 0.0f; }
				}
				else
				{
					meta.speed = 0.01f;
					v.Property(key, val.value.x, meta);
				}
				break;
			case ShaderProperty::Type::Range:
				meta.min = p.min;
				meta.max = p.max;
				meta.speed = std::max((p.max - p.min) / 200.0f, 0.0001f);
				if (v.Property(key, val.value.x, meta)) { val.value.x = std::clamp(val.value.x, p.min, p.max); }
				break;
			case ShaderProperty::Type::Int:
			{
				int32_t i = int32_t(std::lround(val.value.x));
				meta.speed = 0.2f;
				if (v.Property(key, i, meta)) { val.value.x = float(i); }
				break;
			}
			case ShaderProperty::Type::Texture:
				meta.flags = PropertyMeta::AssetTexture;
				v.Property(key, val.texturePath, meta);
				break;
			}
		}
		v.EndGroup();
	}

	MaterialPropertyValue& Material::GetOrCreateProperty(const ShaderProperty& p)
	{
		auto [it, inserted] = properties.try_emplace(p.name);
		if (inserted) { it->second.value = p.defaultValue; }
		return it->second;
	}

	// エンジンを起動していない（テストなど）ときは読まない
	void Material::ResolveShader()
	{
		if (shaderPath.empty()) { shader.reset(); return; }
		if (!AssetManager::IsAvailable()) { return; }
		if (!shader || shader->GetPath() != NormalizePath(shaderPath)) { shader = AssetManager::Get().LoadNow<ShaderAsset>(shaderPath); }
	}

	void Material::ResolveTextures()
	{
		ResolveTexture(albedoPath, albedo, true);
		ResolveTexture(normalPath, normal, false);
		ResolveTexture(emissivePath, emissiveMap, true);
		ResolveShader();
		if (const ShaderAsset* s = GetCustomShader())
		{
			for (const ShaderProperty& p : s->GetProperties())
			{
				if (p.type != ShaderProperty::Type::Texture) { continue; }
				MaterialPropertyValue& val = GetOrCreateProperty(p);
				// 法線マップ（既定値が bump）は色ではないので sRGB にしない
				ResolveTexture(val.texturePath, val.texture, p.defaultTexture != "bump");
			}
		}
	}

	const ShaderAsset* Material::GetCustomShader() const
	{
		return !shaderPath.empty() && shader && shader->IsReady() ? shader.get() : nullptr;
	}

	// Int は整数のビットのまま入れる（シェーダーの int で読むので、float の値のままでは違う数になる）
	void Material::BuildShaderConstants(std::vector<Vector4>& out) const
	{
		out.clear();
		const ShaderAsset* s = GetCustomShader();
		if (!s) { return; }
		for (const ShaderProperty& p : s->GetProperties())
		{
			if (p.type == ShaderProperty::Type::Texture) { continue; }
			auto it = properties.find(p.name);
			Vector4 v = it != properties.end() ? it->second.value : p.defaultValue;
			switch (p.type)
			{
			case ShaderProperty::Type::Color:
			case ShaderProperty::Type::Vector:
				out.push_back(v);
				break;
			case ShaderProperty::Type::Int:
				out.push_back(Vector4(std::bit_cast<float>(int32_t(std::lround(v.x))), 0.0f, 0.0f, 0.0f));
				break;
			default:
				out.push_back(Vector4(v.x, 0.0f, 0.0f, 0.0f));
				break;
			}
		}
	}

	const Texture* Material::GetShaderTexture(int slot) const
	{
		const ShaderAsset* s = GetCustomShader();
		if (!s) { return nullptr; }
		for (const ShaderProperty& p : s->GetProperties())
		{
			if (p.textureSlot != slot) { continue; }
			auto it = properties.find(p.name);
			if (it == properties.end() || !it->second.texture || !it->second.texture->IsReady()) { return nullptr; }
			return it->second.texture.get();
		}
		return nullptr;
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
