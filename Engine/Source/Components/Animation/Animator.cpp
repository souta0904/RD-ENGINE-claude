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
		if (!result && !name.empty()) { Log::Warning(LogCat::Scene, "Animator: clip '{}' not found in {}", name, model->GetPath()); }
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
		JobSystem::Wait(mJob);
		mJob = JobSystem::Schedule([this, current, previous, weight, sk]
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
	void Animator::OnInspectorGUI()
	{
		auto names = GetClipNames();
		if (names.empty()) { return; }
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
	}
#endif
}
