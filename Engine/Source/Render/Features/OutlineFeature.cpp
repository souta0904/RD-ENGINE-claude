#include "RdPch.h"
#include "Render/Features/OutlineFeature.h"
#include "Asset/Model/Model.h"
#include "Render/Renderer/FrameContext.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Pipeline/PipelineBuilder.h"

namespace rd
{
	using namespace render;

	// 形を印に塗る → 広げた形を HDR に描く（どちらも深度は見ない）
	void OutlineFeature::CreatePipelines(const RenderShared& shared)
	{
		auto device = GraphicsDevice::Get().GetDevice();
		for (int skinned = 0; skinned < 2; ++skinned)
		{
			std::vector<std::string> defines;
			if (skinned) { defines.push_back("SKINNED=1"); }
			mMaskPSO[skinned] = PipelineBuilder()
				.SetRootSignature(shared.GetMeshRS())
				.SetVS(CompileShader("Outline.hlsl", "MaskVS", ShaderStage::Vertex, defines))
				.SetPS(CompileShader("Outline.hlsl", "MaskPS", ShaderStage::Pixel, defines))
				.SetInputLayout(MeshLayout(skinned != 0))
				.SetCull(CullMode::None)
				.SetDepth(DepthMode::Disabled)
				.SetRenderTargets({ kOutlineMaskFormat }, DXGI_FORMAT_UNKNOWN)
				.Build(device, L"OutlineMaskPSO");
			mOutlinePSO[skinned] = PipelineBuilder()
				.SetRootSignature(shared.GetMeshRS())
				.SetVS(CompileShader("Outline.hlsl", "OutlineVS", ShaderStage::Vertex, defines))
				.SetPS(CompileShader("Outline.hlsl", "OutlinePS", ShaderStage::Pixel, defines))
				.SetInputLayout(MeshLayout(skinned != 0))
				.SetBlend(BlendMode::Alpha)
				.SetCull(CullMode::None)
				.SetDepth(DepthMode::Disabled)
				.SetRenderTargets({ kHdrFormat }, DXGI_FORMAT_UNKNOWN)
				.Build(device, L"OutlinePSO");
		}
	}

	void OutlineFeature::AddPasses(rg::RenderGraph& graph, FrameContext& frame)
	{
		if (frame.list.outlines.empty() || !mMaskPSO[0] || !mOutlinePSO[0]) { return; }
		const FrameTextures t = frame.textures;
		rg::TextureHandle mask = graph.Create("Outline Mask", { frame.width, frame.height, kOutlineMaskFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, Color(0, 0, 0, 0) });
		graph.AddPass("Outline Mask", [&](rg::PassBuilder& b) { b.WriteNew(mask); b.Read(t.shadow); },
			[this, mask, t, &frame](rg::PassContext& c)
			{
				RenderTexture& m = c.Color(mask);
				auto rtv = m.GetRtv();
				c.cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
				m.Clear(c.cmd);
				Draw(c.cmd, true, nullptr, c.Texture(t.shadow), frame);
			});
		graph.AddPass("Outline", [&](rg::PassBuilder& b) { b.Read(mask); b.Write(t.hdr); b.Read(t.shadow); },
			[this, mask, t, &frame](rg::PassContext& c)
			{
				auto rtv = c.Color(t.hdr).GetRtv();
				c.cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
				Draw(c.cmd, false, &c.Texture(mask), c.Texture(t.shadow), frame);
			});
	}

	void OutlineFeature::Draw(ID3D12GraphicsCommandList* cmd, bool mask, const GpuTexture* maskTexture, const GpuTexture& shadowMap, FrameContext& frame)
	{
		auto& device = GraphicsDevice::Get();
		frame.BindMeshPass(cmd, shadowMap);
		if (maskTexture) { cmd->SetGraphicsRootDescriptorTable(kRootAlbedo, maskTexture->GetGpuSrv()); }
		ComPtr<ID3D12PipelineState>(&psos)[2] = mask ? mMaskPSO : mOutlinePSO;
		for (const OutlineDrawItem& item : frame.list.outlines)
		{
			if (!item.model || !item.model->IsReady()) { continue; }
			const auto& subs = item.model->GetSubMeshes();
			if (item.subMesh >= subs.size()) { continue; }
			const SubMesh& sub = subs[item.subMesh];
			bool skinned = sub.skinned && item.palette != 0;
			GpuObjectData obj;
			obj.world = item.world;
			obj.worldInvTranspose = Transpose(Inverse(item.world));
			obj.tint = item.color;
			GpuMaterialData mat;
			mat.metallic = item.width;
			mat.roughness = item.blinkSec;
			cmd->SetPipelineState(psos[skinned ? 1 : 0].Get());
			cmd->SetGraphicsRootConstantBufferView(kRootObject, device.UploadConstants(obj));
			cmd->SetGraphicsRootConstantBufferView(kRootMaterial, device.UploadConstants(mat));
			if (skinned) { cmd->SetGraphicsRootShaderResourceView(kRootPalette, item.palette); }
			D3D12_VERTEX_BUFFER_VIEW vbs[2] = { item.model->GetVertexView(), item.model->GetSkinView() };
			cmd->IASetVertexBuffers(0, skinned ? 2 : 1, vbs);
			cmd->IASetIndexBuffer(&item.model->GetIndexView());
			cmd->DrawIndexedInstanced(sub.indexCount, 1, sub.indexStart, 0, 0);
			++frame.stats.drawCalls;
		}
	}
}
