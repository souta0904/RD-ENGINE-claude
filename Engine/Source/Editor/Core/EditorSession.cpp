#include "RdPch.h"
#include "Editor/Core/EditorSession.h"
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include "Runtime/GameModule.h"

namespace rd
{
	namespace
	{
		// これより小さいウィンドウは作れないので、保存された値としておかしいものとして捨てる
		constexpr int kMinWindowWidth = 320;
		constexpr int kMinWindowHeight = 240;
	}

	bool EditorSession::Load(const std::string& path)
	{
		if (!FileExists(path)) { return false; }
		Json json;
		if (!LoadJsonFile(path, json)) { return false; }
		try
		{
			// 途中で失敗しても自分の中身が半端にならないよう、別の変数へ読んでから移す
			EditorSession loaded;
			loaded.FromJson(json);
			*this = std::move(loaded);
			return true;
		}
		catch (const std::exception& e)
		{
			Log::Warning(LogCat::Editor, "Ignoring editor session '{}': {}", path, e.what());
			return false;
		}
	}

	bool EditorSession::Save(const std::string& path) const
	{
		return SaveJsonFile(path, ToJson());
	}

	Json EditorSession::ToJson() const
	{
		Json json = Json::object();
		if (hasWindow)
		{
			json["Window"] = {
				{ "X", window.x }, { "Y", window.y }, { "Width", window.width }, { "Height", window.height },
				{ "Maximized", window.maximized }, { "Fullscreen", window.fullscreen } };
		}
		json["Scene"] = scene;
		json["SelectedActor"] = selectedActor;
		if (hasCamera)
		{
			json["Camera"] = {
				{ "Position", cameraPosition }, { "Yaw", cameraYaw }, { "Pitch", cameraPitch },
				{ "Fov", cameraFov }, { "MoveSpeed", cameraMoveSpeed } };
		}
		json["SceneView"] = {
			{ "GizmoOp", gizmoOp }, { "GizmoLocal", gizmoLocal }, { "Grid", showGrid },
			{ "Gizmos", showGizmos }, { "Physics", showPhysics }, { "NavMesh", showNavMesh } };
		json["GameViewActive"] = gameViewActive;
		json["Project"] = { { "Folder", projectFolder }, { "Selected", projectSelected }, { "IconScale", projectIconScale } };
		Json panelsJson = Json::object();
		for (const auto& [name, open] : panels) { panelsJson[name] = open; }
		json["Panels"] = std::move(panelsJson);
		json["Language"] = language;
		return json;
	}

	// 無い項目は今の値（既定値）のままにする。後から項目を増やしても古いファイルを読めるようにするため
	void EditorSession::FromJson(const Json& json)
	{
		if (auto it = json.find("Window"); it != json.end())
		{
			window.x = it->value("X", 0);
			window.y = it->value("Y", 0);
			window.width = it->value("Width", 0);
			window.height = it->value("Height", 0);
			window.maximized = it->value("Maximized", false);
			window.fullscreen = it->value("Fullscreen", false);
			hasWindow = window.width >= kMinWindowWidth && window.height >= kMinWindowHeight;
		}
		scene = json.value("Scene", scene);
		selectedActor = json.value("SelectedActor", selectedActor);
		if (auto it = json.find("Camera"); it != json.end())
		{
			cameraPosition = it->value("Position", cameraPosition);
			cameraYaw = it->value("Yaw", cameraYaw);
			// 範囲はエディタカメラの操作で取れる値に合わせる（真上・真下を向くと向きが計算できなくなる）
			cameraPitch = math::Clamp(it->value("Pitch", cameraPitch), -math::kPiOver2 + 0.01f, math::kPiOver2 - 0.01f);
			cameraFov = math::Clamp(it->value("Fov", cameraFov), 1.0f, 179.0f);
			cameraMoveSpeed = math::Clamp(it->value("MoveSpeed", cameraMoveSpeed), 0.5f, 500.0f);
			hasCamera = true;
		}
		if (auto it = json.find("SceneView"); it != json.end())
		{
			gizmoOp = math::Clamp(it->value("GizmoOp", gizmoOp), 0, 2);
			gizmoLocal = it->value("GizmoLocal", gizmoLocal);
			showGrid = it->value("Grid", showGrid);
			showGizmos = it->value("Gizmos", showGizmos);
			showPhysics = it->value("Physics", showPhysics);
			showNavMesh = it->value("NavMesh", showNavMesh);
		}
		gameViewActive = json.value("GameViewActive", gameViewActive);
		if (auto it = json.find("Project"); it != json.end())
		{
			projectFolder = it->value("Folder", projectFolder);
			projectSelected = it->value("Selected", projectSelected);
			projectIconScale = math::Clamp(it->value("IconScale", projectIconScale), 0.6f, 2.0f);
		}
		if (auto it = json.find("Panels"); it != json.end())
		{
			for (const auto& [name, open] : it->items()) { panels[name] = open.get<bool>(); }
		}
		language = json.value("Language", language);
	}

	std::string EditorSession::GetDefaultPath()
	{
		// RdEditor.exe はどのゲームでも同じ exe なので、読んだゲームの名前で分ける
		if (!GameModule::GetName().empty()) { return "UserSettings/EditorSession_" + GameModule::GetName() + ".json"; }
		wchar_t exe[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		return "UserSettings/EditorSession_" + ToUtf8(std::filesystem::path(exe).stem().wstring()) + ".json";
	}
}
