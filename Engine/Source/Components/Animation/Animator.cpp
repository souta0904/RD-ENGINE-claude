#include "RdPch.h"
#include "Components/Animation/Animator.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Components/Rendering/MeshRenderer.h"
#include "RHI/Device/GpuScope.h"
#include "Scene/Actor/Actor.h"
#if RD_EDITOR
#include <ImGui/imgui.h>
#endif

namespace rd
{
	void Animator::Reflect(PropertyVisitor& v)
	{
		v.Property("Clip Source", clipSource, { .flags = PropertyMeta::AssetModel, .tooltip = "Empty = MeshRenderer's model" });
		v.Property("Clip", clip);
		v.Property("Speed", speed, { .speed = 0.01f, .min = -10.0f, .max = 10.0f });
		v.Property("Loop", loop);
		v.Property("Play On Start", playOnStart);
		v.Property("Preview In Editor", previewInEditor);
		v.Property("Drive Bone Actors", driveBoneActors);
	}

	void Animator::OnValidate()
	{
		Play(clip, 0.0f, clipSource);
	}

	void Animator::Start()
	{
		if (playOnStart) { Play(clip, 0.0f, clipSource); }
	}

	std::shared_ptr<AnimationClip> Animator::ResolveClip(const std::string& source, const std::string& name)
	{
		AssetRef<Model> model;
		if (!source.empty())
		{
			if (!mSource || mSource->GetPath() != source + "#anim") { mSource = AssetManager::Get().LoadAnimations(source); }
			model = mSource;
		}
		else if (auto mr = GetComponent<MeshRenderer>())
		{
			model = mr->GetModel();
		}
		if (!model || !model->IsReady()) { return nullptr; }
		mResolvedSkeleton = model->GetSkeleton();
		auto result = model->FindClip(name);
		if (!result && !name.empty()) { Logger::Warning(LogCat::Scene, "Animator: clip '{}' not found in {}", name, model->GetPath()); }
		return result;
	}

	// アセットがまだ読み込み中かもしれないので、ここでは読み込みを始めるだけで、クリップを決めるのは Advance
	void Animator::Play(const std::string& clipName, float fade, const std::string& source, bool restart)
	{
		mPendingClip = clipName;
		mHasPending = true;
		mPendingRestart = restart;
		mPendingSource = source;
		mPendingFade = fade;
		mPlaying = true;
		if (!source.empty()) { ResolveClip(source, clipName); }
	}

	float Animator::GetNormalizedTime() const
	{
		return mCurrent.clip && mCurrent.clip->duration > 0.0f ? mCurrent.time / mCurrent.clip->duration : 0.0f;
	}

	std::vector<std::string> Animator::GetClipNames() const
	{
		std::vector<std::string> names;
		AssetRef<Model> model = !clipSource.empty() ? mSource : nullptr;
		if (!model)
		{
			if (auto mr = GetComponent<MeshRenderer>()) { model = mr->GetModel(); }
		}
		if (model && model->IsReady())
		{
			for (auto& c : model->GetClips()) { names.push_back(c->name); }
		}
		return names;
	}

	void Animator::BindLayer(Layer& layer, const Skeleton& skeleton)
	{
		layer.binding = layer.clip ? layer.clip->BuildBinding(skeleton) : std::vector<int32_t>{};
		layer.translationScale = layer.clip ? layer.clip->BuildTranslationScale(skeleton, layer.source.get()) : std::vector<float>{};
	}

	void Animator::Update(float dt)
	{
		Advance(dt);
	}

	void Animator::LateUpdate(float)
	{
		if (!driveBoneActors || !mBoundSkeleton) { return; }
		JobSystem::Wait(mJob);
		if (!mPoseReady || mGlobal.size() != mBoundSkeleton->joints.size()) { return; }
		if (mBoneSkeleton != mBoundSkeleton) { BindBoneActors(*mBoundSkeleton); }
		WriteBoneActors();
	}

	void Animator::SetJointRotationOffset(const std::string& joint, const Quaternion& worldRotation)
	{
		// ワールドの回転をモデル空間に直す（v_model * M * R * M^-1 = モデル空間で R を掛けたもの）
		Quaternion model = GetActor()->GetTransform().GetRotation();
		Quaternion local = Normalize(model * worldRotation * Inverse(model));
		bool identity = std::abs(local.w) > 0.99999f;
		for (size_t i = 0; i < mJointOffsets.size(); ++i)
		{
			if (mJointOffsets[i].name != joint) { continue; }
			if (identity) { mJointOffsets.erase(mJointOffsets.begin() + ptrdiff_t(i)); }
			else { mJointOffsets[i].rotation = local; }
			return;
		}
		if (!identity) { mJointOffsets.push_back({ joint, -1, local }); }
	}

	// バインドポーズのうちに、アクターと関節の対応とずれを覚える（アクターはシーンに置いたときの姿勢 = バインドポーズ）
	void Animator::BindBoneActors(const Skeleton& skeleton)
	{
		mBoneSkeleton = &skeleton;
		mBoneActors.clear();
		std::vector<TransformTRS> bindLocal(skeleton.joints.size());
		for (size_t i = 0; i < bindLocal.size(); ++i) { bindLocal[i] = skeleton.joints[i].bindLocal; }
		std::vector<Matrix4> bindGlobal, unused;
		ComputeSkinningPalette(skeleton, bindLocal, bindGlobal, unused);

		Matrix4 invModel = Inverse(GetActor()->GetTransform().GetWorldMatrix());
		std::vector<Actor*> stack(GetActor()->GetChildren().begin(), GetActor()->GetChildren().end());
		while (!stack.empty())
		{
			Actor* a = stack.back();
			stack.pop_back();
			int32_t joint = skeleton.Find(a->GetName());
			if (joint >= 0)
			{
				Matrix4 offset = a->GetTransform().GetWorldMatrix() * invModel * Inverse(bindGlobal[size_t(joint)]);
				mBoneActors.push_back({ joint, a, offset });
			}
			for (Actor* c : a->GetChildren()) { stack.push_back(c); }
		}
		// 親から順に書くので関節の順に並べる
		std::sort(mBoneActors.begin(), mBoneActors.end(), [](auto& x, auto& y) { return x.joint < y.joint; });
		Logger::Debug(LogCat::Scene, "Animator: {} bone actors bound on '{}'", mBoneActors.size(), GetActor()->GetName());
	}

	void Animator::WriteBoneActors()
	{
		const Matrix4& model = GetActor()->GetTransform().GetWorldMatrix();
		for (auto& b : mBoneActors)
		{
			b.actor->GetTransform().SetWorldMatrix(b.offset * mGlobal[size_t(b.joint)] * model);
		}
	}

	void Animator::OnEditorUpdate(float dt)
	{
		if (previewInEditor)
		{
			if (!mPlaying) { Play(clip, 0.0f, clipSource); }
			Advance(dt);
		}
		else if (mPlaying)
		{
			mPlaying = false;
			mCurrent = {};
			mPrevious = {};
		}
	}

	void Animator::Advance(float dt)
	{
		auto mr = GetComponent<MeshRenderer>();
		const Model* skinModel = mr ? mr->GetModel().get() : nullptr;
		if (!skinModel || !skinModel->IsReady() || !skinModel->GetSkeleton()) { return; }
		const Skeleton& skeleton = *skinModel->GetSkeleton();

		// スケルトンが変わったら対応表を作り直す
		if (mBoundSkeleton != &skeleton)
		{
			mBoundSkeleton = &skeleton;
			BindLayer(mCurrent, skeleton);
			BindLayer(mPrevious, skeleton);
		}

		if (mHasPending || (mPlaying && !mCurrent.clip)) { ResolvePending(skeleton); }
		if (!mPlaying || !mCurrent.clip) { return; }

		AdvanceLayer(mCurrent, dt);
		AdvanceLayer(mPrevious, dt);
		mTime = mCurrent.time;
		if (mPrevious.clip)
		{
			mFade += dt;
			if (mFade >= mFadeDuration) { mPrevious = {}; }
		}
		SchedulePose(skeleton);
	}

	// フェードするなら今のクリップを mPrevious に残して切り替える
	void Animator::ResolvePending(const Skeleton& skeleton)
	{
		auto c = ResolveClip(mPendingSource, mPendingClip);
		if (!c) { return; }
		if (c != mCurrent.clip || mPendingRestart)
		{
			if (mPendingFade > 0.0f && mCurrent.clip)
			{
				mPrevious = mCurrent;
				mFade = 0.0f;
				mFadeDuration = mPendingFade;
			}
			else
			{
				mPrevious = {};
			}
			mCurrent = {};
			mCurrent.clip = c;
			mCurrent.source = mResolvedSkeleton;
			BindLayer(mCurrent, skeleton);
		}
		mHasPending = false;
		mPendingRestart = false;
	}

	void Animator::AdvanceLayer(Layer& layer, float dt) const
	{
		if (!layer.clip) { return; }
		float duration = std::max(layer.clip->duration, 1.0e-4f);
		layer.time += dt * speed;
		if (loop) { layer.time = math::Wrap(layer.time, 0.0f, duration); }
		else { layer.time = math::Clamp(layer.time, 0.0f, duration); }
	}

	// 姿勢の計算をジョブに投げる（描くときに GetPaletteGpu で終わるのを待つ）
	//  ジョブには再生の状態をコピーして渡し、クロスフェード中は前のクリップの姿勢と混ぜる
	void Animator::SchedulePose(const Skeleton& skeleton)
	{
		Layer current = mCurrent;
		Layer previous = mPrevious;
		float weight = mPrevious.clip ? math::Saturate(mFade / std::max(mFadeDuration, 1.0e-4f)) : 1.0f;
		const Skeleton* sk = &skeleton;
		for (auto& o : mJointOffsets) { o.joint = skeleton.Find(o.name); }
		std::vector<JointOffset> offsets = mJointOffsets;
		JobSystem::Wait(mJob);
		mPoseReady = true;
		mJob = JobSystem::Schedule([this, current, previous, weight, sk, offsets = std::move(offsets)]
			{
				size_t count = sk->joints.size();
				mLocal.resize(count);
				for (size_t i = 0; i < count; ++i) { mLocal[i] = sk->joints[i].bindLocal; }
				current.clip->Sample(current.time, current.binding, mLocal, current.translationScale);
				if (previous.clip && weight < 1.0f)
				{
					mBlend.resize(count);
					for (size_t i = 0; i < count; ++i) { mBlend[i] = sk->joints[i].bindLocal; }
					previous.clip->Sample(previous.time, previous.binding, mBlend, previous.translationScale);
					for (size_t i = 0; i < count; ++i) { mLocal[i] = TransformTRS::Blend(mBlend[i], mLocal[i], weight); }
				}
				// 足す回転: グローバルの向き G を G * R にしたいので、ローカルは L * Gp * R * Gp^-1（Gp は親のグローバルの向き）
				for (const JointOffset& o : offsets)
				{
					if (o.joint < 0) { continue; }
					Quaternion parent = Quaternion::kIdentity;
					for (int32_t p = sk->joints[size_t(o.joint)].parent; p >= 0; p = sk->joints[size_t(p)].parent)
					{
						parent = parent * mLocal[size_t(p)].rotation;
					}
					TransformTRS& l = mLocal[size_t(o.joint)];
					l.rotation = Normalize(l.rotation * parent * o.rotation * Inverse(parent));
				}
				ComputeSkinningPalette(*sk, mLocal, mGlobal, mPalette);
			}, JobPriority::High, "Animator Pose");
	}

	D3D12_GPU_VIRTUAL_ADDRESS Animator::GetPaletteGpu(const Model* skinModel)
	{
		if (!skinModel || !skinModel->GetSkeleton() || skinModel->GetSkeleton().get() != mBoundSkeleton) { return 0; }
		auto& device = GraphicsDevice::Get();
		if (mUploadedFrame != device.GetFrameNumber())
		{
			JobSystem::Wait(mJob);
			if (mPalette.size() != mBoundSkeleton->joints.size()) { return 0; }
			mPaletteGpu = device.UploadData(mPalette.data(), sizeof(Matrix4) * mPalette.size());
			mUploadedFrame = device.GetFrameNumber();
		}
		return mPaletteGpu;
	}

#if RD_EDITOR
	bool Animator::OnInspectorGUI()
	{
		auto names = GetClipNames();
		if (names.empty()) { return false; }
		if (ImGui::BeginCombo("Select Clip", clip.empty() ? names.front().c_str() : clip.c_str()))
		{
			for (auto& n : names)
			{
				if (ImGui::Selectable(n.c_str(), n == clip))
				{
					clip = n;
					Play(clip, 0.2f, clipSource);
				}
			}
			ImGui::EndCombo();
		}
		ImGui::ProgressBar(GetNormalizedTime(), ImVec2(-1, 0), std::format("{:.2f}s", mTime).c_str());
		return false;
	}
#endif
}
