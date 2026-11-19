#include "RdPch.h"
#include "Editor/Core/AssetPreview.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Asset/Texture/Texture.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Style/EditorStyle.h"
#include "RHI/Device/GraphicsDevice.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <fstream>

namespace rd
{
	namespace
	{
		constexpr float kFovDegrees = 30.0f;
		constexpr float kDefaultYaw = 210.0f;
		constexpr float kDefaultPitch = 20.0f;
		constexpr uint32_t kThumbnailSize = 128;
		// サムネイルを 1 フレームに描く数と、覚えておく数（超えたら最近見ていないものから捨てる）
		constexpr int kThumbnailsPerFrame = 2;
		constexpr size_t kMaxThumbnails = 256;
		constexpr int kThumbnailKeepFrames = 120;

		Material ToMaterial(const MaterialDesc& d)
		{
			Material m;
			m.baseColor = d.baseColor;
			m.metallic = d.metallic;
			m.roughness = d.roughness;
			m.emissive = d.emissive;
			m.albedoPath = d.albedoPath;
			m.normalPath = d.normalPath;
			m.blend = d.transparent ? BlendMode::Alpha : BlendMode::Opaque;
			// 裏側から見ることもあるので両面にし、床がないので影は落とさない
			m.doubleSided = true;
			m.castShadows = false;
			return m;
		}

		bool IsSettled(const AssetRef<Texture>& t) { return !t || t->IsReady() || t->IsFailed(); }

		// 空は描かず、暗い灰色の背景にする
		Environment PreviewEnvironment()
		{
			Environment env;
			env.drawSky = false;
			env.skyHorizon = Color(0.13f, 0.14f, 0.16f, 1.0f);
			env.ambientIntensity = 0.8f;
			env.fogDensity = 0.0f;
			return env;
		}

		// Unity のプレハブは "%YAML" で始まるテキスト。エンジンのプレハブは JSON なので読めない
		bool IsUnityYaml(const std::string& path)
		{
			std::ifstream file(ToWide(path), std::ios::binary);
			char head[5] = {};
			file.read(head, sizeof(head));
			return file.gcount() == sizeof(head) && std::string_view(head, sizeof(head)) == "%YAML";
		}

		// 左ドラッグで回す・ホイールで寄る・ダブルクリックで元の向き。残りの広さいっぱいに描く
		void DrawOrbitView(PreviewView& view)
		{
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 size(std::max(1.0f, std::floor(avail.x)), std::max(64.0f, std::floor(avail.y)));
			ImVec2 pos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##orbitPreview", size);
			bool hovered = ImGui::IsItemHovered();
			ImGuiIO& io = ImGui::GetIO();
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
			{
				view.yaw += io.MouseDelta.x * 0.4f;
				view.pitch = std::clamp(view.pitch + io.MouseDelta.y * 0.4f, -89.0f, 89.0f);
			}
			if (hovered && io.MouseWheel != 0.0f) { view.zoom = std::clamp(view.zoom * std::pow(0.88f, io.MouseWheel), 0.2f, 5.0f); }
			if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				view.yaw = kDefaultYaw;
				view.pitch = kDefaultPitch;
				view.zoom = 1.0f;
			}
			if (hovered)
			{
				// ホイールでパネルが流れないようにする
				ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 end(pos.x + size.x, pos.y + size.y);
			if (view.Render(uint32_t(size.x), uint32_t(size.y)))
			{
				dl->AddImage(ImTextureID(view.GetOutput().GetGpuSrv().ptr), pos, end);
			}
			dl->AddRect(pos, end, ImGui::GetColorU32(ImGuiCol_Border));
			if (hovered)
			{
				// 狭いときは短い説明にし、それでも収まらなければ切る
				const char* hint = "Drag: rotate   Wheel: zoom   Double-click: reset";
				if (ImGui::CalcTextSize(hint).x + 20.0f > size.x) { hint = "Drag / Wheel / Double-click"; }
				ImVec2 textSize = ImGui::CalcTextSize(hint);
				ImVec2 textPos(pos.x + 6.0f, end.y - textSize.y - 6.0f);
				dl->PushClipRect(pos, end, true);
				dl->AddRectFilled(ImVec2(textPos.x - 4.0f, textPos.y - 2.0f), ImVec2(textPos.x + textSize.x + 4.0f, textPos.y + textSize.y + 2.0f), IM_COL32(0, 0, 0, 140), 3.0f);
				dl->AddText(textPos, IM_COL32(230, 230, 230, 255), hint);
				dl->PopClipRect();
			}
		}
	}

	// ---------------------------------------------------------------- PreviewView

	// 包む球が画面に収まる距離から見て、カメラの左上から当てる光と、後ろからの弱い光で照らす
	void PreviewView::DrawList(const AABB& bounds, uint32_t width, uint32_t height)
	{
		Vector3 center = bounds.GetCenter();
		float radius = std::max(Length(bounds.GetExtents()), 1.0e-3f);
		float fov = math::ToRadians(kFovDegrees);
		float aspect = float(width) / float(height);
		// 横長でも縦長でも、狭い方の画角に球が収まるようにする
		float halfFov = aspect < 1.0f ? std::atan(std::tan(fov * 0.5f) * aspect) : fov * 0.5f;
		float distance = radius / std::sin(halfFov) * zoom;
		Quaternion rotation = Quaternion::FromEuler({ math::ToRadians(pitch), math::ToRadians(yaw), 0.0f });
		Vector3 forward = Vector3::kUnitZ * rotation;
		Vector3 right = Vector3::kUnitX * rotation;
		Vector3 up = Vector3::kUnitY * rotation;
		float nearZ = std::max(distance - radius * 1.5f, distance * 0.01f);
		RenderCamera camera = RenderCamera::Make(center - forward * distance, rotation, fov, aspect, nearZ, distance + radius * 2.0f);

		GpuDirLight key;
		key.direction = Normalize(forward - up * 0.9f + right * 0.6f);
		key.color = Vector3(1.0f, 0.97f, 0.92f);
		key.intensity = 1.6f;
		GpuDirLight rim;
		rim.direction = Normalize(-forward - up * 0.3f - right * 0.4f);
		rim.color = Vector3(0.75f, 0.85f, 1.0f);
		rim.intensity = 0.6f;
		mList.dirLights.push_back(key);
		mList.dirLights.push_back(rim);

		Renderer::Get().RenderOffscreen(mTargets, mList, camera, PreviewEnvironment(), width, height);
	}

	// ---------------------------------------------------------------- ModelView

	void ModelView::SetModel(const std::string& path)
	{
		if (path == mPath && (mModel || path.empty())) { return; }
		mPath = path;
		mModel = path.empty() ? nullptr : AssetManager::Get().LoadModel(path);
		mSynced = nullptr;
		mMaterials.clear();
		mBindingClip = -2;
		clip = -1;
		time = 0.0f;
	}

	bool ModelView::IsReady() const
	{
		return mModel && mModel->IsReady();
	}

	bool ModelView::IsFullyLoaded()
	{
		if (!mModel) { return true; }
		if (!mModel->IsReady()) { return mModel->IsFailed(); }
		SyncMaterials();
		for (const auto& m : mMaterials)
		{
			if (!IsSettled(m.albedo) || !IsSettled(m.normal) || !IsSettled(m.emissiveMap)) { return false; }
		}
		return true;
	}

	void ModelView::SyncMaterials()
	{
		if (mSynced == mModel.get()) { return; }
		mMaterials.clear();
		for (const auto& d : mModel->GetMaterials()) { mMaterials.push_back(ToMaterial(d)); }
		if (mMaterials.empty()) { mMaterials.push_back(ToMaterial(MaterialDesc{})); }
		for (auto& m : mMaterials) { m.ResolveTextures(); }
		mSynced = mModel.get();
	}

	// クリップを選んでいればその時刻の姿勢、なければバインドポーズ
	D3D12_GPU_VIRTUAL_ADDRESS ModelView::BuildPalette()
	{
		const auto& skeleton = mModel->GetSkeleton();
		if (!skeleton || skeleton->joints.empty()) { return 0; }
		mLocal.resize(skeleton->joints.size());
		for (size_t i = 0; i < mLocal.size(); ++i) { mLocal[i] = skeleton->joints[i].bindLocal; }
		const auto& clips = mModel->GetClips();
		if (clip >= 0 && clip < int(clips.size()))
		{
			const AnimationClip& c = *clips[size_t(clip)];
			if (mBindingClip != clip)
			{
				mBinding = c.BuildBinding(*skeleton);
				mBindingClip = clip;
			}
			float t = c.duration > 0.0f ? std::fmod(time, c.duration) : 0.0f;
			c.Sample(t, mBinding, mLocal);
		}
		ComputeSkinningPalette(*skeleton, mLocal, mGlobal, mPalette);
		return GraphicsDevice::Get().UploadData(mPalette.data(), sizeof(Matrix4) * mPalette.size());
	}

	bool ModelView::Render(uint32_t width, uint32_t height)
	{
		if (!IsReady() || width == 0 || height == 0) { return false; }
		SyncMaterials();
		const Model& model = *mModel;
		const AABB& bounds = model.GetBounds();
		Vector3 center = bounds.GetCenter();
		float radius = std::max(Length(bounds.GetExtents()), 1.0e-3f);

		D3D12_GPU_VIRTUAL_ADDRESS palette = model.HasSkinning() ? BuildPalette() : 0;
		const auto& subs = model.GetSubMeshes();
		for (uint32_t i = 0; i < subs.size(); ++i)
		{
			MeshDrawItem item;
			item.model = &model;
			item.subMesh = i;
			item.material = &mMaterials[std::min<size_t>(subs[i].materialIndex, mMaterials.size() - 1)];
			item.palette = subs[i].skinned ? palette : 0;
			// 動かすと境界から出るので、切り捨てられないように大きめにする
			item.worldBounds = AABB::FromCenterExtents(center, Vector3(radius, radius, radius) * 4.0f);
			mList.meshes.push_back(item);
		}
		DrawList(bounds, width, height);
		return true;
	}

	// ---------------------------------------------------------------- PrefabView

	PrefabView::PrefabView() = default;
	PrefabView::~PrefabView() = default;

	// 選び直すたびにシーンごと作り直す（前のプレハブのアクターは残さない）
	void PrefabView::SetPrefab(const std::string& path)
	{
		if (path == mPath) { return; }
		mPath = path;
		mError.clear();
		mScene.reset();
		if (path.empty()) { return; }
		if (IsUnityYaml(path))
		{
			mError = "Unity prefab (YAML) cannot be previewed. Use the exported one.";
			return;
		}
		mScene = std::make_unique<Scene>();
		mScene->SetName("Prefab Preview");
		if (!mScene->Instantiate(path))
		{
			mScene.reset();
			mError = "Failed to load prefab";
		}
	}

	size_t PrefabView::GetActorCount() const
	{
		return mScene ? mScene->GetActorCount() : 0;
	}

	bool PrefabView::IsReady() const
	{
		return mScene != nullptr;
	}

	// どのモデルもテクスチャも読み終わったか（マテリアルは一度描くとそろうので、まだなら未完了）
	bool PrefabView::IsFullyLoaded()
	{
		if (!mScene) { return true; }
		for (MeshRenderer* mr : mScene->FindComponents<MeshRenderer>())
		{
			const auto& model = mr->GetModel();
			if (!model || model->IsFailed()) { continue; }
			if (!model->IsReady() || mr->GetMaterials().empty()) { return false; }
			for (const auto& m : mr->GetMaterials())
			{
				if (!IsSettled(m.albedo) || !IsSettled(m.normal) || !IsSettled(m.emissiveMap)) { return false; }
			}
		}
		return true;
	}

	// MeshRenderer の箱をまとめる。何もなければアクターの位置のまわり
	AABB PrefabView::ComputeBounds() const
	{
		AABB bounds;
		for (MeshRenderer* mr : mScene->FindComponents<MeshRenderer>()) { bounds.Expand(mr->GetWorldBounds()); }
		if (!bounds.IsValid())
		{
			mScene->ForEachActor([&](Actor* a) { bounds.Expand(a->GetTransform().GetPosition()); });
			bounds = AABB::FromCenterExtents(bounds.IsValid() ? bounds.GetCenter() : Vector3::kZero, Vector3(0.5f, 0.5f, 0.5f));
		}
		return bounds;
	}

	// プレイ中ではないので、描くのはエディタで置いたときと同じ姿（アニメーションはバインドポーズ）
	bool PrefabView::Render(uint32_t width, uint32_t height)
	{
		if (!mScene || width == 0 || height == 0) { return false; }
		mScene->CollectRender(mList);
		DrawList(ComputeBounds(), width, height);
		return true;
	}

	// ---------------------------------------------------------------- プレビューパネルのモデル

	void ModelPreviewWidget::Draw(const std::string& path)
	{
		mView.SetModel(path);
		Model* model = mView.GetModel();
		if (!model || !model->IsReady()) { return; }

		const auto& clips = model->GetClips();
		if (!clips.empty())
		{
			const char* current = mView.clip >= 0 && mView.clip < int(clips.size()) ? clips[size_t(mView.clip)]->name.c_str() : "Bind Pose";
			ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() * 2.5f);
			if (ImGui::BeginCombo("##clip", current))
			{
				if (ImGui::Selectable("Bind Pose", mView.clip < 0)) { mView.clip = -1; }
				for (int i = 0; i < int(clips.size()); ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(clips[size_t(i)]->name.c_str(), mView.clip == i))
					{
						mView.clip = i;
						mView.time = 0.0f;
						mPlaying = true;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(mView.clip < 0);
			if (ImGui::Button(mPlaying ? "Pause" : "Play", ImVec2(-1.0f, 0.0f))) { mPlaying = !mPlaying; }
			ImGui::EndDisabled();
			if (mView.clip >= 0 && mView.clip < int(clips.size()))
			{
				float duration = clips[size_t(mView.clip)]->duration;
				float t = duration > 0.0f ? std::fmod(mView.time, duration) : 0.0f;
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::SliderFloat("##time", &t, 0.0f, std::max(duration, 1.0e-3f), "%.2f s"))
				{
					mView.time = t;
					mPlaying = false;
				}
			}
		}
		if (mPlaying && mView.clip >= 0) { mView.time += ImGui::GetIO().DeltaTime; }
		DrawOrbitView(mView);
	}

	// ---------------------------------------------------------------- プレビューパネルのプレハブ

	void PrefabPreviewWidget::Draw(const std::string& path)
	{
		mView.SetPrefab(path);
		if (!mView.IsReady())
		{
			ImGui::TextDisabled("%s", mView.GetError().c_str());
			return;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%zu actors", mView.GetActorCount());
		DrawOrbitView(mView);
	}

	// ---------------------------------------------------------------- サムネイル

	const GpuTexture* AssetThumbnails::Get(const std::string& path)
	{
		int frame = ImGui::GetFrameCount();
		if (frame != mFrame)
		{
			mFrame = frame;
			mRenderedThisFrame = 0;
			if (mEntries.size() > kMaxThumbnails) { Evict(); }
		}
		Entry& e = mEntries[path];
		e.lastUsed = frame;
		if (!e.view)
		{
			if (GetExtension(path) == ".prefab")
			{
				auto view = std::make_unique<PrefabView>();
				view->SetPrefab(path);
				e.view = std::move(view);
			}
			else
			{
				auto view = std::make_unique<ModelView>();
				view->SetModel(path);
				e.view = std::move(view);
			}
		}
		// テクスチャが読み終わるまでは描き直す
		if (!e.done && mRenderedThisFrame < kThumbnailsPerFrame && e.view->IsReady())
		{
			bool loaded = e.view->IsFullyLoaded();
			if (e.view->Render(kThumbnailSize, kThumbnailSize))
			{
				++mRenderedThisFrame;
				e.rendered = true;
				e.done = loaded;
			}
		}
		return e.rendered ? &e.view->GetOutput() : nullptr;
	}

	void AssetThumbnails::Evict()
	{
		std::erase_if(mEntries, [&](const auto& kv) { return kv.second.lastUsed < mFrame - kThumbnailKeepFrames; });
	}

	// ---------------------------------------------------------------- テクスチャ

	void DrawTexturePreview(const Texture& texture, float maxHeight)
	{
		float texW = float(std::max(1u, texture.GetWidth()));
		float texH = float(std::max(1u, texture.GetHeight()));
		float availW = std::max(1.0f, ImGui::GetContentRegionAvail().x);
		float scale = std::min(availW / texW, maxHeight / texH);
		ImVec2 size(texW * scale, texH * scale);
		ImVec2 pos = ImGui::GetCursorScreenPos();
		pos.x += std::floor((availW - size.x) * 0.5f);
		ImGui::SetCursorScreenPos(pos);
		ImGui::InvisibleButton("##texturePreview", size);
		bool hovered = ImGui::IsItemHovered();
		ImVec2 end(pos.x + size.x, pos.y + size.y);

		// 透明なところが分かるように市松模様を敷く
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float cell = 8.0f;
		dl->PushClipRect(pos, end, true);
		dl->AddRectFilled(pos, end, IM_COL32(70, 70, 70, 255));
		for (float y = pos.y; y < end.y; y += cell)
		{
			int row = int((y - pos.y) / cell);
			for (float x = pos.x + ((row & 1) ? cell : 0.0f); x < end.x; x += cell * 2.0f)
			{
				dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cell, y + cell), IM_COL32(100, 100, 100, 255));
			}
		}
		ImTextureID id = ImTextureID(texture.GetSrv().ptr);
		dl->AddImage(id, pos, end);
		dl->PopClipRect();
		dl->AddRect(pos, end, ImGui::GetColorU32(ImGuiCol_Border));

		// マウスの下を 4 倍に拡大して、ピクセルの位置と一緒に出す
		if (hovered)
		{
			ImVec2 m = ImGui::GetMousePos();
			float u = std::clamp((m.x - pos.x) / size.x, 0.0f, 1.0f);
			float v = std::clamp((m.y - pos.y) / size.y, 0.0f, 1.0f);
			const float zoom = 4.0f, loupe = 160.0f;
			float hu = loupe / (zoom * scale * 2.0f) / texW;
			float hv = loupe / (zoom * scale * 2.0f) / texH;
			ImGui::BeginTooltip();
			ImGui::Text("%d, %d", int(u * texW), int(v * texH));
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::Dummy(ImVec2(loupe, loupe));
			ImDrawList* tl = ImGui::GetWindowDrawList();
			tl->AddRectFilled(p, ImVec2(p.x + loupe, p.y + loupe), IM_COL32(70, 70, 70, 255));
			tl->AddImage(id, p, ImVec2(p.x + loupe, p.y + loupe), ImVec2(u - hu, v - hv), ImVec2(u + hu, v + hv));
			ImGui::EndTooltip();
		}
	}
}
