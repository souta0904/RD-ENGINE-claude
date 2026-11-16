#include "RdPch.h"
#include "Editor/Commands/SceneSettingsCommand.h"
#include "Scene/Scene.h"

namespace rd
{
	void SceneSettingsCommand::Undo(Scene& scene) { scene.ApplySettings(mBefore); }
	void SceneSettingsCommand::Redo(Scene& scene) { scene.ApplySettings(mAfter); }
}
