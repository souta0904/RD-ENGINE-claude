#include "RdPch.h"
#include "Editor/Commands/CompositeCommand.h"

namespace rd
{
	void CompositeCommand::Undo(Scene& scene)
	{
		for (auto it = mCommands.rbegin(); it != mCommands.rend(); ++it) { (*it)->Undo(scene); }
	}

	void CompositeCommand::Redo(Scene& scene)
	{
		for (auto& c : mCommands) { c->Redo(scene); }
	}
}
