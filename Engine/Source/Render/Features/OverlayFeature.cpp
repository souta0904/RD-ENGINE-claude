#include "RdPch.h"
#include "Render/Features/OverlayFeature.h"
#include "Asset/AssetManager.h"
#include "Math/Matrix3.h"
#include "Render/Renderer/FrameContext.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Pipeline/PipelineBuilder.h"

namespace rd
{
	using namespace render;

	void OverlayFeature::CreatePipelines(const RenderShared& shared)
	{
		auto device = GraphicsDevice::Get().GetDevice();
		PipelineBuilder line;
		line.SetRootSignature(shared.GetSimpleRS())
			.SetVS(CompileShader("Simple.hlsl", "LineVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "LinePS", ShaderStage::Pixel))
			.SetInputLayout(kLineLayout)
			.SetTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetRenderTargets({ kOutputRtvFormat }, DepthTexture::kDsvFormat);
		mLineDepthPSO = line.SetDepth(DepthMode::ReadOnly).Build(device, L"LineDepthPSO");
		mLineOverlayPSO = line.SetDepth(DepthMode::Disabled).Build(device, L"LineOverlayPSO");

		// デバッグ描画の面。線と同じシェーダと頂点で、三角形として描く
		PipelineBuilder tri;
		tri.SetRootSignature(shared.GetSimpleRS())
			.SetVS(CompileShader("Simple.hlsl", "LineVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "LinePS", ShaderStage::Pixel))
			.SetInputLayout(kLineLayout)
			.SetTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetRenderTargets({ kOutputRtvFormat }, DepthTexture::kDsvFormat);
		mTriDepthPSO = tri.SetDepth(DepthMode::ReadOnly).Build(device, L"DebugTriDepthPSO");
		mTriOverlayPSO = tri.SetDepth(DepthMode::Disabled).Build(device, L"DebugTriOverlayPSO");

		mSpritePSO = PipelineBuilder()
			.SetRootSignature(shared.GetSimpleRS())
			.SetVS(CompileShader("Simple.hlsl", "SpriteVS", ShaderStage::Vertex))
			.SetPS(CompileShader("Simple.hlsl", "SpritePS", ShaderStage::Pixel))
			.SetInputLayout(kSpriteLayout)
			.SetBlend(BlendMode::Alpha)
			.SetCull(CullMode::None)
			.SetDepth(DepthMode::Disabled)
			.SetRenderTargets({ kOutputRtvFormat }, DepthTexture::kDsvFormat)
			.Build(device, L"SpritePSO");
	}

	void OverlayFeature::AddPasses(rg::RenderGraph& graph, FrameContext& frame)
	{
		const FrameTextures t = frame.textures;
		graph.AddPass("Overlay", [&](rg::PassBuilder& b) { b.Write(t.output); b.ReadDepth(t.depth); },
			[this, t, &frame](rg::PassContext& c)
			{
				RenderTexture& output = c.Color(t.output);
				auto rtv = output.GetRtv();
				auto dsv = c.Depth(t.depth).GetDsv();
				c.cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
				SetViewport(c.cmd, output.GetWidth(), output.GetHeight());
				c.cmd->SetGraphicsRootSignature(frame.shared.GetSimpleRS());
				// 面を先に描き、その上に線を重ねる（面の縁取りの線が面に埋もれないように）
				constexpr auto kTris = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				constexpr auto kLines = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
				DrawDebugVertices(c.cmd, frame.debug.GetDepthTestedTriangles(), mTriDepthPSO.Get(), kTris, frame);
				DrawDebugVertices(c.cmd, frame.debug.GetDepthTested(), mLineDepthPSO.Get(), kLines, frame);
				DrawDebugVertices(c.cmd, frame.debug.GetOverlayTriangles(), mTriOverlayPSO.Get(), kTris, frame);
				DrawDebugVertices(c.cmd, frame.debug.GetOverlay(), mLineOverlayPSO.Get(), kLines, frame);
				DrawSprites(c.cmd, output.GetWidth(), output.GetHeight(), frame);
			});
	}

	void OverlayFeature::DrawDebugVertices(ID3D12GraphicsCommandList* cmd, const std::vector<DebugDraw::Vertex>& vertices, ID3D12PipelineState* pso,
		D3D12_PRIMITIVE_TOPOLOGY topology, FrameContext& frame)
	{
		if (vertices.empty() || !pso) { return; }
		auto& device = GraphicsDevice::Get();
		uint64_t size = sizeof(DebugDraw::Vertex) * vertices.size();
		D3D12_VERTEX_BUFFER_VIEW vbv = { device.UploadData(vertices.data(), size), UINT(size), sizeof(DebugDraw::Vertex) };
		SimpleConstants c;
		c.matrix = frame.data.viewProj;
		cmd->SetPipelineState(pso);
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->IASetPrimitiveTopology(topology);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		cmd->DrawInstanced(UINT(vertices.size()), 1, 0, 0);
		++frame.stats.drawCalls;
	}

	void OverlayFeature::DrawSprites(ID3D12GraphicsCommandList* cmd, uint32_t width, uint32_t height, FrameContext& frame)
	{
		auto& sprites = frame.list.sprites;
		if (sprites.empty() || !mSpritePSO) { return; }
		auto& device = GraphicsDevice::Get();
		auto& assets = AssetManager::Get();
		std::stable_sort(sprites.begin(), sprites.end(), [](auto& a, auto& b) { return a.layer < b.layer; });

		std::vector<SpriteVertex> vertices;
		vertices.reserve(sprites.size() * 6);
		for (const auto& s : sprites)
		{
			Matrix3 m = Matrix3::CreateAffine(s.size, s.rotation, s.position);
			Vector2 corners[4] = { { -s.pivot.x, -s.pivot.y }, { 1.0f - s.pivot.x, -s.pivot.y }, { -s.pivot.x, 1.0f - s.pivot.y }, { 1.0f - s.pivot.x, 1.0f - s.pivot.y } };
			Vector2 uvs[4] = { { s.uvRect.x, s.uvRect.y }, { s.uvRect.z, s.uvRect.y }, { s.uvRect.x, s.uvRect.w }, { s.uvRect.z, s.uvRect.w } };
			SpriteVertex v[4];
			for (int i = 0; i < 4; ++i) { v[i] = { corners[i] * m, uvs[i], s.color }; }
			vertices.insert(vertices.end(), { v[0], v[1], v[2], v[2], v[1], v[3] });
		}
		uint64_t size = sizeof(SpriteVertex) * vertices.size();
		D3D12_VERTEX_BUFFER_VIEW vbv = { device.UploadData(vertices.data(), size), UINT(size), sizeof(SpriteVertex) };

		SimpleConstants c;
		c.matrix = Matrix4::CreateOrthographic(0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f);
		cmd->SetPipelineState(mSpritePSO.Get());
		cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(c) / 4, &c, 0);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->IASetVertexBuffers(0, 1, &vbv);

		size_t start = 0;
		while (start < sprites.size())
		{
			const Texture* tex = sprites[start].texture;
			size_t end = start + 1;
			while (end < sprites.size() && sprites[end].texture == tex) { ++end; }
			cmd->SetGraphicsRootDescriptorTable(kSimpleTex0, TextureSrv(tex, assets.GetWhiteTexture()));
			cmd->DrawInstanced(UINT((end - start) * 6), 1, UINT(start * 6), 0);
			++frame.stats.drawCalls;
			start = end;
		}
		frame.stats.sprites = uint32_t(sprites.size());
	}
}
