#pragma once
#include "Asset/Asset.h"
#include "Asset/Texture/Texture.h"
#include "RHI/Device/GraphicsDevice.h"
#include "RHI/Shader/Shader.h"
#include <span>

// 描画（Render/Renderer と Render/Features）の .cpp だけが使う定数と小さな関数
namespace rd::render
{
	inline constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	inline constexpr DXGI_FORMAT kBloomFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	inline constexpr DXGI_FORMAT kOutputRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	inline constexpr DXGI_FORMAT kOutlineMaskFormat = DXGI_FORMAT_R8_UNORM;
	// G バッファー（0: 色 + 金属感、1: 法線 + 粗さ、2: 発光 + 種類）。色も 1 を超えることがあるので 16 ビットの浮動小数にする
	inline constexpr uint32_t kGBufferCount = 3;
	inline constexpr DXGI_FORMAT kGBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
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

	// 頂点の並び（メッシュは 1 本目、スキンの情報は 2 本目の頂点バッファから読む）
	inline const D3D12_INPUT_ELEMENT_DESC kStaticLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	inline const D3D12_INPUT_ELEMENT_DESC kSkinnedLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	// デバッグ線・半透明の三角形（位置と色）
	inline const D3D12_INPUT_ELEMENT_DESC kLineLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	inline const D3D12_INPUT_ELEMENT_DESC kSpriteLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	inline std::span<const D3D12_INPUT_ELEMENT_DESC> MeshLayout(bool skinned)
	{
		return skinned ? std::span<const D3D12_INPUT_ELEMENT_DESC>(kSkinnedLayout) : std::span<const D3D12_INPUT_ELEMENT_DESC>(kStaticLayout);
	}

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
