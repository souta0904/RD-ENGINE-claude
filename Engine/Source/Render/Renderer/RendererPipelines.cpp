#include "RdPch.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Pipeline/PipelineBuilder.h"
#include "RHI/Pipeline/RootSignatureBuilder.h"
#include "RHI/Pipeline/Samplers.h"
#include "Render/Material/ShaderAsset.h"

namespace rd
{
	using namespace render;

	namespace
	{
		const D3D12_INPUT_ELEMENT_DESC kStaticLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		// スキンの情報は 2 本目の頂点バッファから読む
		const D3D12_INPUT_ELEMENT_DESC kSkinnedLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		const D3D12_INPUT_ELEMENT_DESC kLineLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		const D3D12_INPUT_ELEMENT_DESC kSpriteLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		ComPtr<ID3D12PipelineState> BuildPostPipeline(ID3D12Device* device, ID3D12RootSignature* rs, const Shader* vs,
			const char* ps, DXGI_FORMAT format, BlendMode blend, const wchar_t* name)
		{
			return PipelineBuilder()
				.SetRootSignature(rs)
				.SetVS(vs)
				.SetPS(CompileShader("Post.hlsl", ps, ShaderStage::Pixel))
				.SetBlend(blend)
				.SetCull(CullMode::None)
				.SetDepth(DepthMode::Disabled)
				.SetRenderTargets({ format }, DXGI_FORMAT_UNKNOWN)
				.Build(device, name);
		}
	}

	// メッシュ: b0 Object / b1 Frame / b2 Material / b3 Lights / t0 Albedo / t1 Normal / t2 Palette（またはパーティクル）/ t3 ShadowMap / t4 Emissive
	//  b4 と t5～t8 は自作のシェーダーの Properties（頂点を動かす関数でも読めるように、どの段からも見える）
	void Renderer::CreateRootSignatures()
	{
		auto device = GraphicsDevice::Get().GetDevice();
		RootSignatureBuilder mesh;
		mesh.AddCbv(0)
			.AddCbv(1)
			.AddCbv(2)
			.AddCbv(3)
			.AddSrvTable(0)
			.AddSrvTable(1)
			.AddSrv(2, D3D12_SHADER_VISIBILITY_VERTEX)
			.AddSrvTable(3)
			.AddSrvTable(4)
			.AddCbv(4);
		for (int i = 0; i < ShaderAsset::kMaxTextures; ++i) { mesh.AddSrvTable(5 + i, 1, D3D12_SHADER_VISIBILITY_ALL); }
		mMeshRS = mesh
			.AddStaticSampler(Samplers::AnisotropicWrap(0))
			.AddStaticSampler(Samplers::LinearClamp(1))
			.AddStaticSampler(Samplers::ShadowCompare(2))
			.Build(device, L"MeshRS");

		mSimpleRS = RootSignatureBuilder()
			.AddConstants(0, sizeof(SimpleConstants) / 4)
			.AddSrvTable(0)
			.AddSrvTable(1)
			.AddStaticSampler(Samplers::LinearClamp(0))
			.AddStaticSampler(Samplers::PointClamp(1))
			.AddStaticSampler(Samplers::LinearWrap(2))
			.Build(device, L"SimpleRS");
	}

	void Renderer::CreatePipelines()
	{
		auto device = GraphicsDevice::Get().GetDevice();

		mSkyPSO = PipelineBuilder()
			.SetRootSignature(mMeshRS.Get())
			.SetVS(CompileShader("Sky.hlsl", "VSMain", ShaderStage::Vertex))
			.SetPS(CompileShader("Sky.hlsl", "PSMain", ShaderStage::Pixel))
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::ReadOnly, D3D12_COMPARISON_FUNC_LESS_EQUAL)
			.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat)
			.Build(device, L"SkyPSO");

		PipelineBuilder particle;
		particle.SetRootSignature(mMeshRS.Get())
			.SetVS(CompileShader("Particle.hlsl", "VSMain", ShaderStage::Vertex))
			.SetPS(CompileShader("Particle.hlsl", "PSMain", ShaderStage::Pixel))
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::ReadOnly)
			.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat);
		mParticleAlphaPSO = particle.SetBlend(BlendMode::Alpha).Build(device, L"ParticleAlphaPSO");
		mParticleAddPSO = particle.SetBlend(BlendMode::Additive).Build(device, L"ParticleAddPSO");

		PipelineBuilder line;
		line.SetRootSignature(mSimpleRS.Get())
			.SetVS(CompileShader("Simple.hlsl", "LineVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "LinePS", ShaderStage::Pixel))
			.SetInputLayout(kLineLayout)
			.SetTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetRenderTargets({ kOutputRtvFormat }, DepthTexture::kDsvFormat);
		mLineDepthPSO = line.SetDepth(DepthMode::ReadOnly).Build(device, L"LineDepthPSO");
		mLineOverlayPSO = line.SetDepth(DepthMode::Disabled).Build(device, L"LineOverlayPSO");

		// デバッグ描画の面。線と同じシェーダと頂点で、三角形として描く（トーンマップの後の画面へ直接描く）
		PipelineBuilder debugTri;
		debugTri.SetRootSignature(mSimpleRS.Get())
			.SetVS(CompileShader("Simple.hlsl", "LineVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "LinePS", ShaderStage::Pixel))
			.SetInputLayout(kLineLayout)
			.SetTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetRenderTargets({ kOutputRtvFormat }, DepthTexture::kDsvFormat);
		mDebugTriDepthPSO = debugTri.SetDepth(DepthMode::ReadOnly).Build(device, L"DebugTriDepthPSO");
		mDebugTriOverlayPSO = debugTri.SetDepth(DepthMode::Disabled).Build(device, L"DebugTriOverlayPSO");

		// 線と同じシェーダで三角形を描く（HDR に描くのでトーンマップがかかる）
		mTranslucentTrianglePSO = PipelineBuilder()
			.SetRootSignature(mSimpleRS.Get())
			.SetVS(CompileShader("Simple.hlsl", "LineVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "LinePS", ShaderStage::Pixel))
			.SetInputLayout(kLineLayout)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::ReadOnly)
			.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat)
			.Build(device, L"TranslucentTrianglePSO");

		// 縁取り: 形を印に塗る → 広げた形を HDR に描く（どちらも深度は見ない）
		for (int skinned = 0; skinned < 2; ++skinned)
		{
			std::vector<std::string> defines;
			if (skinned) { defines.push_back("SKINNED=1"); }
			auto layout = skinned ? std::span<const D3D12_INPUT_ELEMENT_DESC>(kSkinnedLayout) : std::span<const D3D12_INPUT_ELEMENT_DESC>(kStaticLayout);
			mOutlineMaskPSO[skinned] = PipelineBuilder()
				.SetRootSignature(mMeshRS.Get())
				.SetVS(CompileShader("Outline.hlsl", "MaskVS", ShaderStage::Vertex, defines))
				.SetPS(CompileShader("Outline.hlsl", "MaskPS", ShaderStage::Pixel, defines))
				.SetInputLayout(layout)
				.SetCull(CullMode::None)
				.SetDepth(DepthMode::Disabled)
				.SetRenderTargets({ kOutlineMaskFormat }, DXGI_FORMAT_UNKNOWN)
				.Build(device, L"OutlineMaskPSO");
			mOutlinePSO[skinned] = PipelineBuilder()
				.SetRootSignature(mMeshRS.Get())
				.SetVS(CompileShader("Outline.hlsl", "OutlineVS", ShaderStage::Vertex, defines))
				.SetPS(CompileShader("Outline.hlsl", "OutlinePS", ShaderStage::Pixel, defines))
				.SetInputLayout(layout)
				.SetBlend(BlendMode::Alpha)
				.SetCull(CullMode::None)
				.SetDepth(DepthMode::Disabled)
				.SetRenderTargets({ kHdrFormat }, DXGI_FORMAT_UNKNOWN)
				.Build(device, L"OutlinePSO");
		}

		mSpritePSO = PipelineBuilder()
			.SetRootSignature(mSimpleRS.Get())
			.SetVS(CompileShader("Simple.hlsl", "SpriteVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "SpritePS", ShaderStage::Pixel))
			.SetInputLayout(kSpriteLayout)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::Disabled)
			.SetRenderTargets({ kOutputRtvFormat }, DepthTexture::kDsvFormat)
			.Build(device, L"SpritePSO");

		const Shader* fullscreen = CompileShader("Post.hlsl", "FullscreenVS", ShaderStage::Vertex);
		ID3D12RootSignature* rs = mSimpleRS.Get();
		mBloomPrefilterPSO = BuildPostPipeline(device, rs, fullscreen, "BloomPrefilterPS", kBloomFormat, BlendMode::Opaque, L"BloomPrefilterPSO");
		mBloomDownPSO = BuildPostPipeline(device, rs, fullscreen, "BloomDownPS", kBloomFormat, BlendMode::Opaque, L"BloomDownPSO");
		mBloomUpPSO = BuildPostPipeline(device, rs, fullscreen, "BloomUpPS", kBloomFormat, BlendMode::Additive, L"BloomUpPSO");
		mTonemapPSO = BuildPostPipeline(device, rs, fullscreen, "TonemapPS", kOutputRtvFormat, BlendMode::Opaque, L"TonemapPSO");
		mBlitPSO = BuildPostPipeline(device, rs, fullscreen, "BlitPS", GraphicsDevice::kBackBufferFormat, BlendMode::Opaque, L"BlitPSO");
	}

	// 組み合わせごとに初めて使うときに作ってキャッシュする（キーはビットで組み合わせを表す。上位は自作のシェーダーの番号）
	//  自作のシェーダーがコンパイルできなければ、標準のシェーダーのパイプラインを代わりに覚えておく
	//  （毎フレームコンパイルし直さないため。直したら Reload Shaders で作り直す）
	ID3D12PipelineState* Renderer::GetMeshPipeline(bool skinned, BlendMode blend, bool doubleSided, bool shadow, const ShaderAsset* custom)
	{
		bool wire = mSettings.wireframe && !shadow;
		if (shadow) { custom = nullptr; } // 影は形だけなので、どのシェーダーでも同じものを使う
		uint64_t key = (skinned ? 1u : 0u) | (uint32_t(blend) << 1) | (doubleSided ? 16u : 0u) | (shadow ? 32u : 0u) | (wire ? 64u : 0u);
		if (custom) { key |= uint64_t(custom->GetId()) << 8; }
		if (auto it = mMeshPipelines.find(key); it != mMeshPipelines.end()) { return it->second.Get(); }

		std::vector<std::string> defines;
		if (skinned) { defines.push_back("SKINNED=1"); }
		const Shader* vs = nullptr;
		const Shader* ps = nullptr;
		if (custom)
		{
			std::string source = custom->GetPrelude() + custom->GetSource();
			vs = CompileShaderSource(custom->GetPath(), source, "VSMain", ShaderStage::Vertex, defines);
			ps = CompileShaderSource(custom->GetPath(), source, "PSMain", ShaderStage::Pixel, defines);
			if (!vs || !vs->IsValid() || !ps || !ps->IsValid())
			{
				Logger::Error(LogCat::Render, "Shader '{}' failed to compile; drawing with the standard shader instead", custom->GetPath());
				ID3D12PipelineState* fallback = GetMeshPipeline(skinned, blend, doubleSided, shadow, nullptr);
				mMeshPipelines[key] = fallback;
				return fallback;
			}
		}
		else if (!shadow)
		{
			vs = CompileShader("Mesh.hlsl", "VSMain", ShaderStage::Vertex, defines);
			ps = CompileShader("Mesh.hlsl", "PSMain", ShaderStage::Pixel, defines);
		}
		PipelineBuilder builder;
		builder.SetRootSignature(mMeshRS.Get())
			.SetInputLayout(skinned ? std::span<const D3D12_INPUT_ELEMENT_DESC>(kSkinnedLayout) : std::span<const D3D12_INPUT_ELEMENT_DESC>(kStaticLayout))
			.SetCull(doubleSided ? CullMode::None : CullMode::Back);

		if (shadow)
		{
			builder.SetVS(CompileShader("Shadow.hlsl", "VSMain", ShaderStage::Vertex, defines))
				.SetDepthBias(1000, 1.5f, 0.01f)
				.SetRenderTargets({}, DepthTexture::kDsvFormat);
			builder.Desc().NumRenderTargets = 0;
		}
		else
		{
			builder.SetVS(vs)
				.SetPS(ps)
				.SetBlend(blend)
				.SetDepth(blend == BlendMode::Opaque ? DepthMode::ReadWrite : DepthMode::ReadOnly)
				.SetWireframe(wire)
				.SetRenderTargets({ kHdrFormat }, DepthTexture::kDsvFormat);
		}
		auto pso = builder.Build(GraphicsDevice::Get().GetDevice(), L"MeshPSO");
		mMeshPipelines[key] = pso;
		return pso.Get();
	}
}
