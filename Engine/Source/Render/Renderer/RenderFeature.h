#pragma once

namespace rd
{
	class RenderShared;
	struct FrameContext;
	namespace rg { class RenderGraph; }

	// 描画の機能 1 つ（Unity の URP の Renderer Feature にあたる）
	//  自分のパイプラインを持ち、毎フレーム自分のパスをレンダーグラフに登録する。
	//  機能どうしは FrameContext::textures でテクスチャを受け渡す（例: 影 → 不透明 → ブルーム → トーンマップ）。
	//  Renderer は機能を決まった順に並べて呼ぶだけで、パスの中身は知らない。
	class RenderFeature
	{
	public:
		virtual ~RenderFeature() = default;
		// パイプラインを作る（起動時と、シェーダーを読み直したとき）
		virtual void CreatePipelines(const RenderShared& shared) = 0;
		// パスをグラフに登録する（実行は Renderer がまとめて行う）
		virtual void AddPasses(rg::RenderGraph& graph, FrameContext& frame) = 0;
	};
}
