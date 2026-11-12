#pragma once
#include "Render/RenderTypes.h"
#include "Scene/Component.h"

namespace rd
{
	// カメラ（優先度が最も高い有効なカメラがゲーム画面に使われる）
	class CameraComponent final : public Component
	{
		RD_COMPONENT(CameraComponent)
	public:
		float fov = 60.0f;// 垂直視野角（度）
		float nearZ = 0.1f;
		float farZ = 1000.0f;
		int32_t priority = 0;

		void Reflect(PropertyVisitor& v) override;
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;

		RenderCamera GetRenderCamera(float aspect) const;
		int32_t GetPriority() const { return priority; }
	};
}
