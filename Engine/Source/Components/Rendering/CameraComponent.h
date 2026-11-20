#pragma once
#include "Render/RenderList/RenderList.h"
#include "Render/Material/Material.h"
#include "Render/Camera/RenderCamera.h"
#include "Render/Settings/Environment.h"
#include "Render/Settings/RenderSettings.h"
#include "Render/Settings/RenderStats.h"
#include "Scene/Component/Component.h"

namespace rd
{
	// カメラ（有効なものの中で priority が一番高いものがゲーム画面に使われる。fov は縦の視野角（度））
	class RD_API CameraComponent final : public Component
	{
		RD_COMPONENT(CameraComponent)
	public:
		float fov = 60.0f;
		float nearZ = 0.1f;
		float farZ = 1000.0f;
		int32_t priority = 0;

		void Reflect(PropertyVisitor& v) override;
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif

		RenderCamera GetRenderCamera(float aspect) const;
		int32_t GetPriority() const { return priority; }

		// 同じ priority なら先に追加されたもの
		static CameraComponent* FindMain(const Scene& scene);
	};
}
