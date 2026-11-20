#pragma once
#include <ImGui/imgui.h>

namespace rd
{
	// エディタの配色と寸法（すべてのパネルがここの値を使って見た目をそろえる）
	//  Apply で ImGui のスタイルに反映する。プレイ中はメニューバーの色を変えて分かるようにする
	class RD_API EditorStyle
	{
	public:
		static void Apply(float dpiScale);
		static void SetPlayMode(bool playing, bool paused);

		static constexpr ImVec4 kAccent{ 0.26f, 0.55f, 0.95f, 1.0f };
		static constexpr ImVec4 kAccentDim{ 0.20f, 0.36f, 0.62f, 1.0f };
		static constexpr ImVec4 kWindowBg{ 0.13f, 0.13f, 0.14f, 1.0f };
		static constexpr ImVec4 kToolbarBg{ 0.16f, 0.16f, 0.17f, 1.0f };
		static constexpr ImVec4 kFieldBg{ 0.09f, 0.09f, 0.10f, 1.0f };
		static constexpr ImVec4 kHeaderBg{ 0.19f, 0.19f, 0.21f, 1.0f };
		static constexpr ImVec4 kRowAlt{ 1.0f, 1.0f, 1.0f, 0.025f };
		static constexpr ImVec4 kText{ 0.86f, 0.86f, 0.87f, 1.0f };
		static constexpr ImVec4 kTextDim{ 0.52f, 0.52f, 0.55f, 1.0f };
		static constexpr ImVec4 kInfo{ 0.80f, 0.80f, 0.82f, 1.0f };
		static constexpr ImVec4 kWarning{ 1.00f, 0.78f, 0.30f, 1.0f };
		static constexpr ImVec4 kError{ 1.00f, 0.42f, 0.38f, 1.0f };
		static constexpr ImVec4 kSuccess{ 0.40f, 0.82f, 0.48f, 1.0f };
		static constexpr ImVec4 kPlayTint{ 0.16f, 0.30f, 0.20f, 1.0f };
		static constexpr ImVec4 kPauseTint{ 0.36f, 0.28f, 0.12f, 1.0f };

		// アイコンの色（種類ごとに決めて、どのパネルでも同じ色にする）
		static constexpr ImVec4 kIconNeutral{ 0.72f, 0.74f, 0.78f, 1.0f };
		static constexpr ImVec4 kIconMesh{ 0.55f, 0.72f, 1.00f, 1.0f };
		static constexpr ImVec4 kIconLight{ 1.00f, 0.84f, 0.30f, 1.0f };
		static constexpr ImVec4 kIconCamera{ 0.62f, 0.86f, 0.62f, 1.0f };
		static constexpr ImVec4 kIconEffect{ 1.00f, 0.60f, 0.30f, 1.0f };
		static constexpr ImVec4 kIconPhysics{ 0.45f, 0.90f, 0.55f, 1.0f };
		static constexpr ImVec4 kIconAudio{ 0.85f, 0.55f, 0.95f, 1.0f };
		static constexpr ImVec4 kIconFolder{ 0.90f, 0.74f, 0.38f, 1.0f };

		static float ToolbarHeight();
		static float IconSize();
	};
}
