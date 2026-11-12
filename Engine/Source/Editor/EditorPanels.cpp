#include "RdPch.h"
#include "Editor/Editor.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture.h"
#include "Core/JobSystem.h"
#include "Core/JsonArchive.h"
#include "Core/StringUtil.h"
#include "Core/Time.h"
#include "Editor/ImGuiInspector.h"
#include "Engine.h"
#include "Physics/PhysicsWorld.h"
#include "Render/Renderer.h"
#include "RHI/GraphicsDevice.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	namespace
	{
		// 階層ビューでの操作はツリー描画中に行うと走査が壊れるので後でまとめて実行する
		struct HierarchyOps
		{
			Actor* reparent = nullptr;
			Actor* newParent = nullptr;
			bool toRoot = false;
			Actor* remove = nullptr;
			Actor* duplicate = nullptr;
			Actor* createChildOf = nullptr;
			Actor* savePrefab = nullptr;
		};
		HierarchyOps gOps;

		const char* FileTag(const std::string& ext)
		{
			if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".dds" || ext == ".hdr") { return "IMG"; }
			if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx") { return "MDL"; }
			if (ext == ".wav") { return "SND"; }
			if (ext == ".scene") { return "SCN"; }
			if (ext == ".prefab") { return "PFB"; }
			if (ext == ".hlsl" || ext == ".hlsli") { return "SHD"; }
			return "FILE";
		}
	}

	// ==================================================
	// Hierarchy
	// ==================================================
	void Editor::DrawHierarchy()
	{
		ImGui::Begin("Hierarchy", &mShowHierarchy);
		Scene& scene = Engine::Get().GetScene();

		if (ImGui::Button("+ Create")) { ImGui::OpenPopup("CreateActor"); }
		if (ImGui::BeginPopup("CreateActor"))
		{
			for (const char* kind : { "Empty", "Cube", "Sphere", "Plane", "Capsule", "Directional Light", "Point Light",
				"Spot Light", "Camera", "Particle System" })
			{
				if (ImGui::MenuItem(kind)) { CreateActorFromMenu(kind); }
			}
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%zu actors", scene.GetActorCount());
		ImGui::Separator();

		gOps = {};
		ImGui::BeginChild("tree");
		auto roots = scene.GetRootActors();// コピー（描画中の変更に備える）
		for (Actor* root : roots) { DrawHierarchyNode(root); }

		// 何もないところへのドロップ = ルートへ移動
		ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, std::max(40.0f, ImGui::GetContentRegionAvail().y)));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID"))
			{
				gOps.reparent = scene.FindActor(*static_cast<const ActorId*>(p->Data));
				gOps.toRoot = true;
			}
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				std::string path = static_cast<const char*>(p->Data);
				if (GetExtension(path) == ".prefab")
				{
					Select(scene.Instantiate(path));
					Commit("Instantiate");
				}
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::IsItemClicked()) { mSelectedId = 0; }
		ImGui::EndChild();

		// 後回しにした操作
		if (gOps.reparent && (gOps.newParent || gOps.toRoot))
		{
			gOps.reparent->SetParent(gOps.toRoot ? nullptr : gOps.newParent);
			Commit("Reparent");
		}
		if (gOps.duplicate) { Select(gOps.duplicate); DuplicateSelected(); }
		if (gOps.remove) { Select(gOps.remove); DeleteSelected(); }
		if (gOps.createChildOf)
		{
			Actor* child = scene.CreateActor("Actor", gOps.createChildOf);
			Select(child);
			Commit("Create Child");
		}
		if (gOps.savePrefab)
		{
			std::string path = "Assets/Prefabs/" + gOps.savePrefab->GetName() + ".prefab";
			if (Scene::SavePrefab(gOps.savePrefab, path)) { Log::Info("Saved prefab: {}", path); }
		}
		ImGui::End();
	}

	void Editor::DrawHierarchyNode(Actor* actor)
	{
		Scene& scene = Engine::Get().GetScene();
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (actor->GetChildren().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
		if (actor->GetId() == mSelectedId) { flags |= ImGuiTreeNodeFlags_Selected; }

		// 選択中のアクターの親は開いておく
		if (Actor* sel = GetSelected(); sel && sel->IsDescendantOf(actor)) { ImGui::SetNextItemOpen(true, ImGuiCond_Once); }

		if (!actor->IsActive()) { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); }
		bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(actor->GetId()), flags, "%s", actor->GetName().c_str());
		if (!actor->IsActive()) { ImGui::PopStyleColor(); }

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { Select(actor); }

		// ドラッグで親子付け替え
		if (ImGui::BeginDragDropSource())
		{
			ActorId id = actor->GetId();
			ImGui::SetDragDropPayload("ACTOR_ID", &id, sizeof(id));
			ImGui::Text("%s", actor->GetName().c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID"))
			{
				Actor* dragged = scene.FindActor(*static_cast<const ActorId*>(p->Data));
				if (dragged && dragged != actor && !actor->IsDescendantOf(dragged))
				{
					gOps.reparent = dragged;
					gOps.newParent = actor;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem())
		{
			Select(actor);
			if (ImGui::MenuItem("Create Child")) { gOps.createChildOf = actor; }
			if (ImGui::MenuItem("Duplicate", "Ctrl+D")) { gOps.duplicate = actor; }
			if (ImGui::MenuItem("Delete", "Del")) { gOps.remove = actor; }
			if (actor->GetParent() && ImGui::MenuItem("Unparent")) { gOps.reparent = actor; gOps.toRoot = true; }
			ImGui::Separator();
			if (ImGui::MenuItem("Save as Prefab")) { gOps.savePrefab = actor; }
			ImGui::EndPopup();
		}

		if (open)
		{
			auto children = actor->GetChildren();
			for (Actor* child : children) { DrawHierarchyNode(child); }
			ImGui::TreePop();
		}
	}

	// ==================================================
	// Inspector
	// ==================================================
	void Editor::DrawInspector()
	{
		ImGui::Begin("Inspector", &mShowInspector);
		Actor* actor = GetSelected();
		if (!actor)
		{
			ImGui::TextDisabled("No actor selected");
			ImGui::End();
			return;
		}

		ImGuiInspector inspector;
		bool commit = false;

		// アクターの基本情報
		bool active = actor->IsActiveSelf();
		if (ImGui::Checkbox("##active", &active)) { actor->SetActive(active); commit = true; }
		ImGui::SameLine();
		std::string name = actor->GetName();
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##name", &name)) { actor->SetName(name); }
		if (ImGui::IsItemDeactivatedAfterEdit()) { commit = true; }

		std::string tag = actor->GetTag();
		if (inspector.Property("Tag", tag)) { actor->SetTag(tag); }
		uint32_t layer = actor->GetLayer();
		if (inspector.Property("Layer", layer, { .speed = 0.1f, .min = 0.0f, .max = 31.0f })) { actor->SetLayer(layer); }
		ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(actor->GetId()));

		// Transform
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			actor->GetTransform().Reflect(inspector);
		}

		// コンポーネント
		Component* toRemove = nullptr;
		Component* toMoveUp = nullptr;
		Component* toMoveDown = nullptr;
		Component* toReset = nullptr;
		for (auto& comp : actor->GetAllComponents())
		{
			Component* c = comp.get();
			ImGui::PushID(c);
			bool open = ImGui::CollapsingHeader(c->GetTypeName(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
			if (ImGui::BeginPopupContextItem("comp"))
			{
				if (ImGui::MenuItem("Remove")) { toRemove = c; }
				if (ImGui::MenuItem("Move Up")) { toMoveUp = c; }
				if (ImGui::MenuItem("Move Down")) { toMoveDown = c; }
				if (ImGui::MenuItem("Reset")) { toReset = c; }
				ImGui::EndPopup();
			}
			// 有効/無効のチェックボックスをヘッダの右端に
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 4.0f);
			bool enabled = c->IsEnabled();
			if (ImGui::Checkbox("##enabled", &enabled)) { c->SetEnabled(enabled); commit = true; }

			if (open)
			{
				inspector.ResetFrameState();
				ImGui::Indent(4.0f);
				c->Reflect(inspector);
				c->OnInspectorGUI();
				ImGui::Unindent(4.0f);
				if (inspector.AnyChanged()) { c->OnValidate(); }
				commit |= inspector.AnyCommitted();
				inspector.ResetFrameState();
			}
			ImGui::PopID();
		}
		commit |= inspector.AnyCommitted();

		if (toRemove) { actor->RemoveComponent(toRemove); Engine::Get().GetScene().FlushPendingDestroy(); commit = true; }
		if (toMoveUp) { actor->MoveComponent(toMoveUp, -1); commit = true; }
		if (toMoveDown) { actor->MoveComponent(toMoveDown, 1); commit = true; }
		if (toReset)
		{
			// 新品のインスタンスのプロパティで上書き
			if (auto fresh = ComponentRegistry::Create(toReset->GetTypeName()))
			{
				Json j = Json::object();
				JsonWriter w(j);
				fresh->Reflect(w);
				JsonReader r(j);
				toReset->Reflect(r);
				toReset->OnValidate();
				commit = true;
			}
		}

		// コンポーネントの追加
		ImGui::Spacing();
		ImGui::Separator();
		float w = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w * 0.15f);
		if (ImGui::Button("Add Component", ImVec2(w * 0.7f, 0))) { ImGui::OpenPopup("AddComponent"); mAddComponentFilter.clear(); }
		if (ImGui::BeginPopup("AddComponent"))
		{
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::IsWindowAppearing()) { ImGui::SetKeyboardFocusHere(); }
			ImGui::InputTextWithHint("##filter", "Search...", &mAddComponentFilter);
			std::string filter = ToLower(mAddComponentFilter);
			std::string category;
			for (const auto& entry : ComponentRegistry::GetEntries())
			{
				if (!filter.empty() && ToLower(entry.name).find(filter) == std::string::npos) { continue; }
				if (entry.category != category)
				{
					category = entry.category;
					ImGui::SeparatorText(category.c_str());
				}
				if (ImGui::Selectable(entry.name.c_str()))
				{
					actor->AddComponent(entry.name);
					commit = true;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}

		if (commit) { Commit("Edit " + actor->GetName()); }
		ImGui::End();
	}

	// ==================================================
	// Assets
	// ==================================================
	void Editor::DrawAssets()
	{
		ImGui::Begin("Assets", &mShowAssets);

		// パンくずリスト
		auto parts = Split(mAssetDir, '/');
		std::string accum;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			accum += (i ? "/" : "") + parts[i];
			if (i) { ImGui::SameLine(0, 2); ImGui::TextDisabled(">"); ImGui::SameLine(0, 2); }
			if (ImGui::SmallButton(parts[i].c_str())) { mAssetDir = accum; }
		}
		ImGui::SameLine(0, 16);
		ImGui::SetNextItemWidth(180.0f);
		ImGui::InputTextWithHint("##filter", "Filter", &mAssetFilter);
		ImGui::Separator();

		ImGui::BeginChild("files");
		std::vector<std::filesystem::directory_entry> dirs, files;
		std::error_code ec;
		for (auto& e : std::filesystem::directory_iterator(ToWide(mAssetDir), ec))
		{
			(e.is_directory() ? dirs : files).push_back(e);
		}
		std::string filter = ToLower(mAssetFilter);

		const float cell = 92.0f;
		int columns = std::max(1, int(ImGui::GetContentRegionAvail().x / cell));
		ImGui::Columns(columns, nullptr, false);
		auto drawEntry = [&](const std::filesystem::directory_entry& e, bool isDir)
			{
				std::string fname = ToUtf8(e.path().filename().wstring());
				if (!filter.empty() && ToLower(fname).find(filter) == std::string::npos) { return; }
				std::string path = mAssetDir + "/" + fname;
				std::string ext = GetExtension(fname);
				ImGui::PushID(path.c_str());

				ImVec2 size(cell - 16, cell - 30);
				bool clicked = false;
				const char* tag = isDir ? "DIR" : FileTag(ext);
				if (!isDir && std::string(tag) == "IMG")
				{
					auto tex = AssetManager::Get().LoadTexture(path);
					if (tex->IsReady()) { clicked = ImGui::ImageButton("thumb", ImTextureID(tex->GetSrv().ptr), size); }
					else { clicked = ImGui::Button("...", size); }
				}
				else
				{
					if (isDir) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.30f, 0.15f, 1.0f)); }
					clicked = ImGui::Button(tag, size);
					if (isDir) { ImGui::PopStyleColor(); }
				}
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (isDir) { mAssetDir = path; }
					else if (ext == ".scene") { Engine::Get().LoadScene(path); }
				}
				(void)clicked;
				if (!isDir && ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("ASSET_PATH", path.c_str(), path.size() + 1);
					ImGui::Text("%s", fname.c_str());
					ImGui::EndDragDropSource();
				}
				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", path.c_str()); }
				ImGui::TextWrapped("%s", fname.c_str());
				ImGui::PopID();
				ImGui::NextColumn();
			};
		for (auto& d : dirs) { drawEntry(d, true); }
		for (auto& f : files) { drawEntry(f, false); }
		ImGui::Columns(1);
		ImGui::EndChild();
		ImGui::End();
	}

	// ==================================================
	// Console
	// ==================================================
	void Editor::DrawConsole()
	{
		ImGui::Begin("Console", &mShowConsole);
		if (ImGui::Button("Clear")) { Log::Clear(); }
		ImGui::SameLine();
		ImGui::Checkbox("Info", &mConsoleShowInfo);
		ImGui::SameLine();
		ImGui::Checkbox("Warning", &mConsoleShowWarn);
		ImGui::SameLine();
		ImGui::Checkbox("Error", &mConsoleShowError);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputTextWithHint("##filter", "Filter", &mConsoleFilter);
		ImGui::Separator();

		ImGui::BeginChild("log", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		std::string filter = ToLower(mConsoleFilter);
		Log::ForEach([&](const LogEntry& e)
			{
				if ((e.level == LogLevel::Info && !mConsoleShowInfo) || (e.level == LogLevel::Warning && !mConsoleShowWarn) ||
					(e.level == LogLevel::Error && !mConsoleShowError)) { return; }
				if (!filter.empty() && ToLower(e.message).find(filter) == std::string::npos) { return; }
				ImVec4 color = e.level == LogLevel::Error ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
					: (e.level == LogLevel::Warning ? ImVec4(1.0f, 0.8f, 0.3f, 1.0f) : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
				ImGui::TextDisabled("%8.2f", e.time);
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextUnformatted(e.message.c_str());
				ImGui::PopStyleColor();
			});
		// 新しいログが来たら一番下へ
		if (Log::GetVersion() != mConsoleVersion)
		{
			mConsoleVersion = Log::GetVersion();
			ImGui::SetScrollHereY(1.0f);
		}
		ImGui::EndChild();
		ImGui::End();
	}

	// ==================================================
	// Stats
	// ==================================================
	void Editor::DrawStats()
	{
		ImGui::Begin("Stats", &mShowStats);
		Engine& engine = Engine::Get();
		auto& device = GraphicsDevice::Get();
		const auto& rs = Renderer::Get().GetStats();
		Scene& scene = engine.GetScene();

		ImGui::Text("FPS %.1f  (%.2f ms)", engine.GetFps(), engine.GetFrameMs());
		float maxMs = 1.0f;
		for (float t : mFrameTimes) { maxMs = std::max(maxMs, t); }
		ImGui::PlotLines("##frame", mFrameTimes.data(), int(mFrameTimes.size()), 0, nullptr, 0.0f, maxMs * 1.2f, ImVec2(-1, 60));

		if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("GPU: %s", device.GetAdapterName().c_str());
			ImGui::Text("Viewport: %u x %u", mViewportWidth, mViewportHeight);
			ImGui::Text("Draw calls: %u   Triangles: %u", rs.drawCalls, rs.triangles);
			ImGui::Text("Meshes: %u visible / %u culled", rs.meshesVisible, rs.meshesCulled);
			ImGui::Text("Shadow casters: %u   Lights: %u", rs.shadowCasters, rs.lights);
			ImGui::Text("Particles: %u   Sprites: %u", rs.particles, rs.sprites);
			ImGui::Text("Dynamic upload: %.1f KB/frame", double(device.GetDynamicBytesUsed()) / 1024.0);
			ImGui::Text("Descriptors: %u / %u", device.GetSrvHeap().GetUsedCount(), device.GetSrvHeap().GetCapacity());
			bool vsync = device.GetVSync();
			if (ImGui::Checkbox("VSync", &vsync)) { device.SetVSync(vsync); }
		}
		if (ImGui::CollapsingHeader("Systems", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Job workers: %u   Pending jobs: %u", JobSystem::GetWorkerCount(), JobSystem::GetPendingCount());
			auto assets = AssetManager::Get().GetAllAssets();
			ImGui::Text("Assets: %zu (loading %u)", assets.size(), AssetManager::Get().GetLoadingCount());
			ImGui::Text("Actors: %zu", scene.GetActorCount());
			ImGui::Text("Physics: %u colliders, %u contacts", scene.GetPhysics().GetBodyCount(), scene.GetPhysics().GetContactCount());
			ImGui::Text("Time: %.2f s  (scale %.2f)", Time::TotalTime(), Time::TimeScale());
		}
		if (ImGui::CollapsingHeader("Loaded Assets"))
		{
			for (auto& a : AssetManager::Get().GetAllAssets())
			{
				const char* state = a->IsReady() ? "OK" : (a->IsLoading() ? "..." : "FAIL");
				ImGui::Text("[%s] %-9s %s", state, a->GetTypeName(), a->GetPath().c_str());
			}
		}
		ImGui::End();
	}

	// ==================================================
	// Render Settings
	// ==================================================
	void Editor::DrawRenderSettings()
	{
		ImGui::Begin("Render Settings", &mShowRenderSettings);
		ImGuiInspector inspector;
		if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
		{
			Renderer::Get().GetSettings().Reflect(inspector);
		}
		inspector.ResetFrameState();

		Scene& scene = Engine::Get().GetScene();
		if (ImGui::CollapsingHeader("Environment (Scene)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			scene.GetEnvironment().Reflect(inspector);
		}
		if (ImGui::CollapsingHeader("Physics / Time"))
		{
			inspector.Property("Gravity", scene.GetPhysics().gravity);
			inspector.Property("Solver Iterations", scene.GetPhysics().solverIterations, { .speed = 0.1f, .min = 1.0f, .max = 50.0f });
			float timeScale = Time::TimeScale();
			if (inspector.Property("Time Scale", timeScale, { .speed = 0.01f, .min = 0.0f, .max = 10.0f })) { Time::SetTimeScale(timeScale); }
		}
		if (inspector.AnyCommitted()) { Commit("Environment"); }
		ImGui::End();
	}
}
