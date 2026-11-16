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

		// 次の Render の結果を PNG で保存する（FinishCapture で書き出す）
		void RequestCapture(const std::string& path) { mCapturePath = path; }
		void FinishCapture();

		const GpuTexture& GetOutput() const { return mOutput; }
		uint32_t GetOutputWidth() const { return mOutput.GetWidth(); }
		uint32_t GetOutputHeight() const { return mOutput.GetHeight(); }

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
		void DrawParticles(ID3D12GraphicsCommandList* cmd);
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
		ComPtr<ID3D12PipelineState> mLineDepthPSO;
		ComPtr<ID3D12PipelineState> mLineOverlayPSO;
		ComPtr<ID3D12PipelineState> mSpritePSO;
		ComPtr<ID3D12PipelineState> mBloomPrefilterPSO;
		ComPtr<ID3D12PipelineState> mBloomDownPSO;
		ComPtr<ID3D12PipelineState> mBloomUpPSO;
		ComPtr<ID3D12PipelineState> mTonemapPSO;
		ComPtr<ID3D12PipelineState> mBlitPSO;

		RenderTexture mHdr;
		DepthTexture mDepth;
		RenderTexture mOutput;
		DepthTexture mShadowMap;
		static constexpr uint32_t kBloomLevels = 5;
		RenderTexture mBloom[kBloomLevels];

		D3D12_GPU_VIRTUAL_ADDRESS mFrameCB = 0;
		D3D12_GPU_VIRTUAL_ADDRESS mLightCB = 0;
		GpuFrameData mFrameData;
		bool mShadowActive = false;
		Frustum mCascadeFrustums[kMaxShadowCascades];

		std::string mCapturePath;
		std::string mPendingCapturePath;
		ComPtr<ID3D12Resource> mCaptureBuffer;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT mCaptureFootprint = {};
	};
}
