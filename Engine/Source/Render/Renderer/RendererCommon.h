#pragma once
#include "Asset/Asset.h"
#include "Asset/Texture/Texture.h"
#include "RHI/Device/GraphicsDevice.h"
#include "RHI/Shader/Shader.h"

// Renderer の .cpp だけが使う定数と小さな関数
namespace rd::render
{
	inline constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	inline constexpr DXGI_FORMAT kBloomFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	inline constexpr DXGI_FORMAT kOutputRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	inline constexpr DXGI_FORMAT kOutlineMaskFormat = DXGI_FORMAT_R8_UNORM;
	inline constexpr const char* kShaderDir = "Assets/Engine/Shaders/";

	// メッシュ用のルートシグネチャの引数の番号
	enum MeshRootParam : UINT
	{
		kRootObject = 0,
		kRootFrame,
		kRootMaterial,
		kRootLights,
		kRootAlbedo,
		kRootNormal,
		kRootPalette,
		kRootShadow,
		kRootEmissive,
	};

	// 2D / ポストエフェクト用のルートシグネチャの引数の番号
	enum SimpleRootParam : UINT
	{
		kSimpleConstants = 0,
		kSimpleTex0,
		kSimpleTex1,
	};

	struct SimpleConstants
	{
		Matrix4 matrix;
		Vector4 params;
	};

	struct SpriteVertex
	{
		Vector2 position;
		Vector2 uv;
		Color color;
	};

	inline const Shader* CompileShader(const char* file, const char* entry, ShaderStage stage, std::vector<std::string> defines = {})
	{
		return GraphicsDevice::Get().GetShaderCompiler().Compile({ std::string(kShaderDir) + file, entry, stage, std::move(defines) });
	}

	// 読み込みが終わっていなければ代わりのテクスチャを使う
	inline D3D12_GPU_DESCRIPTOR_HANDLE TextureSrv(const Texture* tex, const AssetRef<Texture>& fallback)
	{
		return (tex && tex->IsReady()) ? tex->GetSrv() : fallback->GetSrv();
	}
}
