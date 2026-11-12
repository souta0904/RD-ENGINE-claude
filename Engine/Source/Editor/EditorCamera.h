#pragma once
#include "Math/Geometry.h"
#include "Render/RenderTypes.h"

namespace rd
{
	// エディタ用のフライカメラ
	//  右ドラッグ: 視点回転 + WASD/QE 移動（Shift で加速） / 中ドラッグ: パン / ホイール: 前後
	//  Alt + 左ドラッグ: 注視点まわりの回転 / F: 選択物へフォーカス
	class EditorCamera
	{
	public:
		void Update(float dt, bool hovered, bool focused);
		void Focus(const AABB& bounds);
		RenderCamera GetRenderCamera(float aspect) const;
		bool IsControlling() const { return mControlling; }

		Vector3 position = { 0.0f, 5.0f, -12.0f };
		float yaw = 0.0f;// ラジアン
		float pitch = math::ToRadians(20.0f);
		float fov = 60.0f;
		float moveSpeed = 8.0f;

	private:
		Quaternion GetRotation() const { return Quaternion::FromEuler({ pitch, yaw, 0.0f }); }
		bool mControlling = false;
		float mOrbitDistance = 10.0f;
		// フォーカス時の滑らかな移動
		bool mAnimating = false;
		Vector3 mTargetPos;
	};
}
