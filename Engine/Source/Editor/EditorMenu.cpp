#include "RdPch.h"
#include "Editor/Editor.h"
#include "Asset/AssetManager.h"
#include "Editor/Assistant/AIAssistant.h"
#include "Editor/Core/EditorLocale.h"
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
		if (ImGui::BeginMenu(Tr("File")))
		{
			if (ImGui::MenuItem(Tr("New Scene"))) { NewScene(); }
			if (ImGui::MenuItem(Tr("Open Scene..."), "Ctrl+O")) { OpenScene(); }
			if (ImGui::MenuItem(Tr("Save Scene"), "Ctrl+S")) { SaveScene(false); }
			if (ImGui::MenuItem(Tr("Save Scene As..."), "Ctrl+Shift+S")) { SaveScene(true); }
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Reload Shaders"), "Ctrl+R")) { Renderer::Get().ReloadShaders(); }
			if (ImGui::MenuItem(Tr("Unload Unused Assets")))
			{
				Log::Info(LogCat::Editor, "Unloaded {} assets", AssetManager::Get().CollectGarbage());
			}
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Exit"))) { RequestQuit(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(Tr("Edit")))
		{
			bool hasSelection = GetSelected() != nullptr;
			std::string undo = EditorLocale::Format("Undo {}", mHistory.GetUndoLabel());
			std::string redo = EditorLocale::Format("Redo {}", mHistory.GetRedoLabel());
			if (ImGui::MenuItem(undo.c_str(), "Ctrl+Z", false, mHistory.CanUndo() && !IsPlaying())) { Undo(); }
			if (ImGui::MenuItem(redo.c_str(), "Ctrl+Y", false, mHistory.CanRedo() && !IsPlaying())) { Redo(); }
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Copy"), "Ctrl+C", false, hasSelection)) { CopySelected(); }
			if (ImGui::MenuItem(Tr("Paste"), "Ctrl+V", false, HasClipboard())) { Defer([this] { PasteClipboard(); }); }
			if (ImGui::MenuItem(Tr("Duplicate"), "Ctrl+D", false, hasSelection)) { Defer([this] { DuplicateSelected(); }); }
			if (ImGui::MenuItem(Tr("Rename"), "F2", false, hasSelection)) { RenameSelected(); }
			if (ImGui::MenuItem(Tr("Delete"), "Del", false, hasSelection)) { Defer([this] { DeleteSelected(); }); }
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Frame Selected"), "F", false, hasSelection)) { FocusOn(GetSelected()); }
			ImGui::Separator();
			if (ImGui::MenuItem(Tr(IsPlaying() ? "Stop" : "Play"), "Ctrl+P")) { TogglePlay(); }
			if (ImGui::MenuItem(Tr("Pause"), "Ctrl+Shift+P", mEngine.IsPaused(), IsPlaying())) { TogglePause(); }
			if (ImGui::MenuItem(Tr("Step"), "Ctrl+Alt+P", false, IsPlaying())) { StepFrame(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(Tr("Actor")))
		{
			DrawCreateMenu(nullptr);
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Create Prefab from Selection"), nullptr, false, GetSelected() != nullptr))
			{
				CreatePrefab(GetSelected(), mProject->GetCurrentFolder());
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(Tr("Window")))
		{
			for (auto& panel : mPanels) { ImGui::MenuItem(Tr(panel->GetTitle()), panel->GetShortcut(), panel->GetOpenFlag()); }
			ImGui::Separator();
			Window& window = mEngine.GetWindow();
			if (ImGui::MenuItem(Tr("Fullscreen"), "F11", window.IsFullscreen())) { window.ToggleFullscreen(); }
			if (ImGui::MenuItem(Tr("Reset Layout")))
			{
				for (auto& panel : mPanels) { panel->SetOpen(true); }
				mFirstLayout = true;
			}
			ImGui::MenuItem(Tr("ImGui Demo"), nullptr, &mShowDemo);
			ImGui::Separator();
			DrawLanguageMenu();
			ImGui::EndMenu();
		}

		std::string fps = std::format("{:.0f} FPS  {:.2f} ms", mEngine.GetFps(), mEngine.GetFrameMs());
		ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(fps.c_str()).x - 16.0f);
		ImGui::TextDisabled("%s", fps.c_str());
		ImGui::EndMainMenuBar();
	}

	// Window > Language。言語の名前はどちらの言語でも読めるよう、その言語で書く。
	//  切り替えは次のフレームの頭で行う（このフレームで受け取った訳の文字列を使い終わってから表を作り直す）
	void Editor::DrawLanguageMenu()
	{
		if (!ImGui::BeginMenu(Tr("Language"))) { return; }
		using Lang = EditorLocale::Language;
		Lang current = EditorLocale::GetLanguage();
		struct Item { const char* name; Lang lang; };
		for (Item item : { Item{ "English", Lang::English }, Item{ "日本語", Lang::Japanese } })
		{
			if (ImGui::MenuItem(item.name, nullptr, current == item.lang))
			{
				Lang lang = item.lang;
				Defer([lang] { EditorLocale::SetLanguage(lang); });
			}
		}
		ImGui::Separator();
		// 訳の表（Localization/*.json）を直したときに読み直す
		if (ImGui::MenuItem(Tr("Reload Translations"))) { Defer([] { EditorLocale::Reload(); }); }
		ImGui::EndMenu();
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
			if (EditorGui::IconButton("##move", Icon::Move, Tr("Move (W)"), view.GetGizmoOp() == Op::Move)) { view.SetGizmoOp(Op::Move); }
			ImGui::SameLine(0.0f, 2.0f);
			if (EditorGui::IconButton("##rotate", Icon::Rotate, Tr("Rotate (E)"), view.GetGizmoOp() == Op::Rotate)) { view.SetGizmoOp(Op::Rotate); }
			ImGui::SameLine(0.0f, 2.0f);
			if (EditorGui::IconButton("##scale", Icon::Scale, Tr("Scale (R)"), view.GetGizmoOp() == Op::Scale)) { view.SetGizmoOp(Op::Scale); }
			EditorGui::ToolbarSeparator();
			bool local = view.IsGizmoLocal();
			if (EditorGui::IconButton("##space", local ? Icon::Local : Icon::Global, Tr(local ? "Local (click for Global)" : "Global (click for Local)")))
			{
				view.SetGizmoLocal(!local);
			}

			float buttons = ImGui::GetFrameHeight() * 3.0f + 4.0f;
			ImGui::SameLine((ImGui::GetWindowWidth() - buttons) * 0.5f);
			bool playing = IsPlaying();
			if (EditorGui::IconButton("##play", Icon::Play, Tr(playing ? "Stop (Ctrl+P)" : "Play (Ctrl+P)"), playing)) { Defer([this] { TogglePlay(); }); }
			ImGui::SameLine(0.0f, 2.0f);
			if (EditorGui::IconButton("##pause", Icon::Pause, Tr("Pause (Ctrl+Shift+P)"), mEngine.IsPaused(), playing)) { TogglePause(); }
			ImGui::SameLine(0.0f, 2.0f);
			if (EditorGui::IconButton("##step", Icon::Step, Tr("Step (Ctrl+Alt+P)"), false, playing)) { StepFrame(); }
		}
		ImGui::End();
	}

	void Editor::DrawDialogs()
	{
		if (mOpenSavePrompt)
		{
			ImGui::OpenPopup(TrId("Save Changes?"));
			mOpenSavePrompt = false;
		}
		const char* const kButtons[] = { Tr("Save"), Tr("Don't Save"), Tr("Cancel") };
		std::string message = EditorLocale::Format("Save the changes to \"{}\"?",
			GetScene().GetPath().empty() ? std::string(Tr("Untitled")) : GetScene().GetPath());
		int choice = -1;
		if (!EditorGui::ConfirmPopup(TrId("Save Changes?"), message.c_str(), kButtons, 3, choice)) { return; }
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
