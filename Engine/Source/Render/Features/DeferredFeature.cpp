#include "RdPch.h"
#include "Render/Features/DeferredFeature.h"
#include "Render/Features/MeshDrawer.h"
#include "Render/Features/TransparentDrawer.h"
#include "Render/Renderer/FrameContext.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Pipeline/PipelineBuilder.h"
#include <array>

namespace rd
{
	using namespace render;

	void DeferredFeature::CreatePipelines(const RenderShared& shared)
	{
		mLightingPSO = PipelineBuilder()
			.SetRootSignature(shared.GetMeshRS())
			.SetVS(CompileShader("DeferredLighting.hlsl", "VSMain", ShaderStage::Vertex))
			.SetPS(CompileShader("DeferredLighting.hlsl", "PSMain", ShaderStage::Pixel))
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::Disabled)
			.SetRenderTargets({ kHdrFormat }, DXGI_FORMAT_UNKNOWN)
			.Build(GraphicsDevice::Get().GetDevice(), L"DeferredLightingPSO");
	}

	void DeferredFeature::AddPasses(rg::RenderGraph& graph, FrameContext& frame)
	{
		const FrameTextures t = frame.textures;
		const char* names[kGBufferCount] = { "GBuffer Albedo", "GBuffer Normal", "GBuffer Emission" };
		std::array<rg::TextureHandle, kGBufferCount> gbuffer;
		for (uint32_t i = 0; i < kGBufferCount; ++i)
		{
			gbuffer[i] = graph.Create(names[i], { frame.width, frame.height, kGBufferFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, Color(0, 0, 0, 0) });
		}

		graph.AddPass("GBuffer", [&](rg::PassBuilder& b) { for (auto h : gbuffer) { b.WriteNew(h); } b.WriteDepth(t.depth, true); b.Read(t.shadow); },
			[this, gbuffer, t, &frame](rg::PassContext& c)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE rtvs[kGBufferCount];
				for (uint32_t i = 0; i < kGBufferCount; ++i)
				{
					RenderTexture& target = c.Color(gbuffer[i]);
					rtvs[i] = target.GetRtv();
					target.Clear(c.cmd);
				}
				DepthTexture& depthTex = c.Depth(t.depth);
				auto dsv = depthTex.GetDsv();
				c.cmd->OMSetRenderTargets(kGBufferCount, rtvs, FALSE, &dsv);
				depthTex.Clear(c.cmd);
				frame.BindMeshPass(c.cmd, c.Texture(t.shadow));
				mMeshes.Draw(c.cmd, frame.opaque, MeshDrawer::Pass::GBuffer, frame);
			});

		graph.AddPass("Deferred Lighting", [&](rg::PassBuilder& b) { for (auto h : gbuffer) { b.Read(h); } b.Read(t.depth); b.Read(t.shadow); b.WriteNew(t.hdr); },
			[this, gbuffer, t, &frame](rg::PassContext& c)
			{
				auto rtv = c.Color(t.hdr).GetRtv();
				c.cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
				frame.BindMeshPass(c.cmd, c.Texture(t.shadow));
				c.cmd->SetGraphicsRootConstantBufferView(kRootObject, GraphicsDevice::Get().UploadConstants(GpuObjectData{}));
				c.cmd->SetGraphicsRootDescriptorTable(kRootAlbedo, c.Texture(gbuffer[0]).GetGpuSrv());
				c.cmd->SetGraphicsRootDescriptorTable(kRootNormal, c.Texture(gbuffer[1]).GetGpuSrv());
				c.cmd->SetGraphicsRootDescriptorTable(kRootEmissive, c.Texture(gbuffer[2]).GetGpuSrv());
				c.cmd->SetGraphicsRootDescriptorTable(kRootShaderTex0, c.Texture(t.depth).GetGpuSrv());
				c.cmd->SetPipelineState(mLightingPSO.Get());
				c.cmd->DrawInstanced(3, 1, 0, 0);
				++frame.stats.drawCalls;
			});

		graph.AddPass("Sky & Transparent", [&](rg::PassBuilder& b) { b.Write(t.hdr); b.ReadDepth(t.depth); b.Read(t.shadow); },
			[this, t, &frame](rg::PassContext& c)
			{
				auto rtv = c.Color(t.hdr).GetRtv();
				auto dsv = c.Depth(t.depth).GetDsv();
				c.cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
				frame.BindMeshPass(c.cmd, c.Texture(t.shadow));
				mTransparent.Draw(c.cmd, frame);
			});
	}
}
