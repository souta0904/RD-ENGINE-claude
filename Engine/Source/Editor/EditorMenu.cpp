#include "RdPch.h"
#include "Editor/Editor.h"
#include "Asset/AssetManager.h"
#include "Editor/Assistant/AIAssistant.h"
#include "Editor/Panels/EditorPanel.h"
#include "Editor/Panels/ProfilerPanel.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Editor/Panels/SceneViewPanel.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Engine.h"
#include "Render/Renderer/Renderer.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

namespace rd
{
	void Editor::DrawMenuBar()
	{
		if (!ImGui::BeginMainMenuBar()) { return; }
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) { NewScene(); }
			if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) { OpenScene(); }
			if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { SaveScene(false); }
			if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) { SaveScene(true); }
			ImGui::Separator();
			if (ImGui::MenuItem("Reload Shaders", "Ctrl+R")) { Renderer::Get().ReloadShaders(); }
			if (ImGui::MenuItem("Unload Unused Assets"))
			{
				Log::Info(LogCat::Editor, "Unloaded {} assets", AssetManager::Get().CollectGarbage());
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) { RequestQuit(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			bool hasSelection = GetSelected() != nullptr;
			std::string undo = "Undo " + mHistory.GetUndoLabel();
			std::string redo = "Redo " + mHistory.GetRedoLabel();
			if (ImGui::MenuItem(undo.c_str(), "Ctrl+Z", false, mHistory.CanUndo() && !IsPlaying())) { Undo(); }
			if (ImGui::MenuItem(redo.c_str(), "Ctrl+Y", false, mHistory.CanRedo() && !IsPlaying())) { Redo(); }
			ImGui::Separator();
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasSelection)) { CopySelected(); }
			if (ImGui::MenuItem("Paste", "Ctrl+V", false, HasClipboard())) { Defer([this] { PasteClipboard(); }); }
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSelection)) { Defer([this] { DuplicateSelected(); }); }
			if (ImGui::MenuItem("Rename", "F2", false, hasSelection)) { RenameSelected(); }
			if (ImGui::MenuItem("Delete", "Del", false, hasSelection)) { Defer([this] { DeleteSelected(); }); }
			ImGui::Separator();
			if (ImGui::MenuItem("Frame Selected", "F", false, hasSelection)) { FocusOn(GetSelected()); }
			ImGui::Separator();
			if (ImGui::MenuItem(IsPlaying() ? "Stop" : "Play", "Ctrl+P")) { TogglePlay(); }
			if (ImGui::MenuItem("Pause", "Ctrl+Shift+P", mEngine.IsPaused(), IsPlaying())) { TogglePause(); }
			if (ImGui::MenuItem("Step", "Ctrl+Alt+P", false, IsPlaying())) { StepFrame(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Actor"))
		{
			DrawCreateMenu(nullptr);
			ImGui::Separator();
			if (ImGui::MenuItem("Create Prefab from Selection", nullptr, false, GetSelected() != nullptr))
			{
				CreatePrefab(GetSelected(), mProject->GetCurrentFolder());
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window"))
		{
			for (auto& panel : mPanels) { ImGui::MenuItem(panel->GetTitle(), panel->GetShortcut(), panel->GetOpenFlag()); }
			ImGui::Separator();
			Window& window = mEngine.GetWindow();
			if (ImGui::MenuItem("Fullscreen", "F11", window.IsFullscreen())) { window.ToggleFullscreen(); }
			if (ImGui::MenuItem("Reset Layout"))
			{
				for (auto& panel : mPanels) { panel->SetOpen(true); }
				mFirstLayout = true;
			}
			ImGui::MenuItem("ImGui Demo", nullptr, &mShowDemo);
			ImGui::EndMenu();
		}

		std::string fps = std::format("{:.0f} FPS  {:.2f} ms", mEngine.GetFps(), mEngine.GetFrameMs());
		ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(fps.c_str()).x - 16.0f);
		ImGui::TextDisabled("%s", fps.c_str());
		ImGui::EndMainMenuBar();
	}

	// メニューの下の帯（Unity と同じく、左にギズモの切り替え、真ん中に Play / Pause / Step）
	void Editor::DrawMainToolbar()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		float height = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;
		ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorStyle::kToolbarBg);
		bool open = ImGui::BeginViewportSideBar("##MainToolbar", ImGui::GetMainViewport(), ImGuiDir_Up, height, flags);
		ImGui::PopStyleColor();
		if (open)
		{
			using Op = SceneViewPanel::GizmoOp;
			SceneViewPanel& view = *mSceneView;
			if (EditorGui::IconButton("##move", Icon::Move, "Move (W)", view.GetGizmoOp() == Op::Move)) { view.SetGizmoOp(Op::Move); }
			ImGui::SameLine(0.0f, 2.0f);
			if (EditorGui::IconButton("##rotate", Icon::Rotate, "Rotate (E)", view.GetGizmoOp() == Op::Rotate)) { view.SetGizmoOp(Op::Rotate); }
			ImGui::SameLine(0.0f, 2.0f);
			if (EditorGui::IconButton("##scale", Icon::Scale, "Scale (R)", view.GetGizmoOp() == Op::Scale)) { view.SetGizmoOp(Op::Scale); }
			EditorGui::ToolbarSeparator();
			bool local = view.IsGizmoLocal();
			if (EditorGui::IconButton("##space", local ? Icon::Local : Icon::Global, local ? "Local (click for Global)" : "Global (click for Local)"))
			{
				view.SetGizmoLocal(!local);
			}

			float buttons = ImGui::GetFrameHeight() * 3.0f + 4.0f;
			ImGui::SameLine((ImGui::GetWindowWidth() - buttons) * 0.5f);
			bool playing = IsPlaying();
			if (EditorGui::IconButton("##play", Icon::Play, playing ? "Stop (Ctrl+P)" : "Play (Ctrl+P)", playing)) { Defer([this] { TogglePlay(); }); }
			ImGui::SameLine(0.0f, 2.0f);
			if (EditorGui::IconButton("##pause", Icon::Pause, "Pause (Ctrl+Shift+P)", mEngine.IsPaused(), playing)) { TogglePause(); }
			ImGui::SameLine(0.0f, 2.0f);
			if (EditorGui::IconButton("##step", Icon::Step, "Step (Ctrl+Alt+P)", false, playing)) { StepFrame(); }
		}
		ImGui::End();
	}

	void Editor::DrawDialogs()
	{
		if (mOpenSavePrompt)
		{
			ImGui::OpenPopup("Save Changes?");
			mOpenSavePrompt = false;
		}
		static const char* const kButtons[] = { "Save", "Don't Save", "Cancel" };
		std::string message = std::format("Save the changes to \"{}\"?",
			GetScene().GetPath().empty() ? std::string("Untitled") : GetScene().GetPath());
		int choice = -1;
		if (!EditorGui::ConfirmPopup("Save Changes?", message.c_str(), kButtons, 3, choice)) { return; }
		auto action = std::move(mPendingAfterSave);
		mPendingAfterSave = nullptr;
		if (choice == 0 && !SaveScene(false)) { return; }
		if ((choice == 0 || choice == 1) && action) { Defer(std::move(action)); }
	}

	// 文字を入力している間は何もしない。W/E/R/F はシーンビューにマウスがあるときだけ（カメラ操作中の WASD とぶつかるため）
	void Editor::HandleShortcuts()
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput) { return; }
		bool ctrl = io.KeyCtrl;
		bool shift = io.KeyShift;
		bool alt = io.KeyAlt;
		auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };

		if (ctrl && pressed(ImGuiKey_Z)) { Undo(); }
		if (ctrl && pressed(ImGuiKey_Y)) { Redo(); }
		if (ctrl && pressed(ImGuiKey_S)) { SaveScene(shift); }
		if (ctrl && pressed(ImGuiKey_O)) { OpenScene(); }
		if (ctrl && pressed(ImGuiKey_D)) { Defer([this] { DuplicateSelected(); }); }
		if (ctrl && pressed(ImGuiKey_C)) { CopySelected(); }
		if (ctrl && pressed(ImGuiKey_V)) { Defer([this] { PasteClipboard(); }); }
		if (ctrl && !shift && pressed(ImGuiKey_R)) { Renderer::Get().ReloadShaders(); }
		if (ctrl && pressed(ImGuiKey_P))
		{
			if (alt) { StepFrame(); }
			else if (shift) { TogglePause(); }
			else { Defer([this] { TogglePlay(); }); }
		}
		if (pressed(ImGuiKey_F9)) { mProfiler->SetOpen(!mProfiler->IsOpen()); }
		if (pressed(ImGuiKey_F8)) { mAssistant->SetOpen(!mAssistant->IsOpen()); }
		if (pressed(ImGuiKey_Delete) && ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) { Defer([this] { DeleteSelected(); }); }

		SceneViewPanel& view = *mSceneView;
		if (!view.GetCamera().IsControlling() && !ctrl && (view.IsFocused() || view.IsHovered()))
		{
			if (pressed(ImGuiKey_W)) { view.SetGizmoOp(SceneViewPanel::GizmoOp::Move); }
			if (pressed(ImGuiKey_E)) { view.SetGizmoOp(SceneViewPanel::GizmoOp::Rotate); }
			if (pressed(ImGuiKey_R)) { view.SetGizmoOp(SceneViewPanel::GizmoOp::Scale); }
			if (pressed(ImGuiKey_F)) { FocusOn(GetSelected()); }
		}
	}
}
