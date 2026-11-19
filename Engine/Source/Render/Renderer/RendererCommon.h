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
		// 自作のシェーダーの Properties（b4 の定数バッファと t5 から 4 枚のテクスチャ）
		kRootShaderProps,
		kRootShaderTex0,
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

	// ファイルの中身ではなく、渡した文字列をコンパイルする（自作のシェーダーに宣言を足したもの）
	//  path はエラーの表示と、相対の include を探す場所に使う
	inline const Shader* CompileShaderSource(const std::string& path, const std::string& source, const char* entry, ShaderStage stage, std::vector<std::string> defines = {})
	{
		ShaderDesc desc{ path, entry, stage, std::move(defines) };
		desc.source = source;
		return GraphicsDevice::Get().GetShaderCompiler().Compile(desc);
	}

	// 読み込みが終わっていなければ代わりのテクスチャを使う
	inline D3D12_GPU_DESCRIPTOR_HANDLE TextureSrv(const Texture* tex, const AssetRef<Texture>& fallback)
	{
		return (tex && tex->IsReady()) ? tex->GetSrv() : fallback->GetSrv();
	}
}
