#include "RdPch.h"
#include "Editor/Editor.h"
#include "Asset/AssetManager.h"
#include "Asset/AudioClip.h"
#include "Asset/Texture.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Serialization/JsonReader.h"
#include "Core/Serialization/JsonWriter.h"
#include "Core/Util/StringUtil.h"
#include "Core/Util/FileUtil.h"
#include "Core/Time/Time.h"
#include "Editor/ImGuiInspector.h"
#include "Engine.h"
#include "Physics/PhysicsWorld.h"
#include "Render/Renderer.h"
#include "RHI/GraphicsDevice.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	namespace
	{
		const char* FileTag(const std::string& ext)
		{
			if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".dds" || ext == ".hdr") { return "IMG"; }
			if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx") { return "MDL"; }
			if (AudioClip::IsSupportedExtension(ext)) { return "SND"; }
			if (ext == ".scene") { return "SCN"; }
			if (ext == ".prefab") { return "PFB"; }
			if (ext == ".hlsl" || ext == ".hlsli") { return "SHD"; }
			return "FILE";
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

		ImGuiInspector inspector(this);
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
		Component* toReset = nullptr;
		Component* toMove = nullptr;// 並べ替え（ドラッグ・右クリックメニュー）
		int moveIndex = -1;// 動かした後の位置
		bool dropLine = false;// ドロップ位置の線
		float dropLineY = 0.0f, dropLineX0 = 0.0f, dropLineX1 = 0.0f;
		const auto& components = actor->GetAllComponents();
		const int compCount = int(components.size());
		for (int index = 0; index < compCount; ++index)
		{
			Component* c = components[size_t(index)].get();
			ImGui::PushID(c);
			bool open = ImGui::CollapsingHeader(c->GetTypeName(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
			const ImVec2 headerMin = ImGui::GetItemRectMin(), headerMax = ImGui::GetItemRectMax();

			// ヘッダをドラッグして並べ替え（上半分へのドロップで前、下半分で後ろ）
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("COMPONENT_INDEX", &index, sizeof(index));
				ImGui::Text("%s", c->GetTypeName());
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget())
			{
				const ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
				if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("COMPONENT_INDEX", flags))
				{
					const int from = *static_cast<const int*>(p->Data);
					const bool after = ImGui::GetMousePos().y > (headerMin.y + headerMax.y) * 0.5f;
					int to = index + (after ? 1 : 0);// 動かす前の並びでの挿入位置
					if (to > from) { --to; }// 自分が抜けた分だけ前に詰まる
					if (from >= 0 && from < compCount && to != from)
					{
						dropLine = true;
						dropLineY = after ? headerMax.y : headerMin.y;
						dropLineX0 = headerMin.x;
						dropLineX1 = headerMax.x;
						if (p->IsDelivery())
						{
							toMove = components[size_t(from)].get();
							moveIndex = to;
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::BeginPopupContextItem("comp"))
			{
				if (ImGui::MenuItem("Remove")) { toRemove = c; }
				if (ImGui::MenuItem("Reset")) { toReset = c; }
				ImGui::Separator();
				if (ImGui::MenuItem("Move Up", nullptr, false, index > 0)) { toMove = c; moveIndex = index - 1; }
				if (ImGui::MenuItem("Move Down", nullptr, false, index < compCount - 1)) { toMove = c; moveIndex = index + 1; }
				if (ImGui::MenuItem("Move to Top", nullptr, false, index > 0)) { toMove = c; moveIndex = 0; }
				if (ImGui::MenuItem("Move to Bottom", nullptr, false, index < compCount - 1)) { toMove = c; moveIndex = compCount - 1; }
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
		if (dropLine)
		{
			ImGui::GetWindowDrawList()->AddLine(ImVec2(dropLineX0, dropLineY), ImVec2(dropLineX1, dropLineY), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
		}

		std::string label = "Edit " + actor->GetName();
		if (toRemove)
		{
			label = std::format("Remove {}", toRemove->GetTypeName());
			actor->RemoveComponent(toRemove);
			mEngine.GetScene().FlushPendingDestroy();
			commit = true;
		}
		if (toMove && actor->SetComponentIndex(toMove, moveIndex))
		{
			label = std::format("Move {}", toMove->GetTypeName());
			commit = true;
		}
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
				label = std::format("Reset {}", toReset->GetTypeName());
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
					label = "Add " + entry.name;
					commit = true;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}

		if (commit) { CommitActorEdit(label); }
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
		ActorId prefabActor = 0;// ドロップされたアクター（プレハブにする）
		std::string prefabDir;

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
					else if (ext == ".scene") { mEngine.LoadScene(path); }
				}
				(void)clicked;
				// フォルダへアクターをドロップしたら、そのフォルダにプレハブを作る
				if (isDir && ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID"))
					{
						prefabActor = *static_cast<const ActorId*>(p->Data);
						prefabDir = path;
					}
					ImGui::EndDragDropTarget();
				}
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

		// パネル全体へのドロップ: 今開いているフォルダにプレハブを作る（フォルダの上なら、そちらが優先される）
		if (ImGuiWindow* window = ImGui::GetCurrentWindow(); ImGui::BeginDragDropTargetCustom(window->InnerRect, window->ID))
		{
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID"))
			{
				prefabActor = *static_cast<const ActorId*>(p->Data);
				prefabDir = mAssetDir;
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextWindow("assetsMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Prefab from Selection", nullptr, false, GetSelected() != nullptr))
			{
				prefabActor = mSelectedId;
				prefabDir = mAssetDir;
			}
			ImGui::EndPopup();
		}
		ImGui::EndChild();

		if (prefabActor != 0) { CreatePrefab(mEngine.GetScene().FindActor(prefabActor), prefabDir); }
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
		ImGui::Checkbox("Debug", &mConsoleShowDebug);
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
		Log::ForEach([&](const LogRecord& e)
			{
				bool show = e.level <= LogLevel::Debug ? mConsoleShowDebug
					: e.level == LogLevel::Info ? mConsoleShowInfo
					: e.level == LogLevel::Warning ? mConsoleShowWarn
					: mConsoleShowError;
				if (!show) { return; }
				if (!filter.empty() && ToLower(e.message).find(filter) == std::string::npos &&
					ToLower(e.category).find(filter) == std::string::npos) { return; }
				ImVec4 color = e.level >= LogLevel::Error ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
					: e.level == LogLevel::Warning ? ImVec4(1.0f, 0.8f, 0.3f, 1.0f)
					: e.level == LogLevel::Info ? ImVec4(0.85f, 0.85f, 0.85f, 1.0f)
					: ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
				ImGui::TextDisabled("%8.2f %-8s", e.time, e.category);
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextUnformatted(e.message.c_str());
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s  %s:%u", ToString(e.level), e.location.file_name(), unsigned(e.location.line()));
				}
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
		Engine& engine = mEngine;
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
			auto& physics = scene.GetSystem<PhysicsWorld>();
			ImGui::Text("Physics: %u colliders, %u contacts", physics.GetBodyCount(), physics.GetContactCount());
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
		ImGuiInspector inspector(this);
		if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
		{
			Renderer::Get().GetSettings().Reflect(inspector);
		}
		inspector.ResetFrameState();

		Scene& scene = mEngine.GetScene();
		if (ImGui::CollapsingHeader("Environment (Scene)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			scene.GetEnvironment().Reflect(inspector);
		}
		if (ImGui::CollapsingHeader("Physics / Time"))
		{
			Vector3 gravity = scene.GetGravity();
			if (inspector.Property("Gravity", gravity)) { scene.SetGravity(gravity); }
			inspector.Property("Solver Iterations", scene.GetSystem<PhysicsWorld>().solverIterations, { .speed = 0.1f, .min = 1.0f, .max = 50.0f });
			float timeScale = Time::TimeScale();
			if (inspector.Property("Time Scale", timeScale, { .speed = 0.01f, .min = 0.0f, .max = 10.0f })) { Time::SetTimeScale(timeScale); }
		}
		if (inspector.AnyCommitted()) { CommitSettingsEdit("Environment"); }
		ImGui::End();
	}
}
