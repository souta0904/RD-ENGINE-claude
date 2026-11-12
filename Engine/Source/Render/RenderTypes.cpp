#include "RdPch.h"
#include "Render/RenderTypes.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture.h"

namespace rd
{
	// ==================================================
	// Material
	// ==================================================
	void Material::Reflect(PropertyVisitor& v)
	{
		v.Property("Base Color", baseColor);
		v.Property("Albedo Map", albedoPath, { .flags = PropertyMeta::AssetTexture });
		v.Property("Normal Map", normalPath, { .flags = PropertyMeta::AssetTexture });
		v.Property("Metallic", metallic, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Roughness", roughness, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Emissive", emissive);
		v.Property("Emissive Intensity", emissiveIntensity, { .speed = 0.05f, .min = 0.0f, .max = 100.0f });
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
		auto& assets = AssetManager::Get();
		auto resolve = [&](const std::string& path, AssetRef<Texture>& tex, bool srgb)
			{
				if (path.empty()) { tex.reset(); return; }
				std::string key = srgb || path.ends_with("#linear") ? path : path + "#linear";
				if (!tex || tex->GetPath() != key) { tex = assets.LoadTexture(path, srgb); }
			};
		resolve(albedoPath, albedo, true);
		resolve(normalPath, normal, false);
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
		if (unlit) { d.flags |= kMatUnlit; }
		if (alphaClip) { d.flags |= kMatAlphaClip; }
		if (receiveShadows) { d.flags |= kMatReceiveShadow; }
		return d;
	}

	// ==================================================
	// RenderCamera
	// ==================================================
	RenderCamera RenderCamera::Make(const Vector3& position, const Quaternion& rotation, float fovY, float aspect, float nearZ, float farZ)
	{
		RenderCamera c;
		c.position = position;
		c.rotation = rotation;
		c.fovY = fovY;
		c.nearZ = nearZ;
		c.farZ = farZ;
		c.view = Inverse(Matrix4::CreateRotate(rotation) * Matrix4::CreateTranslate(position));
		c.proj = Matrix4::CreatePerspectiveFov(fovY, aspect, nearZ, farZ);
		return c;
	}

	Ray RenderCamera::ScreenPointToRay(const Vector2& uv) const
	{
		Matrix4 invVP = Inverse(view * proj);
		Vector2 ndc(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
		Vector3 nearP = TransformCoord(Vector3(ndc.x, ndc.y, 0.0f), invVP);
		Vector3 farP = TransformCoord(Vector3(ndc.x, ndc.y, 1.0f), invVP);
		return { nearP, Normalize(farP - nearP) };
	}

	// ==================================================
	// Environment / RenderSettings
	// ==================================================
	void Environment::Reflect(PropertyVisitor& v)
	{
		v.Property("Ambient Sky", ambientSky);
		v.Property("Ambient Ground", ambientGround);
		v.Property("Ambient Intensity", ambientIntensity, { .speed = 0.01f, .min = 0.0f, .max = 10.0f });
		v.Property("Draw Sky", drawSky);
		v.Property("Sky Zenith", skyZenith, { .flags = PropertyMeta::HDR });
		v.Property("Sky Horizon", skyHorizon, { .flags = PropertyMeta::HDR });
		v.Property("Fog Color", fogColor);
		v.Property("Fog Density", fogDensity, { .speed = 0.001f, .min = 0.0f, .max = 1.0f });
		v.Property("Fog Start", fogStart, { .speed = 0.1f, .min = 0.0f, .max = 10000.0f });
	}

	void RenderSettings::Reflect(PropertyVisitor& v)
	{
		if (v.BeginGroup("Shadow"))
		{
			v.Property("Enabled", shadows);
			v.Property("Map Size", shadowMapSize, { .speed = 64.0f, .min = 256.0f, .max = 8192.0f });
			v.Property("Distance", shadowDistance, { .speed = 0.5f, .min = 1.0f, .max = 1000.0f });
			v.Property("Bias", shadowBias, { .speed = 0.00005f, .min = 0.0f, .max = 0.05f });
			v.Property("Strength", shadowStrength, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
			v.EndGroup();
		}
		if (v.BeginGroup("Post Process"))
		{
			v.Property("Bloom", bloom);
			v.Property("Bloom Threshold", bloomThreshold, { .speed = 0.01f, .min = 0.0f, .max = 20.0f });
			v.Property("Bloom Knee", bloomKnee, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
			v.Property("Bloom Intensity", bloomIntensity, { .speed = 0.01f, .min = 0.0f, .max = 10.0f });
			v.Property("Exposure", exposure, { .speed = 0.01f, .min = 0.01f, .max = 20.0f });
			v.Property("Tonemap (ACES)", tonemap);
			v.Property("Vignette", vignette, { .speed = 0.01f, .min = 0.0f, .max = 2.0f });
			v.EndGroup();
		}
		if (v.BeginGroup("Debug"))
		{
			v.Property("Frustum Culling", frustumCulling);
			v.Property("Wireframe", wireframe);
			v.EndGroup();
		}
	}
}
