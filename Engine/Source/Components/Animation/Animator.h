#pragma once
#include "Asset/Animation/AnimationClip.h"
#include "Asset/Asset.h"
#include "Core/Jobs/JobSystem.h"
#include "RHI/D3D12Common.h"
#include "Scene/Component/Component.h"
#include <vector>

namespace rd
{
	class Model;

	// スケルタルアニメーションの再生（クロスフェードできる）
	//  姿勢の計算はジョブで並列に行い、描くときに終わるのを待つ
	//  clipSource はアニメーションを読むファイル（空なら MeshRenderer のモデル）、clip はクリップ名（空なら最初のクリップ）
	class RD_API Animator final : public Component
	{
		RD_COMPONENT(Animator)
	public:
		std::string clipSource;
		std::string clip;
		float speed = 1.0f;
		bool loop = true;
		bool playOnStart = true;
		bool previewInEditor = false;
		// 子孫のうち関節と同じ名前のアクターを、アニメーションの姿勢に合わせて動かす
		//  （手や頭に付けた当たり判定を骨に付いて行かせるため。姿勢の計算を LateUpdate で待つ）
		bool driveBoneActors = false;

		~Animator() override { JobSystem::Wait(mJob); }
		void Reflect(PropertyVisitor& v) override;
		void OnValidate() override;
		void OnDestroy() override { JobSystem::Wait(mJob); }
		void Start() override;
		void Update(float dt) override;
		void LateUpdate(float dt) override;
		void OnEditorUpdate(float dt) override;

		// fade 秒かけて今のクリップから切り替える（restart なら同じクリップでも最初から）
		void Play(const std::string& clipName, float fade = 0.2f, const std::string& source = "", bool restart = false);
		void Stop() { mPlaying = false; }
		bool IsPlaying() const { return mPlaying; }
		float GetTime() const { return mTime; }
		float GetNormalizedTime() const;
		std::vector<std::string> GetClipNames() const;

		// 関節の向きに、アニメーションの後からワールド空間の回転を足す（首をネズミに向けるなど）
		//  消すまで毎フレームかかる。identity を渡すと消える
		void SetJointRotationOffset(const std::string& joint, const Quaternion& worldRotation);
		void ClearJointRotationOffsets() { mJointOffsets.clear(); }

		// 描くときのスキニング行列（このフレームの GPU アドレス。まだなら 0）
		D3D12_GPU_VIRTUAL_ADDRESS GetPaletteGpu(const Model* skinModel);

	private:
		// 再生中のクリップ 1 つ（source はクリップの元のスケルトン）
		struct Layer
		{
			std::shared_ptr<AnimationClip> clip;
			std::vector<int32_t> binding;
			std::vector<float> translationScale;
			std::shared_ptr<Skeleton> source;
			float time = 0.0f;
		};
		void Advance(float dt);
		void AdvanceLayer(Layer& layer, float dt) const;
		void ResolvePending(const Skeleton& skeleton);
		void SchedulePose(const Skeleton& skeleton);
		std::shared_ptr<AnimationClip> ResolveClip(const std::string& source, const std::string& name);
		void BindLayer(Layer& layer, const Skeleton& skeleton);
		void BindBoneActors(const Skeleton& skeleton);
		void WriteBoneActors();

		// 関節に付いて行くアクター（offset はバインドポーズでのアクターのワールド * 関節のバインドの逆）
		struct BoneActor
		{
			int32_t joint = -1;
			Actor* actor = nullptr;
			Matrix4 offset;
		};
		// rotation はモデル空間の回転
		struct JointOffset
		{
			std::string name;
			int32_t joint = -1;
			Quaternion rotation;
		};
		std::vector<BoneActor> mBoneActors;
		const Skeleton* mBoneSkeleton = nullptr;
		std::vector<JointOffset> mJointOffsets;
		bool mPoseReady = false;

		AssetRef<Model> mSource;
		std::shared_ptr<Skeleton> mResolvedSkeleton;
		std::string mPendingClip;
		bool mHasPending = false;
		bool mPendingRestart = false;
		std::string mPendingSource;
		float mPendingFade = 0.0f;
		bool mPlaying = false;
		float mTime = 0.0f;
		Layer mCurrent;
		Layer mPrevious;
		float mFade = 0.0f;
		float mFadeDuration = 0.0f;

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
