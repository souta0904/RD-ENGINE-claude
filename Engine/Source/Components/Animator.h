#pragma once
#include "Asset/Animation.h"
#include "Asset/Asset.h"
#include "Core/JobSystem.h"
#include "RHI/D3D12Common.h"
#include "Scene/Component.h"
#include <vector>

namespace rd
{
	class Model;

	// スケルタルアニメーションの再生（クロスフェード対応）
	//  姿勢計算はジョブシステムで並列に行われ、描画時に完了を待つ
	class Animator final : public Component
	{
		RD_COMPONENT(Animator)
	public:
		std::string clipSource;// アニメーションを読むファイル（空なら MeshRenderer のモデル）
		std::string clip;// クリップ名（空なら最初のクリップ）
		float speed = 1.0f;
		bool loop = true;
		bool playOnStart = true;
		bool previewInEditor = false;

		~Animator() override { JobSystem::Wait(mJob); }
		void Reflect(PropertyVisitor& v) override;
		void OnValidate() override;
		void OnDestroy() override { JobSystem::Wait(mJob); }
		void Start() override;
		void Update(float dt) override;
		void OnEditorUpdate(float dt) override;
		void OnInspectorGUI() override;

		// クリップを再生（fade 秒かけて現在のクリップから切り替える）
		void Play(const std::string& clipName, float fade = 0.2f, const std::string& source = "");
		void Stop() { mPlaying = false; }
		bool IsPlaying() const { return mPlaying; }
		float GetTime() const { return mTime; }
		float GetNormalizedTime() const;
		std::vector<std::string> GetClipNames() const;

		// 描画用の行列パレット（このフレームの GPU アドレス、未準備なら 0）
		D3D12_GPU_VIRTUAL_ADDRESS GetPaletteGpu(const Model* skinModel);

	private:
		struct Layer
		{
			std::shared_ptr<AnimationClip> clip;
			std::vector<int32_t> binding;
			std::vector<float> translationScale;
			std::shared_ptr<Skeleton> source;// クリップの元のスケルトン
			float time = 0.0f;
		};
		void Advance(float dt);
		std::shared_ptr<AnimationClip> ResolveClip(const std::string& source, const std::string& name);
		void BindLayer(Layer& layer, const Skeleton& skeleton);

		AssetRef<Model> mSource;
		std::shared_ptr<Skeleton> mResolvedSkeleton;
		std::string mPendingClip;
		bool mHasPending = false;
		std::string mPendingSource;
		float mPendingFade = 0.0f;
		bool mPlaying = false;
		float mTime = 0.0f;
		Layer mCurrent;
		Layer mPrevious;
		float mFade = 0.0f;// 前のクリップからの切り替え進行（秒）
		float mFadeDuration = 0.0f;

		// 姿勢計算（ジョブ）
		const Skeleton* mBoundSkeleton = nullptr;
		JobHandle mJob;
		std::vector<TransformTRS> mLocal;
		std::vector<TransformTRS> mBlend;
		std::vector<Matrix4> mGlobal;
		std::vector<Matrix4> mPalette;
		uint64_t mUploadedFrame = UINT64_MAX;
		D3D12_GPU_VIRTUAL_ADDRESS mPaletteGpu = 0;
	};
}
