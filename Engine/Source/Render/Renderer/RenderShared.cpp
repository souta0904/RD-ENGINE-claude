#include "RdPch.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Material/ShaderAsset.h"
#include "Render/Renderer/RendererCommon.h"
#include "Render/Settings/RenderStats.h"
#include "RHI/Pipeline/PipelineBuilder.h"
#include "RHI/Pipeline/RootSignatureBuilder.h"
#include "RHI/Pipeline/Samplers.h"

namespace rd
{
	using namespace render;

	void RenderShared::Create()
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

	ComPtr<ID3D12PipelineState> RenderShared::BuildPostPipeline(const char* ps, DXGI_FORMAT format, BlendMode blend, const wchar_t* name) const
	{
		return PipelineBuilder()
			.SetRootSignature(mSimpleRS.Get())
			.SetVS(CompileShader("Post.hlsl", "FullscreenVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Post.hlsl", ps, ShaderStage::Pixel))
			.SetBlend(blend)
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::Disabled)
			.SetRenderTargets({ format }, DXGI_FORMAT_UNKNOWN)
			.Build(GraphicsDevice::Get().GetDevice(), name);
	}

	void RenderShared::DrawFullscreen(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso, const Vector4& params,
		D3D12_GPU_DESCRIPTOR_HANDLE t0, RenderStats& stats, D3D12_GPU_DESCRIPTOR_HANDLE t1) const
	{
		if (!pso) { return; }
		SimpleConstants c;
		c.params = params;
		cmd->SetPipelineState(pso);
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->SetGraphicsRootDescriptorTable(kSimpleTex0, t0);
		cmd->SetGraphicsRootDescriptorTable(kSimpleTex1, t1.ptr ? t1 : t0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->DrawInstanced(3, 1, 0, 0);
		++stats.drawCalls;
	}
}
