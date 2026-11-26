#pragma once
#include "Render/Camera/RenderCamera.h"
#include "Render/Debug/DebugDraw.h"
#include "Render/GpuData.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Renderer/RenderShared.h"
#include "Render/RenderList/RenderList.h"
#include "Render/Settings/Environment.h"
#include "Render/Settings/RenderSettings.h"
#include "Render/Settings/RenderStats.h"
#include "RHI/Texture/RenderTexture.h"
#include <memory>
#include <vector>

namespace rd
{
	class BloomFeature;
	class DeferredFeature;
	class ForwardFeature;
	class MeshDrawer;
	class OutlineFeature;
	class OverlayFeature;
	class RenderFeature;
	class ScreenshotFeature;
	class ShadowFeature;
	class TonemapFeature;
	class TransparentDrawer;
	struct FrameContext;

	// 1 つの画面の結果（sRGB にした LDR）。大きさは描くときに合わせる
	//  メインの画面のほか、エディタのプレビューなどが自分の分を持って RenderOffscreen に渡す
	//  途中で使うテクスチャ（HDR・深度・ブルーム・G バッファーなど）はレンダーグラフが持って使い回す
	struct RD_API RenderTargets
	{
		RenderTexture output;
	};

	// レンダラー（フォワードとディファードを Render Settings の Render Path で切り替える）
	//  描くこと自体は描画の機能（Render/Features の RenderFeature）が行う。Renderer の仕事は
	//   1. フレームの値（カメラ・ライト・影の行列）を作って GPU へ送る
	//   2. カメラに映るメッシュを集めて並べる
	//   3. 機能を決まった順に呼んでパスをレンダーグラフに登録させ、まとめて実行する
	//  機能の順: 影 → 不透明（フォワード または ディファード）→ 縁取り → ブルーム → トーンマップ → 重ねるもの → スクリーンショット
	//  結果は GetOutput()（sRGB にした LDR）に入る。エディタは ImGui でこれを表示し、
	//  ゲームでは BlitToBackBuffer でバックバッファへ写す。
	class RD_API Renderer
	{
	public:
		Renderer();
		~Renderer();
		static Renderer& Get() { return *sInstance; }

		void Initialize();
		void Shutdown();
		void ReloadShaders();

		RenderList& GetRenderList() { return mList; }
		DebugDraw& GetDebugDraw() { return mDebug; }
		RenderSettings& GetSettings() { return mSettings; }
		const RenderStats& GetStats() const { return mStats; }

		void Render(const RenderCamera& camera, const Environment& env, uint32_t width, uint32_t height, float time);
		void BlitToBackBuffer();

		// メインの画面とは別に、list の中身だけを targets へ描く（統計や描画リストはメインの分を残す）
		//  描き終えた結果は targets.output にあり、そのままシェーダーから読める。list と debug は空になる
		//  debug を渡すとその線も重ねる
		void RenderOffscreen(RenderTargets& targets, RenderList& list, const RenderCamera& camera, const Environment& env,
			uint32_t width, uint32_t height, float time = 0.0f, DebugDraw* debug = nullptr);

		// 次の Render の結果を PNG で保存する（FinishCapture で書き出す）
		void RequestCapture(const std::string& path);
		void FinishCapture();

		const GpuTexture& GetOutput() const { return mMain.output; }
		uint32_t GetOutputWidth() const { return mMain.output.GetWidth(); }
		uint32_t GetOutputHeight() const { return mMain.output.GetHeight(); }

		// メインの画面を最後に描いたときのレンダーグラフ（エディタで見せる）
		const rg::GraphInfo& GetGraphInfo() const { return mGraphInfo; }

	private:
		void CreatePipelines();
		void EnsureTargets(uint32_t width, uint32_t height);
		void SetupFrameData(FrameContext& frame, float time);
		GpuLightData SetupLights(FrameContext& frame);
		// カメラに映るメッシュを、不透明は手前から、半透明は奥から並べる
		void CullAndSort(FrameContext& frame);

		static inline Renderer* sInstance = nullptr;

		RenderList mList;
		DebugDraw mDebug;
		RenderSettings mSettings;
		RenderStats mStats;
		RenderShared mShared;

		// 描く道具（機能どうしで共有する）と描画の機能
		std::unique_ptr<MeshDrawer> mMeshes;
		std::unique_ptr<TransparentDrawer> mTransparent;
		std::unique_ptr<ShadowFeature> mShadow;
		std::unique_ptr<ForwardFeature> mForward;
		std::unique_ptr<DeferredFeature> mDeferred;
		std::unique_ptr<OutlineFeature> mOutline;
		std::unique_ptr<BloomFeature> mBloom;
		std::unique_ptr<TonemapFeature> mTonemap;
		std::unique_ptr<OverlayFeature> mOverlay;
		std::unique_ptr<ScreenshotFeature> mScreenshot;
		// パイプラインを作り直すときに回す一覧（上の機能を指す）
		std::vector<RenderFeature*> mFeatures;
		ComPtr<ID3D12PipelineState> mBlitPSO;

		// mT は今描いている先（ふだんは mMain、RenderOffscreen の間だけ差し替わる）
		RenderTargets mMain;
		RenderTargets* mT = &mMain;
		rg::RenderGraph mGraph;
		rg::GraphInfo mGraphInfo;
	};
}
