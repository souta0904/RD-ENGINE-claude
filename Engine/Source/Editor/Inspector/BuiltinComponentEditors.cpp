#include "RdPch.h"
#include "Editor/Inspector/ComponentEditors.h"
#include "Components/Animation/Animator.h"
#include "Components/Audio/AudioSource.h"
#include "Components/Effects/ParticleSystem.h"
#include "Components/Navigation/NavMeshSurface.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Style/EditorGui.h"
#include <ImGui/imgui.h>

namespace rd
{
	namespace
	{
		// クリップを選んで再生し、今の再生位置を棒で見せる
		bool DrawAnimator(Animator& animator)
		{
			auto names = animator.GetClipNames();
			if (names.empty()) { return false; }
			const std::string& current = animator.clip.empty() ? names.front() : animator.clip;
			if (ImGui::BeginCombo("Select Clip", current.c_str()))
			{
				for (auto& n : names)
				{
					if (ImGui::Selectable(n.c_str(), n == animator.clip))
					{
						animator.clip = n;
						animator.Play(animator.clip, 0.2f, animator.clipSource);
					}
				}
				ImGui::EndCombo();
			}
			ImGui::ProgressBar(animator.GetNormalizedTime(), ImVec2(-1, 0), std::format("{:.2f}s", animator.GetTime()).c_str());
			return false;
		}

		// 編集中に音を試しに鳴らす
		bool DrawAudioSource(AudioSource& source)
		{
			if (ImGui::Button("Preview")) { source.Play(); }
			ImGui::SameLine();
			if (ImGui::Button("Stop")) { source.Stop(); }
			return false;
		}

		// 編集中にパーティクルを試しに出す
		bool DrawParticleSystem(ParticleSystem& particles)
		{
			if (ImGui::Button(particles.IsPlaying() ? "Stop" : "Play")) { particles.IsPlaying() ? particles.Stop() : particles.Play(); }
			ImGui::SameLine();
			if (ImGui::Button("Clear")) { particles.Stop(true); }
			ImGui::SameLine();
			ImGui::Text("Alive: %u", particles.GetAliveCount());
			return false;
		}

		// 焼く・外すボタンと、最後に焼いた結果。焼くと data（保存先のパス）が変わるので true を返す
		bool DrawNavMeshSurface(NavMeshSurface& surface)
		{
			bool changed = false;
			ImGui::Spacing();
			if (EditorGui::AccentButton(Tr("Bake"))) { changed = surface.Bake(); }
			ImGui::SameLine();
			if (ImGui::Button(Tr("Clear")) && !surface.data.empty())
			{
				surface.ClearData();
				changed = true;
			}
			const NavMeshSurface::BakeInfo& last = surface.GetLastBake();
			if (last.done)
			{
				ImGui::SameLine();
				if (last.triangles == 0) { ImGui::TextDisabled("%s", Tr("No walkable surface was found")); }
				else
				{
					ImGui::TextDisabled("%s", EditorLocale::Format("{} triangles from {} meshes ({:.0f} ms)", last.triangles, last.meshes, last.ms).c_str());
				}
			}
			return changed;
		}
	}

	void ComponentEditors::RegisterBuiltin()
	{
		Register<Animator>(DrawAnimator);
		Register<AudioSource>(DrawAudioSource);
		Register<ParticleSystem>(DrawParticleSystem);
		Register<NavMeshSurface>(DrawNavMeshSurface);
	}
}
