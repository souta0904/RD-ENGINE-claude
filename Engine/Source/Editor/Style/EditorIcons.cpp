#include "RdPch.h"
#include "Editor/Style/EditorIcons.h"
#include "Asset/Audio/AudioClip.h"
#include "Asset/Model/Model.h"
#include "Components/Animation/Animator.h"
#include "Components/Audio/AudioSource.h"
#include "Components/Effects/ParticleSystem.h"
#include "Components/Lighting/DirectionalLight.h"
#include "Components/Lighting/PointLight.h"
#include "Components/Lighting/SpotLight.h"
#include "Components/Rendering/CameraComponent.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Components/Rendering/SpriteRenderer.h"
#include "Editor/Style/EditorStyle.h"
#include "Physics/Colliders/Collider.h"
#include "Physics/Rigidbody/Rigidbody.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	namespace
	{
		constexpr float kPi = 3.14159265f;

		ImU32 ToU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

		ImU32 Shade(ImU32 color, float factor)
		{
			ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
			return ToU32(ImVec4(c.x * factor, c.y * factor, c.z * factor, c.w));
		}

		// 等角の立方体（filled なら 3 面を濃さを変えて塗る）
		void DrawCube(ImDrawList* dl, float cx, float cy, float s, ImU32 color, bool filled)
		{
			float r = s * 0.46f, w = r * 0.866f;
			ImVec2 top(cx, cy - r), ur(cx + w, cy - r * 0.5f), lr(cx + w, cy + r * 0.5f);
			ImVec2 bot(cx, cy + r), ll(cx - w, cy + r * 0.5f), ul(cx - w, cy - r * 0.5f), c(cx, cy);
			if (filled)
			{
				dl->AddQuadFilled(top, ur, c, ul, color);
				dl->AddQuadFilled(ul, c, bot, ll, Shade(color, 0.75f));
				dl->AddQuadFilled(c, ur, lr, bot, Shade(color, 0.55f));
			}
			ImVec2 hex[] = { top, ur, lr, bot, ll, ul };
			dl->AddPolyline(hex, 6, filled ? Shade(color, 1.2f) : color, ImDrawFlags_Closed, 1.0f);
			dl->AddLine(c, ul, color);
			dl->AddLine(c, ur, color);
			dl->AddLine(c, bot, color);
		}

		void DrawSun(ImDrawList* dl, float cx, float cy, float s, ImU32 color)
		{
			dl->AddCircleFilled(ImVec2(cx, cy), s * 0.2f, color);
			for (int i = 0; i < 8; ++i)
			{
				float a = float(i) * (kPi / 4.0f);
				float ca = std::cos(a), sa = std::sin(a);
				dl->AddLine(ImVec2(cx + ca * s * 0.3f, cy + sa * s * 0.3f), ImVec2(cx + ca * s * 0.46f, cy + sa * s * 0.46f), color, 1.2f);
			}
		}

		void DrawFolder(ImDrawList* dl, ImVec2 p, float s, ImU32 color)
		{
			dl->AddRectFilled(ImVec2(p.x + s * 0.06f, p.y + s * 0.2f), ImVec2(p.x + s * 0.46f, p.y + s * 0.34f), Shade(color, 0.8f), 1.5f);
			dl->AddRectFilled(ImVec2(p.x + s * 0.06f, p.y + s * 0.3f), ImVec2(p.x + s * 0.94f, p.y + s * 0.84f), color, 1.5f);
		}

		// 右上が折れた紙。mark があれば中に小さな印を描く
		void DrawFile(ImDrawList* dl, ImVec2 p, float s, ImU32 color)
		{
			float l = p.x + s * 0.18f, r = p.x + s * 0.82f, t = p.y + s * 0.06f, b = p.y + s * 0.94f, f = s * 0.22f;
			ImVec2 pts[] = { ImVec2(l, t), ImVec2(r - f, t), ImVec2(r, t + f), ImVec2(r, b), ImVec2(l, b) };
			dl->AddPolyline(pts, 5, color, ImDrawFlags_Closed, 1.2f);
			dl->AddLine(ImVec2(r - f, t), ImVec2(r - f, t + f), color);
			dl->AddLine(ImVec2(r - f, t + f), ImVec2(r, t + f), color);
		}

		void DrawEye(ImDrawList* dl, float cx, float cy, float s, ImU32 color, bool open)
		{
			float w = s * 0.42f, h = s * 0.3f;
			dl->PathLineTo(ImVec2(cx - w, cy));
			dl->PathBezierCubicCurveTo(ImVec2(cx - w * 0.5f, cy - h), ImVec2(cx + w * 0.5f, cy - h), ImVec2(cx + w, cy));
			dl->PathBezierCubicCurveTo(ImVec2(cx + w * 0.5f, cy + h), ImVec2(cx - w * 0.5f, cy + h), ImVec2(cx - w, cy));
			dl->PathStroke(color, ImDrawFlags_Closed, 1.2f);
			if (open) { dl->AddCircleFilled(ImVec2(cx, cy), s * 0.12f, color); }
			else { dl->AddLine(ImVec2(cx - w * 0.8f, cy + h * 1.3f), ImVec2(cx + w * 0.8f, cy - h * 1.3f), color, 1.4f); }
		}

		void DrawArrowHead(ImDrawList* dl, ImVec2 tip, ImVec2 dir, float len, ImU32 color)
		{
			ImVec2 n(-dir.y, dir.x);
			dl->AddTriangleFilled(tip, ImVec2(tip.x - dir.x * len + n.x * len * 0.6f, tip.y - dir.y * len + n.y * len * 0.6f),
				ImVec2(tip.x - dir.x * len - n.x * len * 0.6f, tip.y - dir.y * len - n.y * len * 0.6f), color);
		}

		void DrawStatus(ImDrawList* dl, float cx, float cy, float s, ImU32 color, Icon icon)
		{
			if (icon == Icon::Warning)
			{
				dl->AddTriangleFilled(ImVec2(cx, cy - s * 0.42f), ImVec2(cx + s * 0.46f, cy + s * 0.38f), ImVec2(cx - s * 0.46f, cy + s * 0.38f), color);
			}
			else
			{
				dl->AddCircleFilled(ImVec2(cx, cy), s * 0.42f, color);
			}
			ImU32 ink = IM_COL32(20, 20, 22, 255);
			if (icon == Icon::Info)
			{
				dl->AddRectFilled(ImVec2(cx - s * 0.05f, cy - s * 0.06f), ImVec2(cx + s * 0.05f, cy + s * 0.24f), ink);
				dl->AddCircleFilled(ImVec2(cx, cy - s * 0.2f), s * 0.06f, ink);
			}
			else
			{
				dl->AddRectFilled(ImVec2(cx - s * 0.05f, cy - s * 0.2f), ImVec2(cx + s * 0.05f, cy + s * 0.08f), ink);
				dl->AddCircleFilled(ImVec2(cx, cy + s * 0.2f), s * 0.06f, ink);
			}
		}
	}

	void EditorIcons::Draw(ImDrawList* dl, ImVec2 p, float s, Icon icon, ImU32 color)
	{
		float cx = p.x + s * 0.5f, cy = p.y + s * 0.5f;
		switch (icon)
		{
		case Icon::Actor: DrawCube(dl, cx, cy, s, color, false); break;
		case Icon::Mesh: DrawCube(dl, cx, cy, s, color, true); break;
		case Icon::Light: DrawSun(dl, cx, cy, s, color); break;
		case Icon::Camera:
			dl->AddRectFilled(ImVec2(p.x + s * 0.06f, cy - s * 0.22f), ImVec2(p.x + s * 0.62f, cy + s * 0.24f), color, 1.5f);
			dl->AddTriangleFilled(ImVec2(p.x + s * 0.62f, cy), ImVec2(p.x + s * 0.96f, cy - s * 0.24f), ImVec2(p.x + s * 0.96f, cy + s * 0.26f), color);
			break;
		case Icon::Particles:
			dl->AddCircleFilled(ImVec2(cx - s * 0.2f, cy + s * 0.18f), s * 0.14f, color);
			dl->AddCircleFilled(ImVec2(cx + s * 0.22f, cy + s * 0.05f), s * 0.11f, color);
			dl->AddCircleFilled(ImVec2(cx - s * 0.02f, cy - s * 0.25f), s * 0.09f, color);
			break;
		case Icon::Physics:
			dl->AddCircle(ImVec2(cx, cy), s * 0.38f, color, 0, 1.4f);
			dl->AddLine(ImVec2(cx, cy - s * 0.18f), ImVec2(cx, cy + s * 0.18f), color, 1.4f);
			DrawArrowHead(dl, ImVec2(cx, cy + s * 0.26f), ImVec2(0.0f, 1.0f), s * 0.14f, color);
			break;
		case Icon::Audio:
			dl->AddRectFilled(ImVec2(p.x + s * 0.12f, cy - s * 0.14f), ImVec2(p.x + s * 0.34f, cy + s * 0.14f), color);
			dl->AddTriangleFilled(ImVec2(p.x + s * 0.3f, cy), ImVec2(p.x + s * 0.56f, cy - s * 0.34f), ImVec2(p.x + s * 0.56f, cy + s * 0.34f), color);
			dl->PathArcTo(ImVec2(p.x + s * 0.56f, cy), s * 0.24f, -0.9f, 0.9f);
			dl->PathStroke(color, 0, 1.3f);
			break;
		case Icon::Animation:
			dl->AddCircle(ImVec2(cx, cy), s * 0.4f, color, 0, 1.3f);
			dl->AddTriangleFilled(ImVec2(cx - s * 0.12f, cy - s * 0.2f), ImVec2(cx + s * 0.22f, cy), ImVec2(cx - s * 0.12f, cy + s * 0.2f), color);
			break;
		case Icon::Component:
			dl->AddCircle(ImVec2(cx, cy), s * 0.3f, color, 0, 2.0f);
			for (int i = 0; i < 6; ++i)
			{
				float a = float(i) * (kPi / 3.0f);
				dl->AddLine(ImVec2(cx + std::cos(a) * s * 0.3f, cy + std::sin(a) * s * 0.3f), ImVec2(cx + std::cos(a) * s * 0.46f, cy + std::sin(a) * s * 0.46f), color, 2.0f);
			}
			break;
		case Icon::Folder: DrawFolder(dl, p, s, color); break;
		case Icon::File: DrawFile(dl, p, s, color); break;
		case Icon::Image:
			DrawFile(dl, p, s, color);
			dl->AddTriangleFilled(ImVec2(cx - s * 0.2f, cy + s * 0.3f), ImVec2(cx, cy), ImVec2(cx + s * 0.2f, cy + s * 0.3f), color);
			break;
		case Icon::Model:
			DrawCube(dl, cx, cy, s * 0.9f, color, true);
			break;
		case Icon::Scene:
			DrawFile(dl, p, s, color);
			dl->AddCircle(ImVec2(cx, cy + s * 0.08f), s * 0.16f, color);
			break;
		case Icon::Prefab:
			DrawCube(dl, cx, cy, s * 0.9f, color, true);
			break;
		case Icon::Shader:
			DrawFile(dl, p, s, color);
			dl->AddLine(ImVec2(cx - s * 0.16f, cy), ImVec2(cx + s * 0.16f, cy), color);
			dl->AddLine(ImVec2(cx - s * 0.16f, cy + s * 0.18f), ImVec2(cx + s * 0.08f, cy + s * 0.18f), color);
			break;
		case Icon::Play:
			dl->AddTriangleFilled(ImVec2(cx - s * 0.26f, cy - s * 0.34f), ImVec2(cx + s * 0.34f, cy), ImVec2(cx - s * 0.26f, cy + s * 0.34f), color);
			break;
		case Icon::Pause:
			dl->AddRectFilled(ImVec2(cx - s * 0.28f, cy - s * 0.32f), ImVec2(cx - s * 0.08f, cy + s * 0.32f), color);
			dl->AddRectFilled(ImVec2(cx + s * 0.08f, cy - s * 0.32f), ImVec2(cx + s * 0.28f, cy + s * 0.32f), color);
			break;
		case Icon::Step:
			dl->AddTriangleFilled(ImVec2(cx - s * 0.32f, cy - s * 0.32f), ImVec2(cx + s * 0.16f, cy), ImVec2(cx - s * 0.32f, cy + s * 0.32f), color);
			dl->AddRectFilled(ImVec2(cx + s * 0.18f, cy - s * 0.32f), ImVec2(cx + s * 0.32f, cy + s * 0.32f), color);
			break;
		case Icon::Move:
			dl->AddLine(ImVec2(cx - s * 0.36f, cy), ImVec2(cx + s * 0.36f, cy), color, 1.4f);
			dl->AddLine(ImVec2(cx, cy - s * 0.36f), ImVec2(cx, cy + s * 0.36f), color, 1.4f);
			DrawArrowHead(dl, ImVec2(cx + s * 0.46f, cy), ImVec2(1.0f, 0.0f), s * 0.14f, color);
			DrawArrowHead(dl, ImVec2(cx - s * 0.46f, cy), ImVec2(-1.0f, 0.0f), s * 0.14f, color);
			DrawArrowHead(dl, ImVec2(cx, cy - s * 0.46f), ImVec2(0.0f, -1.0f), s * 0.14f, color);
			DrawArrowHead(dl, ImVec2(cx, cy + s * 0.46f), ImVec2(0.0f, 1.0f), s * 0.14f, color);
			break;
		case Icon::Rotate:
			dl->PathArcTo(ImVec2(cx, cy), s * 0.34f, -kPi * 0.9f, kPi * 0.55f);
			dl->PathStroke(color, 0, 1.6f);
			DrawArrowHead(dl, ImVec2(cx + std::cos(kPi * 0.55f) * s * 0.34f, cy + std::sin(kPi * 0.55f) * s * 0.34f), ImVec2(-0.9f, 0.45f), s * 0.16f, color);
			break;
		case Icon::Scale:
			dl->AddRect(ImVec2(p.x + s * 0.12f, p.y + s * 0.38f), ImVec2(p.x + s * 0.62f, p.y + s * 0.88f), color, 0.0f, 0, 1.4f);
			dl->AddLine(ImVec2(p.x + s * 0.5f, p.y + s * 0.5f), ImVec2(p.x + s * 0.86f, p.y + s * 0.14f), color, 1.4f);
			DrawArrowHead(dl, ImVec2(p.x + s * 0.9f, p.y + s * 0.1f), ImVec2(0.707f, -0.707f), s * 0.16f, color);
			break;
		case Icon::Global:
			dl->AddCircle(ImVec2(cx, cy), s * 0.4f, color, 0, 1.2f);
			dl->AddEllipse(ImVec2(cx, cy), s * 0.16f, s * 0.4f, color, 0.0f, 0, 1.0f);
			dl->AddLine(ImVec2(cx - s * 0.4f, cy), ImVec2(cx + s * 0.4f, cy), color);
			break;
		case Icon::Local:
			DrawCube(dl, cx, cy, s * 0.95f, color, false);
			break;
		case Icon::Eye: DrawEye(dl, cx, cy, s, color, true); break;
		case Icon::EyeOff: DrawEye(dl, cx, cy, s, color, false); break;
		case Icon::Plus:
			dl->AddLine(ImVec2(cx - s * 0.32f, cy), ImVec2(cx + s * 0.32f, cy), color, 2.0f);
			dl->AddLine(ImVec2(cx, cy - s * 0.32f), ImVec2(cx, cy + s * 0.32f), color, 2.0f);
			break;
		case Icon::Search:
			dl->AddCircle(ImVec2(cx - s * 0.08f, cy - s * 0.08f), s * 0.24f, color, 0, 1.4f);
			dl->AddLine(ImVec2(cx + s * 0.1f, cy + s * 0.1f), ImVec2(cx + s * 0.34f, cy + s * 0.34f), color, 1.8f);
			break;
		case Icon::Menu:
			for (int i = -1; i <= 1; ++i) { dl->AddCircleFilled(ImVec2(cx, cy + float(i) * s * 0.26f), s * 0.07f, color); }
			break;
		case Icon::Info:
		case Icon::Warning:
		case Icon::Error:
			DrawStatus(dl, cx, cy, s, color, icon);
			break;
		}
	}

	void EditorIcons::Inline(Icon icon, float size)
	{
		if (size <= 0.0f) { size = EditorStyle::IconSize(); }
		ImVec2 p = ImGui::GetCursorScreenPos();
		float y = p.y + (ImGui::GetFrameHeight() - size) * 0.5f;
		Draw(ImGui::GetWindowDrawList(), ImVec2(p.x, y), size, icon);
		ImGui::Dummy(ImVec2(size, ImGui::GetFrameHeight()));
	}

	ImU32 EditorIcons::DefaultColor(Icon icon)
	{
		switch (icon)
		{
		case Icon::Mesh: case Icon::Model: case Icon::Prefab: return ToU32(EditorStyle::kIconMesh);
		case Icon::Light: return ToU32(EditorStyle::kIconLight);
		case Icon::Camera: return ToU32(EditorStyle::kIconCamera);
		case Icon::Particles: return ToU32(EditorStyle::kIconEffect);
		case Icon::Physics: return ToU32(EditorStyle::kIconPhysics);
		case Icon::Audio: case Icon::Animation: return ToU32(EditorStyle::kIconAudio);
		case Icon::Folder: return ToU32(EditorStyle::kIconFolder);
		case Icon::Play: return ToU32(EditorStyle::kSuccess);
		case Icon::Info: return ToU32(EditorStyle::kInfo);
		case Icon::Warning: return ToU32(EditorStyle::kWarning);
		case Icon::Error: return ToU32(EditorStyle::kError);
		default: return ToU32(EditorStyle::kIconNeutral);
		}
	}

	Icon EditorIcons::ForActor(const Actor* a)
	{
		if (a->GetComponent<CameraComponent>()) { return Icon::Camera; }
		if (a->GetComponent<DirectionalLight>() || a->GetComponent<PointLight>() || a->GetComponent<SpotLight>()) { return Icon::Light; }
		if (a->GetComponent<ParticleSystem>()) { return Icon::Particles; }
		if (a->GetComponent<MeshRenderer>() || a->GetComponent<SpriteRenderer>()) { return Icon::Mesh; }
		return Icon::Actor;
	}

	Icon EditorIcons::ForComponent(const Component* c)
	{
		if (dynamic_cast<const MeshRenderer*>(c) || dynamic_cast<const SpriteRenderer*>(c)) { return Icon::Mesh; }
		if (dynamic_cast<const DirectionalLight*>(c) || dynamic_cast<const PointLight*>(c) || dynamic_cast<const SpotLight*>(c)) { return Icon::Light; }
		if (dynamic_cast<const CameraComponent*>(c)) { return Icon::Camera; }
		if (dynamic_cast<const ParticleSystem*>(c)) { return Icon::Particles; }
		if (dynamic_cast<const Collider*>(c) || dynamic_cast<const Rigidbody*>(c)) { return Icon::Physics; }
		if (dynamic_cast<const AudioSource*>(c)) { return Icon::Audio; }
		if (dynamic_cast<const Animator*>(c)) { return Icon::Animation; }
		return Icon::Component;
	}

	Icon EditorIcons::ForFile(const std::string& ext, bool directory)
	{
		if (directory) { return Icon::Folder; }
		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".dds" || ext == ".hdr") { return Icon::Image; }
		if (Model::IsSupportedExtension(ext)) { return Icon::Model; }
		if (AudioClip::IsSupportedExtension(ext)) { return Icon::Audio; }
		if (ext == ".scene") { return Icon::Scene; }
		if (ext == ".prefab") { return Icon::Prefab; }
		if (ext == ".hlsl" || ext == ".hlsli") { return Icon::Shader; }
		return Icon::File;
	}
}
