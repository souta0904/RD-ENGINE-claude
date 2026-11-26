#pragma once
#include "Core/Serialization/Json.h"
#include "Platform/Window/WindowDesc.h"
#include "Scene/Actor/Actor.h"
#include <map>
#include <string>

namespace rd
{
	// 終了時のエディタの状態（次の起動で同じ画面に戻すために UserSettings/ へ保存する）
	//  ここはファイルとの読み書きだけを受け持つ。集めて反映するのは Engine と Editor の役目。
	//  手で書き換えられたり古い形式だったりしても起動できるよう、読めないファイルは無かったものとして扱う
	struct RD_API EditorSession
	{
		// ウィンドウ
		bool hasWindow = false;
		WindowPlacement window;

		// 開いているシーンと、その中で選んでいたもの
		std::string scene;
		ActorId selectedActor = 0;

		// シーンビューのカメラ（シーンごとに見たい場所が違うので、同じシーンを開いたときだけ戻す）
		bool hasCamera = false;
		Vector3 cameraPosition;
		float cameraYaw = 0.0f;
		float cameraPitch = 0.0f;
		float cameraFov = 60.0f;
		float cameraMoveSpeed = 8.0f;

		// シーンビューの表示（gizmoOp は SceneViewPanel::GizmoOp の番号）
		int gizmoOp = 0;
		bool gizmoLocal = false;
		bool showGrid = true;
		bool showGizmos = true;
		bool showPhysics = false;
		bool showNavMesh = false;
		// 前に出ていたのがゲームビューか
		bool gameViewActive = false;

		// プロジェクトパネル
		std::string projectFolder = "Assets";
		std::string projectSelected;
		float projectIconScale = 1.0f;

		// 行動ツリーのパネルで開いていたファイル
		std::string behaviorTree;

		// パネルの開閉（キーはパネルの名前。ドッキングの配置は ImGui が別に保存している）
		std::map<std::string, bool> panels;

		// エディタの表示の言語（"en" / "ja"。EditorLocale。既定は英語）
		std::string language = "en";

		// ファイルがない・壊れているときは false を返し、中身は変えない
		bool Load(const std::string& path);
		bool Save(const std::string& path) const;

		Json ToJson() const;
		// 型が違う値があれば例外を投げる（Load が受け止める）
		void FromJson(const Json& json);

		// 実行ファイルごとの保存先（Sandbox と MouseGame は同じフォルダで動くので分ける）
		static std::string GetDefaultPath();
	};
}
