#include "RdPch.h"
#include "Editor/EditorMainUI.h"
#include "Asset/AssetManager.h"
#include "Editor/Assistant/AIAssistant.h"
#include "Editor/Commands/EditorUndo.h"
#include "Editor/Core/ActorActions.h"
#include "Editor/Core/EditorLayout.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Core/EditorSelection.h"
#include "Editor/Core/HotReload.h"
#include "Editor/Core/PlayMode.h"
#include "Editor/Core/SceneDocument.h"
#include "Editor/Editor.h"
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
	void EditorMainUI::DrawBars()
	{
		DrawMenuBar();
		DrawMainToolbar();
		DrawStatusBar();
	}

	void EditorMainUI::DrawDemoWindow()
	{
		if (mShowDemo) { ImGui::ShowDemoWindow(&mShowDemo); }
	}

	void EditorMainUI::DrawMenuBar()
	{
		if (!ImGui::BeginMainMenuBar()) { return; }
		Editor& editor = mEditor;
		SceneDocument& document = editor.GetDocument();
		ActorActions& actions = editor.GetActions();
		EditorUndo& undo = editor.GetUndo();
		PlayMode& play = editor.GetPlayMode();
		HotReload& hot = editor.GetHotReload();
		EditorStyle::DrawLogo();
		if (ImGui::BeginMenu(Tr("File")))
		{
			if (ImGui::MenuItem(Tr("New Scene"))) { document.NewScene(); }
			if (ImGui::MenuItem(Tr("Open Scene..."), "Ctrl+O")) { document.OpenScene(); }
			if (ImGui::MenuItem(Tr("Save Scene"), "Ctrl+S")) { document.SaveScene(false); }
			if (ImGui::MenuItem(Tr("Save Scene As..."), "Ctrl+Shift+S")) { document.SaveScene(true); }
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Reload Shaders"), "Ctrl+R")) { Renderer::Get().ReloadShaders(); }
			if (ImGui::MenuItem(Tr("Compile Game Code"), "Ctrl+Alt+F11", false, hot.IsAvailable() && !hot.IsCompiling())) { hot.Compile(); }
			if (ImGui::MenuItem(Tr("Unload Unused Assets")))
			{
				Logger::Info(LogCat::Editor, "Unloaded {} assets", AssetManager::Get().CollectGarbage());
			}
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Exit"))) { document.RequestQuit(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(Tr("Edit")))
		{
			Actor* selected = editor.GetSelection().Get();
			bool hasSelection = selected != nullptr;
			bool playing = play.IsPlaying();
			const CommandHistory& history = undo.GetHistory();
			std::string undoLabel = EditorLocale::Format("Undo {}", history.GetUndoLabel());
			std::string redoLabel = EditorLocale::Format("Redo {}", history.GetRedoLabel());
			if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, history.CanUndo() && !playing)) { undo.Undo(); }
			if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, history.CanRedo() && !playing)) { undo.Redo(); }
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Copy"), "Ctrl+C", false, hasSelection)) { actions.CopySelected(); }
			if (ImGui::MenuItem(Tr("Paste"), "Ctrl+V", false, actions.HasClipboard())) { editor.Defer([&actions] { actions.PasteClipboard(); }); }
			if (ImGui::MenuItem(Tr("Duplicate"), "Ctrl+D", false, hasSelection)) { editor.Defer([&actions] { actions.DuplicateSelected(); }); }
			if (ImGui::MenuItem(Tr("Rename"), "F2", false, hasSelection)) { actions.RenameSelected(); }
			if (ImGui::MenuItem(Tr("Delete"), "Del", false, hasSelection)) { editor.Defer([&actions] { actions.DeleteSelected(); }); }
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Frame Selected"), "F", false, hasSelection)) { actions.FocusOn(selected); }
			ImGui::Separator();
			if (ImGui::MenuItem(Tr(playing ? "Stop" : "Play"), "Ctrl+P")) { play.Toggle(); }
			if (ImGui::MenuItem(Tr("Pause"), "Ctrl+Shift+P", play.IsPaused(), playing)) { play.TogglePause(); }
			if (ImGui::MenuItem(Tr("Step"), "Ctrl+Alt+P", false, playing)) { play.Step(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(Tr("Actor")))
		{
			actions.DrawCreateMenu(nullptr);
			ImGui::Separator();
			Actor* selected = editor.GetSelection().Get();
			if (ImGui::MenuItem(Tr("Create Prefab from Selection"), nullptr, false, selected != nullptr))
			{
				actions.CreatePrefab(selected, editor.GetProject().GetCurrentFolder());
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(Tr("Window")))
		{
			for (auto& panel : editor.GetPanels()) { ImGui::MenuItem(Tr(panel->GetTitle()), panel->GetShortcut(), panel->GetOpenFlag()); }
			ImGui::Separator();
			Window& window = editor.GetEngine().GetWindow();
			if (ImGui::MenuItem(Tr("Fullscreen"), "F11", window.IsFullscreen())) { window.ToggleFullscreen(); }
			if (ImGui::MenuItem(Tr("Reset Layout")))
			{
				for (auto& panel : editor.GetPanels()) { panel->SetOpen(true); }
				editor.GetLayout().Reset();
			}
			ImGui::MenuItem(Tr("ImGui Demo"), nullptr, &mShowDemo);
			ImGui::Separator();
			DrawLanguageMenu();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// 一番下の細い帯: 左に今の状態（編集中・再生中・一時停止）とシーンの保存状態、右に選んでいるものと FPS
	void EditorMainUI::DrawStatusBar()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		float font = ImGui::GetFontSize();
		float height = IM_ROUND(font * 1.55f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;
		ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorStyle::kBaseBg);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.WindowPadding.x * 1.2f, 0.0f));
		bool open = ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, height, flags);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		if (open)
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 wp = ImGui::GetWindowPos();
			float cy = wp.y + height * 0.5f;
			float textY = IM_ROUND(cy - font * 0.5f);
			float x = wp.x + style.WindowPadding.x * 1.2f;
			const ImU32 dim = ImGui::ColorConvertFloat4ToU32(EditorStyle::kTextDim);

			// 状態の丸い印（再生中は緑、一時停止は黄色、編集中は差し色）と名前
			const PlayMode& play = mEditor.GetPlayMode();
			bool playing = play.IsPlaying(), paused = play.IsPaused();
			ImVec4 stateColor = playing ? (paused ? EditorStyle::kWarning : EditorStyle::kPlay) : EditorStyle::kAccent;
			const char* state = Tr(playing ? (paused ? "Paused" : "Playing") : "Editing");
			float r = font * 0.22f;
			dl->AddCircleFilled(ImVec2(x + r, cy), r * 2.2f, ImGui::ColorConvertFloat4ToU32(ImVec4(stateColor.x, stateColor.y, stateColor.z, 0.18f)), 20);
			dl->AddCircleFilled(ImVec2(x + r, cy), r, ImGui::ColorConvertFloat4ToU32(stateColor), 20);
			x += r * 2.0f + font * 0.55f;
			dl->AddText(ImVec2(x, textY), ImGui::ColorConvertFloat4ToU32(EditorStyle::kText), state);
			x += ImGui::CalcTextSize(state).x + font * 0.9f;
			dl->AddRectFilled(ImVec2(x, cy - font * 0.4f), ImVec2(x + 1.0f, cy + font * 0.4f), ImGui::GetColorU32(ImGuiCol_Border));
			x += font * 0.9f;

			// シーンの名前と、保存していない変更があるか
			const std::string& name = mEditor.GetScene().GetName();
			std::string scene = name.empty() ? std::string(Tr("Untitled")) : name;
			dl->AddText(ImVec2(x, textY), dim, scene.c_str());
			x += ImGui::CalcTextSize(scene.c_str()).x + font * 0.6f;
			bool dirty = mEditor.GetUndo().IsDirty();
			ImVec4 saveColor = dirty ? EditorStyle::kWarning : EditorStyle::kTextDim;
			const char* save = Tr(dirty ? "Unsaved changes" : "Saved");
			if (dirty) { dl->AddCircleFilled(ImVec2(x + r * 0.8f, cy), r * 0.8f, ImGui::ColorConvertFloat4ToU32(saveColor), 12); x += r * 1.6f + font * 0.4f; }
			dl->AddText(ImVec2(x, textY), ImGui::ColorConvertFloat4ToU32(saveColor), save);

			// 右から: FPS、区切り、選んでいるもの
			const Engine& engine = mEditor.GetEngine();
			std::string fps = std::format("{:.0f} FPS  {:.2f} ms", engine.GetFps(), engine.GetFrameMs());
			float fpsWidth = ImGui::CalcTextSize(fps.c_str()).x;
			float right = wp.x + ImGui::GetWindowWidth() - style.WindowPadding.x * 1.2f;
			dl->AddText(ImVec2(right - fpsWidth, textY), dim, fps.c_str());
			if (Actor* selected = mEditor.GetSelection().Get())
			{
				float sx = right - fpsWidth - font * 0.9f;
				dl->AddRectFilled(ImVec2(sx, cy - font * 0.4f), ImVec2(sx + 1.0f, cy + font * 0.4f), ImGui::GetColorU32(ImGuiCol_Border));
				const std::string& sel = selected->GetName();
				float selWidth = ImGui::CalcTextSize(sel.c_str()).x;
				float iconSize = font * 0.9f;
				float tx = sx - font * 0.9f - selWidth;
				dl->AddText(ImVec2(tx, textY), ImGui::ColorConvertFloat4ToU32(EditorStyle::kText), sel.c_str());
				EditorIcons::Draw(dl, ImVec2(tx - iconSize - font * 0.35f, cy - iconSize * 0.5f), iconSize, EditorIcons::ForActor(selected));
			}
			// 上の縁に、パネルとの境目の細い線
			dl->AddRectFilled(wp, ImVec2(wp.x + ImGui::GetWindowWidth(), wp.y + 1.0f), ImGui::GetColorU32(ImGuiCol_Border));
		}
		ImGui::End();
	}

	// Window > Language。言語の名前はどちらの言語でも読めるよう、その言語で書く。
	//  切り替えは次のフレームの頭で行う（このフレームで受け取った訳の文字列を使い終わってから表を作り直す）
	void EditorMainUI::DrawLanguageMenu()
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
				mEditor.Defer([lang] { EditorLocale::SetLanguage(lang); });
			}
		}
		ImGui::Separator();
		// 訳の表（Localization/*.json）を直したときに読み直す
		if (ImGui::MenuItem(Tr("Reload Translations"))) { mEditor.Defer([] { EditorLocale::Reload(); }); }
		ImGui::EndMenu();
	}

	// メニューの下の帯（Unity と同じく、左にギズモの切り替え、真ん中に Play / Pause / Step）
	void EditorMainUI::DrawMainToolbar()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		float height = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;
		// メニューバーと同じ一番奥の色にして、上の帯をひと続きに見せる
		ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorStyle::kBaseBg);
		bool open = ImGui::BeginViewportSideBar("##MainToolbar", ImGui::GetMainViewport(), ImGuiDir_Up, height, flags);
		ImGui::PopStyleColor();
		if (open)
		{
			using Op = SceneViewPanel::GizmoOp;
			SceneViewPanel& view = mEditor.GetSceneView();
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

			// Play / Pause / Step は真ん中に、少し明るい台に乗せてまとめる
			float gap = 2.0f;
			float buttons = ImGui::GetFrameHeight() * 3.0f + gap * 2.0f;
			ImGui::SameLine((ImGui::GetWindowWidth() - buttons) * 0.5f);
			{
				ImVec2 p = ImGui::GetCursorScreenPos();
				float pad = ImGui::GetFontSize() * 0.2f;
				const ImVec4& bg = EditorStyle::kWindowBg;
				EditorStyle::DrawVerticalShade(ImGui::GetWindowDrawList(), ImVec2(p.x - pad, p.y - pad), ImVec2(p.x + buttons + pad, p.y + ImGui::GetFrameHeight() + pad),
					ImVec4(bg.x + 0.035f, bg.y + 0.037f, bg.z + 0.040f, 1.0f), bg);
			}
			PlayMode& play = mEditor.GetPlayMode();
			bool playing = play.IsPlaying();
			if (EditorGui::IconButton("##play", Icon::Play, Tr(playing ? "Stop (Ctrl+P)" : "Play (Ctrl+P)"), playing, true, &EditorStyle::kPlay))
			{
				mEditor.Defer([&play] { play.Toggle(); });
			}
			ImGui::SameLine(0.0f, gap);
			if (EditorGui::IconButton("##pause", Icon::Pause, Tr("Pause (Ctrl+Shift+P)"), play.IsPaused(), playing, &EditorStyle::kWarning)) { play.TogglePause(); }
			ImGui::SameLine(0.0f, gap);
			if (EditorGui::IconButton("##step", Icon::Step, Tr("Step (Ctrl+Alt+P)"), false, playing)) { play.Step(); }

			if (mEditor.GetHotReload().IsAvailable()) { DrawCompileButton(); }
		}
		ImGui::End();
	}

	// ツールバーの右端: ゲームのコードのビルドと読み直し（Unreal の Compile ボタンにあたる）。左に今の状態を出す
	void EditorMainUI::DrawCompileButton()
	{
		HotReload& hot = mEditor.GetHotReload();
		const char* label = Tr(hot.IsCompiling() ? "Compiling..." : "Compile");
		const char* status = hot.IsCompiling() ? "" : Tr(hot.GetStatusText());
		ImGuiStyle& style = ImGui::GetStyle();
		float buttonWidth = ImGui::GetFrameHeight() + ImGui::CalcTextSize(label).x + style.FramePadding.x * 3.0f;
		float statusWidth = status[0] ? ImGui::CalcTextSize(status).x + style.ItemSpacing.x : 0.0f;
		ImGui::SameLine(ImGui::GetWindowWidth() - buttonWidth - statusWidth - style.WindowPadding.x);
		if (status[0])
		{
			ImVec4 color = hot.GetState() == HotReload::State::Failed ? EditorStyle::kError : ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(color, "%s", status);
			ImGui::SameLine();
		}
		ImGui::BeginDisabled(hot.IsCompiling());
		if (EditorGui::IconTextButton("##compile", Icon::Rotate, label, Tr("Build the game code and reload it without closing the editor (Ctrl+Alt+F11)")))
		{
			hot.Compile();
		}
		ImGui::EndDisabled();
	}

	// 文字を入力している間は何もしない。W/E/R/F はシーンビューにマウスがあるときだけ（カメラ操作中の WASD とぶつかるため）
	void EditorMainUI::HandleShortcuts()
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput) { return; }
		bool ctrl = io.KeyCtrl;
		bool shift = io.KeyShift;
		bool alt = io.KeyAlt;
		auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };
		Editor& editor = mEditor;
		ActorActions& actions = editor.GetActions();
		PlayMode& play = editor.GetPlayMode();

		// フォーカスのあるパネル（行動ツリーなど）が編集のショートカットを受け持つなら、シーンには効かせない
		bool panelEdit = false;
		for (auto& panel : editor.GetPanels())
		{
			if (panel->IsOpen() && panel->HandleShortcuts()) { panelEdit = true; }
		}
		if (!panelEdit)
		{
			if (ctrl && pressed(ImGuiKey_Z)) { editor.GetUndo().Undo(); }
			if (ctrl && pressed(ImGuiKey_Y)) { editor.GetUndo().Redo(); }
			if (ctrl && pressed(ImGuiKey_S)) { editor.GetDocument().SaveScene(shift); }
			if (ctrl && pressed(ImGuiKey_D)) { editor.Defer([&actions] { actions.DuplicateSelected(); }); }
			if (ctrl && pressed(ImGuiKey_C)) { actions.CopySelected(); }
			if (ctrl && pressed(ImGuiKey_V)) { editor.Defer([&actions] { actions.PasteClipboard(); }); }
		}
		if (ctrl && pressed(ImGuiKey_O)) { editor.GetDocument().OpenScene(); }
		if (ctrl && !shift && pressed(ImGuiKey_R)) { Renderer::Get().ReloadShaders(); }
		if (ctrl && pressed(ImGuiKey_P))
		{
			if (alt) { play.Step(); }
			else if (shift) { play.TogglePause(); }
			else { editor.Defer([&play] { play.Toggle(); }); }
		}
		if (ctrl && alt && pressed(ImGuiKey_F11)) { editor.GetHotReload().Compile(); }
		if (pressed(ImGuiKey_F9)) { editor.GetProfiler().SetOpen(!editor.GetProfiler().IsOpen()); }
		if (pressed(ImGuiKey_F8)) { editor.GetAssistant().SetOpen(!editor.GetAssistant().IsOpen()); }
		if (!panelEdit && pressed(ImGuiKey_Delete) && ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) { editor.Defer([&actions] { actions.DeleteSelected(); }); }

		SceneViewPanel& view = editor.GetSceneView();
		if (!view.GetCamera().IsControlling() && !ctrl && (view.IsFocused() || view.IsHovered()))
		{
			if (pressed(ImGuiKey_W)) { view.SetGizmoOp(SceneViewPanel::GizmoOp::Move); }
			if (pressed(ImGuiKey_E)) { view.SetGizmoOp(SceneViewPanel::GizmoOp::Rotate); }
			if (pressed(ImGuiKey_R)) { view.SetGizmoOp(SceneViewPanel::GizmoOp::Scale); }
			if (pressed(ImGuiKey_F)) { actions.FocusOn(editor.GetSelection().Get()); }
		}
	}
}
