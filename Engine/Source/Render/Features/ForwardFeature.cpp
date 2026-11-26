#include "RdPch.h"
#include "Render/Features/ForwardFeature.h"
#include "Render/Features/MeshDrawer.h"
#include "Render/Features/TransparentDrawer.h"
#include "Render/Renderer/FrameContext.h"

namespace rd
{
	void ForwardFeature::AddPasses(rg::RenderGraph& graph, FrameContext& frame)
	{
		const FrameTextures t = frame.textures;
		graph.AddPass("Forward", [&](rg::PassBuilder& b) { b.WriteNew(t.hdr); b.WriteDepth(t.depth, true); b.Read(t.shadow); },
			[this, t, &frame](rg::PassContext& c)
			{
				RenderTexture& target = c.Color(t.hdr);
				DepthTexture& depthTex = c.Depth(t.depth);
				auto rtv = target.GetRtv();
				auto dsv = depthTex.GetDsv();
				c.cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
				// 空を描かないときに見える色（空の地平線の色）で消す
				Color clear(frame.data.skyHorizon.x, frame.data.skyHorizon.y, frame.data.skyHorizon.z, 1.0f);
				c.cmd->ClearRenderTargetView(rtv, clear.Data(), 0, nullptr);
				depthTex.Clear(c.cmd);
				frame.BindMeshPass(c.cmd, c.Texture(t.shadow));
				mMeshes.Draw(c.cmd, frame.opaque, MeshDrawer::Pass::Forward, frame);
				mTransparent.Draw(c.cmd, frame);
			});
	}
}
