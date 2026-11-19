#include "TestFramework.h"
#if RD_EDITOR
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Core/EditorSession.h"
#include <Windows.h>
#include <filesystem>

using namespace rd;
namespace fs = std::filesystem;

namespace
{
	// テストごとの一時ファイル（終わったら消す）
	struct TempSessionFile
	{
		fs::path path;

		explicit TempSessionFile(const char* name)
		{
			path = fs::temp_directory_path() / std::format("rd_session_{}_{}.json", GetCurrentProcessId(), name);
			Remove();
		}
		~TempSessionFile() { Remove(); }
		void Remove()
		{
			std::error_code ec;
			fs::remove(path, ec);
		}
		std::string Get() const { return ToUtf8(path.generic_wstring()); }
	};
}

RD_TEST(EditorSession, RoundTrip)
{
	TempSessionFile file("roundtrip");
	EditorSession saved;
	saved.hasWindow = true;
	saved.window = { -100, 50, 1280, 720, true, true };
	saved.scene = "Assets/シーン/Test.scene";
	saved.selectedActor = 0x1234567890ull;
	saved.hasCamera = true;
	saved.cameraPosition = { 1.0f, 2.0f, 3.0f };
	saved.cameraYaw = 0.5f;
	saved.cameraPitch = -0.25f;
	saved.cameraFov = 45.0f;
	saved.cameraMoveSpeed = 20.0f;
	saved.gizmoOp = 2;
	saved.gizmoLocal = true;
	saved.showGrid = false;
	saved.showGizmos = false;
	saved.showPhysics = true;
	saved.showNavMesh = true;
	saved.gameViewActive = true;
	saved.projectFolder = "Assets/Models";
	saved.projectSelected = "Assets/Models/a.rdmodel";
	saved.projectIconScale = 1.5f;
	saved.panels = { { "Stats", false }, { "AI Assistant", true } };
	RD_REQUIRE(saved.Save(file.Get()));

	EditorSession loaded;
	RD_REQUIRE(loaded.Load(file.Get()));
	RD_CHECK(loaded.hasWindow);
	RD_CHECK_EQ(loaded.window.x, -100);
	RD_CHECK_EQ(loaded.window.y, 50);
	RD_CHECK_EQ(loaded.window.width, 1280);
	RD_CHECK_EQ(loaded.window.height, 720);
	RD_CHECK(loaded.window.maximized);
	RD_CHECK(loaded.window.fullscreen);
	RD_CHECK(loaded.scene == saved.scene);
	RD_CHECK(loaded.selectedActor == saved.selectedActor);
	RD_CHECK(loaded.hasCamera);
	RD_CHECK_NEAR(loaded.cameraPosition.z, 3.0f, 1e-5f);
	RD_CHECK_NEAR(loaded.cameraYaw, 0.5f, 1e-5f);
	RD_CHECK_NEAR(loaded.cameraPitch, -0.25f, 1e-5f);
	RD_CHECK_NEAR(loaded.cameraFov, 45.0f, 1e-5f);
	RD_CHECK_NEAR(loaded.cameraMoveSpeed, 20.0f, 1e-5f);
	RD_CHECK_EQ(loaded.gizmoOp, 2);
	RD_CHECK(loaded.gizmoLocal);
	RD_CHECK(!loaded.showGrid);
	RD_CHECK(!loaded.showGizmos);
	RD_CHECK(loaded.showPhysics);
	RD_CHECK(loaded.showNavMesh);
	RD_CHECK(loaded.gameViewActive);
	RD_CHECK(loaded.projectFolder == saved.projectFolder);
	RD_CHECK(loaded.projectSelected == saved.projectSelected);
	RD_CHECK_NEAR(loaded.projectIconScale, 1.5f, 1e-5f);
	RD_CHECK(loaded.panels == saved.panels);
}

RD_TEST(EditorSession, MissingFileKeepsDefaults)
{
	TempSessionFile file("missing");
	EditorSession session;
	session.scene = "keep";
	RD_CHECK(!session.Load(file.Get()));
	RD_CHECK(session.scene == "keep");
}

RD_TEST(EditorSession, BrokenFilesAreIgnored)
{
	TempSessionFile file("broken");
	for (const char* text : { "{ not json", "[1, 2, 3]", R"({"Scene": 5})", R"({"Panels": {"Log": "yes"}})", R"({"Camera": {"Position": 1}})" })
	{
		RD_REQUIRE(WriteTextFile(file.Get(), text));
		EditorSession session;
		session.scene = "keep";
		RD_CHECK(!session.Load(file.Get()));
		RD_CHECK(session.scene == "keep");
	}
}

RD_TEST(EditorSession, MissingKeysUseDefaults)
{
	TempSessionFile file("partial");
	RD_REQUIRE(WriteTextFile(file.Get(), R"({"Scene": "Assets/a.scene"})"));
	EditorSession session;
	RD_REQUIRE(session.Load(file.Get()));
	RD_CHECK(session.scene == "Assets/a.scene");
	RD_CHECK(!session.hasWindow);
	RD_CHECK(!session.hasCamera);
	RD_CHECK(session.showGrid);
	RD_CHECK(session.projectFolder == "Assets");
}

RD_TEST(EditorSession, OutOfRangeValuesAreCorrected)
{
	TempSessionFile file("range");
	RD_REQUIRE(WriteTextFile(file.Get(), R"({
		"Window": {"X": 0, "Y": 0, "Width": 10, "Height": 10},
		"Camera": {"Pitch": 100, "Fov": 0, "MoveSpeed": 100000},
		"SceneView": {"GizmoOp": 9},
		"Project": {"IconScale": 50}
	})"));
	EditorSession session;
	RD_REQUIRE(session.Load(file.Get()));
	RD_CHECK(!session.hasWindow);
	RD_CHECK(session.cameraPitch < 1.58f);
	RD_CHECK_NEAR(session.cameraFov, 1.0f, 1e-5f);
	RD_CHECK_NEAR(session.cameraMoveSpeed, 500.0f, 1e-5f);
	RD_CHECK_EQ(session.gizmoOp, 2);
	RD_CHECK_NEAR(session.projectIconScale, 2.0f, 1e-5f);
}
#endif
