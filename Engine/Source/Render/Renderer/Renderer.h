#pragma once
#include "Render/Camera/RenderCamera.h"
#include "Render/Debug/DebugDraw.h"
#include "Render/Material/Material.h"
#include "Render/RenderList/RenderList.h"
#include "Render/Settings/Environment.h"
#include "Render/Settings/RenderSettings.h"
#include "Render/Settings/RenderStats.h"
#include "Math/Geometry/Frustum.h"
#include "RHI/Texture/DepthTexture.h"
#include "RHI/Texture/RenderTexture.h"
#include <array>
#include <unordered_map>

namespace rd
{
	// 1 つの画面を描く先（HDR・深度・結果・ブルーム）。大きさは描くときに合わせる
	//  メインの画面のほか、エディタのプレビューなどが自分の分を持って RenderOffscreen に渡す
	struct RenderTargets
	{
		static constexpr uint32_t kBloomLevels = 5;
		RenderTexture hdr;
		DepthTexture depth;
		RenderTexture output;
		RenderTexture bloom[kBloomLevels];
		// 縁取りで「物そのものの形」を覚える印（ステンシルの代わり）
		RenderTexture outlineMask;
	};

	// フォワードレンダラー
	//  シャドウ → 不透明 → 空 → 半透明 / パーティクル → ブルーム → トーンマップ → デバッグ線 / スプライト
	//  結果は GetOutput()（sRGB にした LDR）に入る。エディタは ImGui でこれを表示し、
	//  ゲームでは BlitToBackBuffer でバックバッファへ写す。
	//  実装は Renderer.cpp（流れ）/ RendererPipelines.cpp / RendererShadow.cpp / RendererPostProcess.cpp に分かれている
	class Renderer
	{
	public:
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
		void RequestCapture(const std::string& path) { mCapturePath = path; }
		void FinishCapture();

		const GpuTexture& GetOutput() const { return mMain.output; }
		uint32_t GetOutputWidth() const { return mMain.output.GetWidth(); }
		uint32_t GetOutputHeight() const { return mMain.output.GetHeight(); }

	private:
		using CasterLists = std::array<std::vector<const MeshDrawItem*>, kMaxShadowCascades>;

		void CreateRootSignatures();
		void CreatePipelines();
		ID3D12PipelineState* GetMeshPipeline(bool skinned, BlendMode blend, bool doubleSided, bool shadow);
		void EnsureTargets(uint32_t width, uint32_t height);

		void SetupFrameData(const RenderCamera& camera, const Environment& env, uint32_t width, uint32_t height, float time);
		GpuLightData SetupLights();
		void CullAndSort(const RenderCamera& camera, std::vector<const MeshDrawItem*>& opaque,
			std::vector<const MeshDrawItem*>& transparent, CasterLists& casters);
		void MainPass(ID3D12GraphicsCommandList* cmd, const Environment& env, uint32_t width, uint32_t height,
			const std::vector<const MeshDrawItem*>& opaque, const std::vector<const MeshDrawItem*>& transparent);
		void CopyOutputForCapture(ID3D12GraphicsCommandList* cmd);

		void SetupShadowCascades(const RenderCamera& camera, const Vector3& lightDir, GpuFrameData& f);
		void ShadowPass(ID3D12GraphicsCommandList* cmd, const CasterLists& casters);
		void DrawMeshes(ID3D12GraphicsCommandList* cmd, const std::vector<const MeshDrawItem*>& items, bool shadow);
		// Lights Per Object が 0 より大きいとき、物に効く追加のライトを選ぶ
		void SelectObjectLights(const AABB& bounds, GpuObjectData& obj) const;
		void DrawParticles(ID3D12GraphicsCommandList* cmd);
		void DrawTranslucentTriangles(ID3D12GraphicsCommandList* cmd);
		void OutlinePass(ID3D12GraphicsCommandList* cmd, uint32_t width, uint32_t height);
		void BloomPass(ID3D12GraphicsCommandList* cmd);
		void TonemapPass(ID3D12GraphicsCommandList* cmd);
		void OverlayPass(ID3D12GraphicsCommandList* cmd, const Matrix4& viewProj);
		void DrawLines(ID3D12GraphicsCommandList* cmd, const std::vector<DebugDraw::Vertex>& lines, ID3D12PipelineState* pso, const Matrix4& viewProj);
		void DrawSprites(ID3D12GraphicsCommandList* cmd);
		void DrawFullscreen(ID3D12GraphicsCommandList* cmd, ID3D12PipelineState* pso, const Vector4& params,
			D3D12_GPU_DESCRIPTOR_HANDLE t0, D3D12_GPU_DESCRIPTOR_HANDLE t1 = {});
		void RenderToBloom(ID3D12GraphicsCommandList* cmd, RenderTexture& target);

		static inline Renderer* sInstance = nullptr;

		RenderList mList;
		DebugDraw mDebug;
		RenderSettings mSettings;
		RenderStats mStats;

		// mMeshRS はメッシュ / 空 / パーティクル、mSimpleRS は 2D / ポストエフェクト
		ComPtr<ID3D12RootSignature> mMeshRS;
		ComPtr<ID3D12RootSignature> mSimpleRS;

		std::unordered_map<uint32_t, ComPtr<ID3D12PipelineState>> mMeshPipelines;
		ComPtr<ID3D12PipelineState> mSkyPSO;
		ComPtr<ID3D12PipelineState> mParticleAlphaPSO;
		ComPtr<ID3D12PipelineState> mParticleAddPSO;
		ComPtr<ID3D12PipelineState> mTranslucentTrianglePSO;
		// 縁取り（[0] は静的、[1] はスキン）
		ComPtr<ID3D12PipelineState> mOutlineMaskPSO[2];
		ComPtr<ID3D12PipelineState> mOutlinePSO[2];
		ComPtr<ID3D12PipelineState> mLineDepthPSO;
		ComPtr<ID3D12PipelineState> mLineOverlayPSO;
		ComPtr<ID3D12PipelineState> mSpritePSO;
		ComPtr<ID3D12PipelineState> mBloomPrefilterPSO;
		ComPtr<ID3D12PipelineState> mBloomDownPSO;
		ComPtr<ID3D12PipelineState> mBloomUpPSO;
		ComPtr<ID3D12PipelineState> mTonemapPSO;
		ComPtr<ID3D12PipelineState> mBlitPSO;

		// mT は今描いている先（ふだんは mMain、RenderOffscreen の間だけ差し替わる）
		RenderTargets mMain;
		RenderTargets* mT = &mMain;
		DepthTexture mShadowMap;

		D3D12_GPU_VIRTUAL_ADDRESS mFrameCB = 0;
		D3D12_GPU_VIRTUAL_ADDRESS mLightCB = 0;
		GpuFrameData mFrameData;
		// 今描いている画面の環境（トーンマップで色の調整とレトロ表示に使う）
		Environment mEnv;
		bool mShadowActive = false;
		Frustum mCascadeFrustums[kMaxShadowCascades];

		std::string mCapturePath;
		std::string mPendingCapturePath;
		ComPtr<ID3D12Resource> mCaptureBuffer;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT mCaptureFootprint = {};
	};
}
