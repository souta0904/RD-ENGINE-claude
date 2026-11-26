#include "RdPch.h"
#include "Editor/Commands/SceneSettingsCommand.h"
#include "Scene/Scene.h"
#include "Scene/Serialization/SceneSerializer.h"

namespace rd
{
	void SceneSettingsCommand::Undo(Scene& scene) { SceneSerializer::ApplySettings(scene, mBefore); }
	void SceneSettingsCommand::Redo(Scene& scene) { SceneSerializer::ApplySettings(scene, mAfter); }
}
