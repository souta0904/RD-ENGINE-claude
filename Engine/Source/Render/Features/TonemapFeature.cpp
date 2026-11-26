#include "RdPch.h"
#include "Render/Features/TonemapFeature.h"
#include "Render/Renderer/FrameContext.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Renderer/RendererCommon.h"

namespace rd
{
	using namespace render;

	void TonemapFeature::CreatePipelines(const RenderShared& shared)
	{
		mPSO = shared.BuildPostPipeline("TonemapPS", kOutputRtvFormat, BlendMode::Opaque, L"TonemapPSO");
	}

	void TonemapFeature::AddPasses(rg::RenderGraph& graph, FrameContext& frame)
	{
		const FrameTextures t = frame.textures;
		graph.AddPass("Tonemap", [&](rg::PassBuilder& b) { b.Read(t.hdr); if (t.bloom.IsValid()) { b.Read(t.bloom); } b.WriteNew(t.output); },
			[this, t, &frame](rg::PassContext& c)
			{
				RenderTexture& output = c.Color(t.output);
				const GpuTexture& hdr = c.Texture(t.hdr);
				const GpuTexture* bloom = t.bloom.IsValid() ? &c.Texture(t.bloom) : nullptr;
				const RenderSettings& s = frame.settings;
				const Environment& env = frame.env;
				c.cmd->SetGraphicsRootSignature(frame.shared.GetSimpleRS());
				auto rtv = output.GetRtv();
				c.cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
				SetViewport(c.cmd, output.GetWidth(), output.GetHeight());
				if (!mPSO) { return; }
				// トーンマップの方式は 0 = ACES, 1 = Neutral, 2 = なし（Render Settings で切ってあればなし）
				float tonemapper = s.tonemap ? float(int(env.tonemapper)) : 2.0f;
				SimpleConstants k;
				k.params = Vector4(s.exposure, bloom ? s.bloomIntensity : 0.0f, s.vignette, tonemapper);
				// 行列の場所に色の調整とレトロ表示の値を入れる（シェーダーでは gExtra[0], gExtra[1]）
				float* extra = k.matrix.Data();
				extra[0] = std::exp2(env.postExposure);
				extra[1] = 1.0f + env.contrast / 100.0f;
				extra[2] = 1.0f + env.saturation / 100.0f;
				extra[3] = 0.0f;
				extra[4] = float(env.retroHeight);
				extra[5] = float(std::max(2u, env.retroColorLevels));
				extra[6] = 0.0f;
				extra[7] = 0.0f;
				// Unity の Vignette（gExtra[2] = 色と縦横比、gExtra[3] = 中心・強さ×3・なめらかさ×5。URP と同じ値の渡し方）
				float aspect = float(output.GetWidth()) / float(std::max(output.GetHeight(), 1u));
				extra[8] = env.vignetteColor.r;
				extra[9] = env.vignetteColor.g;
				extra[10] = env.vignetteColor.b;
				extra[11] = env.vignetteRounded ? aspect : 1.0f;
				extra[12] = env.vignetteCenter.x;
				extra[13] = env.vignetteCenter.y;
				extra[14] = env.vignetteIntensity * 3.0f;
				extra[15] = env.vignetteSmoothness * 5.0f;
				c.cmd->SetPipelineState(mPSO.Get());
				c.cmd->SetGraphicsRoot32BitConstants(kSimpleConstants, sizeof(k) / 4, &k, 0);
				c.cmd->SetGraphicsRootDescriptorTable(kSimpleTex0, hdr.GetGpuSrv());
				// ブルームがないときはブルームの代わりに HDR を渡し、強さ 0 で使わない
				c.cmd->SetGraphicsRootDescriptorTable(kSimpleTex1, bloom ? bloom->GetGpuSrv() : hdr.GetGpuSrv());
				c.cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				c.cmd->DrawInstanced(3, 1, 0, 0);
				++frame.stats.drawCalls;
			});
	}
}
