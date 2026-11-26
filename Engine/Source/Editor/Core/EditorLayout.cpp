#include "RdPch.h"
#include "Editor/Core/EditorLayout.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Editor.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <fstream>

namespace rd
{
	namespace
	{
		constexpr const char* kLayoutFile = "Logs/editor_layout.ini";
		// 既定の配置の版（パネルを足したり名前を変えたりしたら上げる。保存してある版と違えば配置を作り直す）
		constexpr const char* kLayoutVersionFile = "Logs/editor_layout.version";
		//  3: ウィンドウの ID を「訳###英語」にした（言語を変えても配置が保たれるように）
		constexpr int kLayoutVersion = 3;

		int ReadLayoutVersion()
		{
			std::ifstream file(kLayoutVersionFile);
			int version = 0;
			file >> version;
			return version;
		}

		void WriteLayoutVersion()
		{
			std::ofstream file(kLayoutVersionFile);
			file << kLayoutVersion;
		}
	}

	// 前回の配置が保存されていればそれを使う（既定の配置の版が古ければ作り直す）
	void EditorLayout::Initialize()
	{
		std::filesystem::create_directories("Logs");
		ImGui::GetIO().IniFilename = kLayoutFile;
		mRebuild = !std::filesystem::exists(kLayoutFile) || ReadLayoutVersion() != kLayoutVersion;
	}

	void EditorLayout::BeginDockSpace()
	{
		ImGuiID dockId = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		if (mRebuild)
		{
			mRebuild = false;
			Build(dockId);
		}
	}

	void EditorLayout::AfterPanels()
	{
		if (mSelectDefaultTabs) { SelectDefaultTabs(); }
	}

	void EditorLayout::Build(unsigned int dockId)
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::DockBuilderRemoveNode(dockId);
		ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);
		ImGuiID center = dockId;
		ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.17f, nullptr, &center);
		ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.3f, nullptr, &center);
		ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.5f, nullptr, &right);
		ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
		// ウィンドウの ID は「訳###英語」なので、同じ形の名前で指す（ID は ### から後ろで決まる）
		ImGui::DockBuilderDockWindow(TrId("Hierarchy"), left);
		ImGui::DockBuilderDockWindow(TrId("Properties"), right);
		ImGui::DockBuilderDockWindow(TrId("Lighting"), right);
		ImGui::DockBuilderDockWindow(TrId("Stats"), right);
		ImGui::DockBuilderDockWindow(TrId("AI Assistant"), right);
		ImGui::DockBuilderDockWindow(TrId("Preview"), rightBottom);
		ImGui::DockBuilderDockWindow(TrId("Project"), bottom);
		ImGui::DockBuilderDockWindow(TrId("Log"), bottom);
		ImGui::DockBuilderDockWindow(TrId("Profiler"), bottom);
		ImGui::DockBuilderDockWindow(TrId("Scene"), center);
		ImGui::DockBuilderDockWindow(TrId("Game"), center);
		ImGui::DockBuilderFinish(dockId);
		mSelectDefaultTabs = true;
		WriteLayoutVersion();
	}

	// 配置を作り直すと最後に入れたタブが前に出るので、タブができてから Properties と Project を選び直す
	void EditorLayout::SelectDefaultTabs()
	{
		for (const char* name : { "Properties", "Project" })
		{
			ImGuiWindow* window = ImGui::FindWindowByName(TrId(name));
			if (!window || !window->DockNode || !window->DockNode->TabBar) { return; }
			if (!ImGui::TabBarFindTabByID(window->DockNode->TabBar, window->TabId)) { return; }
		}
		for (const char* name : { "Properties", "Project" })
		{
			ImGuiWindow* window = ImGui::FindWindowByName(TrId(name));
			window->DockNode->TabBar->NextSelectedTabId = window->TabId;
			window->DockNode->SelectedTabId = window->TabId;
		}
		mEditor.FocusView("Scene");
		ImGui::MarkIniSettingsDirty();
		mSelectDefaultTabs = false;
	}
}
