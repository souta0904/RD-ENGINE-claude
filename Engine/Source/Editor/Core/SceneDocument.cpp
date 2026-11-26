#include "RdPch.h"
#include "Editor/Core/SceneDocument.h"
#include "Editor/Commands/EditorUndo.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Core/FileDialog.h"
#include "Editor/Core/PlayMode.h"
#include "Editor/Editor.h"
#include "Editor/Style/EditorGui.h"
#include "Engine.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>

namespace rd
{
	namespace
	{
		constexpr const wchar_t* kSceneFilter = L"Scene (*.scene)\0*.scene\0";
	}

	void SceneDocument::NewScene()
	{
		RunAfterSaveCheck([this]
			{
				mEditor.GetPlayMode().End();
				mEditor.GetEngine().NewScene();
			});
	}

	void SceneDocument::OpenScene()
	{
		RunAfterSaveCheck([this]
			{
				std::string path = FileDialog::Open(mEditor.GetEngine().GetWindow().GetHandle(), kSceneFilter, L"scene");
				if (path.empty()) { return; }
				mEditor.GetPlayMode().End();
				mEditor.GetEngine().LoadScene(path);
			});
	}

	void SceneDocument::OpenScene(const std::string& path)
	{
		RunAfterSaveCheck([this, path]
			{
				mEditor.GetPlayMode().End();
				mEditor.GetEngine().LoadScene(path);
			});
	}

	bool SceneDocument::SaveScene(bool saveAs)
	{
		if (mEditor.IsPlaying())
		{
			Logger::Warning(LogCat::Editor, "Stop play mode before saving");
			return false;
		}
		Scene& scene = mEditor.GetScene();
		std::string path = scene.GetPath();
		if (saveAs || path.empty())
		{
			path = FileDialog::Save(mEditor.GetEngine().GetWindow().GetHandle(), kSceneFilter, L"scene");
			if (path.empty()) { return false; }
		}
		if (!scene.SaveToFile(path)) { return false; }
		mEditor.GetUndo().MarkSaved();
		mEditor.UpdateTitle();
		return true;
	}

	void SceneDocument::RequestQuit()
	{
		RunAfterSaveCheck([this]
			{
				mAllowClose = true;
				mEditor.GetEngine().Quit();
			});
	}

	bool SceneDocument::InterceptClose()
	{
		if (mAllowClose || !mEditor.GetUndo().IsDirty()) { return false; }
		RequestQuit();
		return true;
	}

	void SceneDocument::RunAfterSaveCheck(std::function<void()> action)
	{
		if (!mEditor.GetUndo().IsDirty())
		{
			action();
			return;
		}
		mPendingAfterSave = std::move(action);
		mOpenSavePrompt = true;
	}

	void SceneDocument::DrawSavePrompt()
	{
		if (mOpenSavePrompt)
		{
			ImGui::OpenPopup(TrId("Save Changes?"));
			mOpenSavePrompt = false;
		}
		const char* const kButtons[] = { Tr("Save"), Tr("Don't Save"), Tr("Cancel") };
		const std::string& path = mEditor.GetScene().GetPath();
		std::string message = EditorLocale::Format("Save the changes to \"{}\"?", path.empty() ? std::string(Tr("Untitled")) : path);
		int choice = -1;
		if (!EditorGui::ConfirmPopup(TrId("Save Changes?"), message.c_str(), kButtons, 3, choice)) { return; }
		auto action = std::move(mPendingAfterSave);
		mPendingAfterSave = nullptr;
		if (choice == 0 && !SaveScene(false)) { return; }
		if ((choice == 0 || choice == 1) && action) { mEditor.Defer(std::move(action)); }
	}
}
