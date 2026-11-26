#include "TestFramework.h"
#include "Render/Graph/RenderGraph.h"

using namespace rd;
using namespace rd::rg;

// レンダーグラフの組み立て（GPU は使わない）: 飛ばすパス、テクスチャの使い回し、状態
namespace
{
	TextureDesc ColorDesc(uint32_t w, uint32_t h, DXGI_FORMAT f = DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		TextureDesc d;
		d.width = w;
		d.height = h;
		d.format = f;
		return d;
	}

	TextureHandle Output(RenderGraph& g) { return g.Import("Output", nullptr, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true); }
}

RD_TEST(RenderGraph, CullsPassesThatDoNotReachTheOutput)
{
	RenderGraph g;
	TextureHandle out = Output(g);
	TextureHandle hdr = g.Create("HDR", ColorDesc(64, 64));
	TextureHandle unused = g.Create("Unused", ColorDesc(64, 64));
	g.AddPass("Main", [&](PassBuilder& b) { b.WriteNew(hdr); }, nullptr);
	g.AddPass("Debug", [&](PassBuilder& b) { b.Read(hdr); b.WriteNew(unused); }, nullptr);
	g.AddPass("Tonemap", [&](PassBuilder& b) { b.Read(hdr); b.WriteNew(out); }, nullptr);
	g.Compile();
	RD_CHECK(!g.IsCulled(0));
	RD_CHECK(g.IsCulled(1));
	RD_CHECK(!g.IsCulled(2));
	// 飛ばしたパスだけが使うテクスチャには実体を作らない
	RD_CHECK_EQ(g.GetPhysical(unused), -1);
	RD_CHECK_EQ(g.GetPhysicalCount(), 1u);
}

RD_TEST(RenderGraph, OverwriteDropsEarlierWritesButDrawOnTopKeepsThem)
{
	RenderGraph g;
	TextureHandle out = Output(g);
	TextureHandle hdr = g.Create("HDR", ColorDesc(64, 64));
	g.AddPass("Lost", [&](PassBuilder& b) { b.WriteNew(hdr); }, nullptr);
	g.AddPass("Main", [&](PassBuilder& b) { b.WriteNew(hdr); }, nullptr);
	g.AddPass("Outline", [&](PassBuilder& b) { b.Write(hdr); }, nullptr);
	g.AddPass("Tonemap", [&](PassBuilder& b) { b.Read(hdr); b.WriteNew(out); }, nullptr);
	g.Compile();
	// Main が全部書き直すので、その前の Lost の結果は誰も見ない
	RD_CHECK(g.IsCulled(0));
	RD_CHECK(!g.IsCulled(1));
	// Outline は上に描くだけなので、前の Main も残る
	RD_CHECK(!g.IsCulled(2));
	RD_CHECK(!g.IsCulled(3));
}

RD_TEST(RenderGraph, SideEffectKeepsAPassWithoutOutput)
{
	RenderGraph g;
	TextureHandle out = Output(g);
	g.AddPass("Tonemap", [&](PassBuilder& b) { b.WriteNew(out); }, nullptr);
	g.AddPass("Capture", [&](PassBuilder& b) { b.CopyFrom(out); b.SideEffect(); }, nullptr);
	g.AddPass("Nothing", [&](PassBuilder& b) { b.Read(out); }, nullptr);
	g.Compile();
	RD_CHECK(!g.IsCulled(0));
	RD_CHECK(!g.IsCulled(1));
	RD_CHECK(g.IsCulled(2));
}

RD_TEST(RenderGraph, ReusesTexturesWhoseLifetimesDoNotOverlap)
{
	RenderGraph g;
	TextureHandle out = Output(g);
	TextureHandle a = g.Create("A", ColorDesc(32, 32));
	TextureHandle b = g.Create("B", ColorDesc(32, 32));
	TextureHandle c = g.Create("C", ColorDesc(32, 32));
	TextureHandle half = g.Create("half", ColorDesc(16, 16));
	g.AddPass("P0", [&](PassBuilder& p) { p.WriteNew(a); }, nullptr);
	g.AddPass("P1", [&](PassBuilder& p) { p.Read(a); p.WriteNew(b); }, nullptr);
	g.AddPass("P2", [&](PassBuilder& p) { p.Read(b); p.WriteNew(c); }, nullptr);
	g.AddPass("P3", [&](PassBuilder& p) { p.Read(c); p.WriteNew(half); }, nullptr);
	g.AddPass("P4", [&](PassBuilder& p) { p.Read(half); p.WriteNew(out); }, nullptr);
	g.Compile();
	// A は P1 で使い終わるので、P2 から使う C が同じ実体を使う。B は A と重なるので別
	RD_CHECK_EQ(g.GetPhysical(a), g.GetPhysical(c));
	RD_CHECK(g.GetPhysical(a) != g.GetPhysical(b));
	// 大きさが違えば使い回さない
	RD_CHECK(g.GetPhysical(half) != g.GetPhysical(a));
	RD_CHECK(g.GetPhysical(half) != g.GetPhysical(b));
	RD_CHECK_EQ(g.GetPhysicalCount(), 3u);
	RD_CHECK_EQ(g.GetPhysical(out), -1);
}

RD_TEST(RenderGraph, DifferentFormatsAndDepthAreNotShared)
{
	RenderGraph g;
	TextureHandle out = Output(g);
	TextureHandle a = g.Create("A", ColorDesc(32, 32, DXGI_FORMAT_R16G16B16A16_FLOAT));
	TextureHandle b = g.Create("B", ColorDesc(32, 32, DXGI_FORMAT_R8G8B8A8_UNORM));
	TextureDesc depthDesc = ColorDesc(32, 32);
	depthDesc.depth = true;
	TextureHandle d = g.Create("Depth", depthDesc);
	g.AddPass("P0", [&](PassBuilder& p) { p.WriteNew(a); }, nullptr);
	g.AddPass("P1", [&](PassBuilder& p) { p.Read(a); p.WriteNew(b); }, nullptr);
	g.AddPass("P2", [&](PassBuilder& p) { p.Read(b); p.WriteDepth(d, true); }, nullptr);
	g.AddPass("P3", [&](PassBuilder& p) { p.Read(d); p.WriteNew(out); }, nullptr);
	g.Compile();
	RD_CHECK(g.GetPhysical(a) != g.GetPhysical(b));
	RD_CHECK(g.GetPhysical(d) != g.GetPhysical(a));
	RD_CHECK_EQ(g.GetPhysicalCount(), 3u);
}

RD_TEST(RenderGraph, PassStatesFollowTheAccess)
{
	RenderGraph g;
	TextureHandle out = Output(g);
	TextureDesc depthDesc;
	depthDesc.depth = true;
	TextureHandle depth = g.Create("Depth", depthDesc);
	TextureHandle hdr = g.Create("HDR", ColorDesc(1, 1));
	g.AddPass("Main", [&](PassBuilder& b) { b.WriteNew(hdr); b.WriteDepth(depth, true); }, nullptr);
	// 深度を比べながらシェーダーでも読む（ディファードのライティングの後の空など）
	g.AddPass("Fog", [&](PassBuilder& b) { b.Write(hdr); b.ReadDepth(depth); b.Read(depth); }, nullptr);
	g.AddPass("Tonemap", [&](PassBuilder& b) { b.Read(hdr); b.WriteNew(out); }, nullptr);
	g.Compile();

	auto states = g.GetPassStates(1);
	RD_REQUIRE(states.size() == 2);
	RD_CHECK(states[0].first == hdr);
	RD_CHECK_EQ(uint32_t(states[0].second), uint32_t(D3D12_RESOURCE_STATE_RENDER_TARGET));
	RD_CHECK(states[1].first == depth);
	RD_CHECK_EQ(uint32_t(states[1].second), uint32_t(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));

	GraphInfo info = g.GetInfo();
	RD_CHECK_EQ(info.passes.size(), size_t(3));
	RD_CHECK_EQ(info.resources.size(), size_t(3));
	RD_CHECK(info.resources[0].output);
	RD_CHECK_EQ(info.passes[1].uses.size(), size_t(3));
}

RD_TEST(RenderGraph, ResetClearsPassesForTheNextFrame)
{
	RenderGraph g;
	TextureHandle out = Output(g);
	g.AddPass("A", [&](PassBuilder& b) { b.WriteNew(out); }, nullptr);
	g.Compile();
	RD_CHECK_EQ(g.GetPassCount(), 1u);
	g.Reset();
	RD_CHECK_EQ(g.GetPassCount(), 0u);
	RD_CHECK_EQ(g.GetInfo().resources.size(), size_t(0));
}
