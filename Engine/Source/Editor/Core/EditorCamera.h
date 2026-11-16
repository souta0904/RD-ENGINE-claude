#pragma once
#include "Math/Geometry/AABB.h"
#include "Render/Camera/RenderCamera.h"

namespace rd
{
	// エディタのフライカメラ
	//  右ドラッグ: 見回す + WASD/QE で移動（Shift で速く、ホイールで速さを変える） / 中ドラッグ: 平行移動 / ホイール: 前後
	//  Alt + 左ドラッグ: 注視点のまわりを回る / Focus: 選んだものが収まる位置へ滑らかに動く
	class EditorCamera
	{
	public:
		void Update(float dt, bool hovered);
		void Focus(const AABB& bounds);
		RenderCamera GetRenderCamera(float aspect) const;
		bool IsControlling() const { return mControlling; }

		Vector3 position = { 0.0f, 5.0f, -12.0f };
		float yaw = 0.0f;
		float pitch = math::ToRadians(20.0f);
		float fov = 60.0f;
		float moveSpeed = 8.0f;

	private:
		Quaternion GetRotation() const { return Quaternion::FromEuler({ pitch, yaw, 0.0f }); }
		void Look(const Vector2& delta);
		bool mControlling = false;
		float mOrbitDistance = 10.0f;
		bool mAnimating = false;
		Vector3 mTargetPos;
	};
}
