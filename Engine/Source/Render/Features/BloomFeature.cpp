#include "RdPch.h"
#include "Render/Features/BloomFeature.h"
#include "Render/Renderer/FrameContext.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Texture/RenderTexture.h"
#include <array>

namespace rd
{
	using namespace render;

	void BloomFeature::CreatePipelines(const RenderShared& shared)
	{
		mPrefilterPSO = shared.BuildPostPipeline("BloomPrefilterPS", kBloomFormat, BlendMode::Opaque, L"BloomPrefilterPSO");
		mDownPSO = shared.BuildPostPipeline("BloomDownPS", kBloomFormat, BlendMode::Opaque, L"BloomDownPSO");
		mUpPSO = shared.BuildPostPipeline("BloomUpPS", kBloomFormat, BlendMode::Additive, L"BloomUpPSO");
	}

	void BloomFeature::SetTarget(ID3D12GraphicsCommandList* cmd, RenderTexture& target)
	{
		auto rtv = target.GetRtv();
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		SetViewport(cmd, target.GetWidth(), target.GetHeight());
	}

	void BloomFeature::AddPasses(rg::RenderGraph& graph, FrameContext& frame)
	{
		if (!frame.settings.bloom) { return; }
		static const char* kDown[kLevels] = { "Bloom Prefilter", "Bloom Down 1", "Bloom Down 2", "Bloom Down 3", "Bloom Down 4" };
		static const char* kUp[kLevels] = { "", "Bloom Up 0", "Bloom Up 1", "Bloom Up 2", "Bloom Up 3" };
		static const char* kTextures[kLevels] = { "Bloom 0", "Bloom 1", "Bloom 2", "Bloom 3", "Bloom 4" };
		const rg::TextureHandle hdr = frame.textures.hdr;
		std::array<rg::TextureHandle, kLevels> bloom;
		uint32_t w = frame.width, h = frame.height;
		for (uint32_t i = 0; i < kLevels; ++i)
		{
			w = std::max(1u, w / 2);
			h = std::max(1u, h / 2);
			bloom[i] = graph.Create(kTextures[i], { w, h, kBloomFormat, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false, Color(0, 0, 0, 1) });
		}
		graph.AddPass(kDown[0], [&](rg::PassBuilder& b) { b.Read(hdr); b.WriteNew(bloom[0]); },
			[this, hdr, dst = bloom[0], &frame](rg::PassContext& c)
			{
				c.cmd->SetGraphicsRootSignature(frame.shared.GetSimpleRS());
				SetTarget(c.cmd, c.Color(dst));
				frame.shared.DrawFullscreen(c.cmd, mPrefilterPSO.Get(), Vector4(frame.settings.bloomThreshold, frame.settings.bloomKnee, 0, 0),
					c.Texture(hdr).GetGpuSrv(), frame.stats);
			});
		for (uint32_t i = 1; i < kLevels; ++i)
		{
			graph.AddPass(kDown[i], [&](rg::PassBuilder& b) { b.Read(bloom[i - 1]); b.WriteNew(bloom[i]); },
				[this, src = bloom[i - 1], dst = bloom[i], &frame](rg::PassContext& c)
				{
					c.cmd->SetGraphicsRootSignature(frame.shared.GetSimpleRS());
					SetTarget(c.cmd, c.Color(dst));
					frame.shared.DrawFullscreen(c.cmd, mDownPSO.Get(), Vector4(), c.Texture(src).GetGpuSrv(), frame.stats);
				});
		}
		for (uint32_t i = kLevels - 1; i > 0; --i)
		{
			graph.AddPass(kUp[i], [&](rg::PassBuilder& b) { b.Read(bloom[i]); b.Write(bloom[i - 1]); },
				[this, src = bloom[i], dst = bloom[i - 1], &frame](rg::PassContext& c)
				{
					c.cmd->SetGraphicsRootSignature(frame.shared.GetSimpleRS());
					SetTarget(c.cmd, c.Color(dst));
					frame.shared.DrawFullscreen(c.cmd, mUpPSO.Get(), Vector4(1.0f, 0, 0, 0), c.Texture(src).GetGpuSrv(), frame.stats);
				});
		}
		frame.textures.bloom = bloom[0];
	}
}
