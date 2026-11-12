#include "RdPch.h"
#include "Editor/EditorCamera.h"
#include "Platform/Input.h"

namespace rd
{
	void EditorCamera::Update(float dt, bool hovered, bool focused)
	{
		Vector2 delta = Input::GetMouseDeltaRaw();
		bool rmb = Input::GetMouseButtonRaw(MouseButton::Right);
		bool mmb = Input::GetMouseButtonRaw(MouseButton::Middle);
		bool lmb = Input::GetMouseButtonRaw(MouseButton::Left);
		bool alt = Input::GetKeyRaw(Key::Alt);

		// 操作の開始はビューポート上でのみ、続行はどこでも
		if (!mControlling && hovered && (rmb || mmb || (alt && lmb))) { mControlling = true; }
		if (mControlling && !rmb && !mmb && !(alt && lmb)) { mControlling = false; }

		Quaternion rot = GetRotation();
		Vector3 forward = Vector3::kUnitZ * rot;
		Vector3 right = Vector3::kUnitX * rot;
		Vector3 up = Vector3::kUnitY * rot;
		const float sensitivity = 0.004f;

		if (mControlling)
		{
			mAnimating = false;
			if (rmb)
			{
				yaw += delta.x * sensitivity;
				pitch = math::Clamp(pitch + delta.y * sensitivity, -math::kPiOver2 + 0.01f, math::kPiOver2 - 0.01f);
				// 移動
				float speed = moveSpeed * (Input::GetKeyRaw(Key::Shift) ? 3.0f : 1.0f);
				Vector3 move;
				if (Input::GetKeyRaw(Key::W)) { move += forward; }
				if (Input::GetKeyRaw(Key::S)) { move -= forward; }
				if (Input::GetKeyRaw(Key::D)) { move += right; }
				if (Input::GetKeyRaw(Key::A)) { move -= right; }
				if (Input::GetKeyRaw(Key::E)) { move += Vector3::kUnitY; }
				if (Input::GetKeyRaw(Key::Q)) { move -= Vector3::kUnitY; }
				position += move * speed * dt;
				// ホイールで速度調整
				float wheel = Input::GetMouseWheelRaw();
				if (wheel != 0.0f) { moveSpeed = math::Clamp(moveSpeed * (wheel > 0 ? 1.2f : 1.0f / 1.2f), 0.5f, 500.0f); }
			}
			else if (mmb)
			{
				float scale = std::max(mOrbitDistance, 1.0f) * 0.0015f;
				position += (-right * delta.x + up * delta.y) * scale;
			}
			else if (alt && lmb)
			{
				// 前方の注視点を中心に回る
				Vector3 pivot = position + forward * mOrbitDistance;
				yaw += delta.x * sensitivity;
				pitch = math::Clamp(pitch + delta.y * sensitivity, -math::kPiOver2 + 0.01f, math::kPiOver2 - 0.01f);
				position = pivot - (Vector3::kUnitZ * GetRotation()) * mOrbitDistance;
			}
		}
		else if (hovered)
		{
			float wheel = Input::GetMouseWheelRaw();
			if (wheel != 0.0f)
			{
				position += forward * wheel * std::max(mOrbitDistance * 0.15f, 0.5f);
				mOrbitDistance = std::max(1.0f, mOrbitDistance - wheel * mOrbitDistance * 0.15f);
			}
		}
		(void)focused;

		if (mAnimating)
		{
			position = math::Lerp(position, mTargetPos, 1.0f - std::exp(-12.0f * dt));
			if (LengthSq(position - mTargetPos) < 1.0e-4f) { mAnimating = false; }
		}
	}

	void EditorCamera::Focus(const AABB& bounds)
	{
		if (!bounds.IsValid()) { return; }
		float radius = std::max(Length(bounds.GetExtents()), 0.5f);
		mOrbitDistance = radius / std::tan(math::ToRadians(fov) * 0.5f) * 1.2f;
		Vector3 forward = Vector3::kUnitZ * GetRotation();
		mTargetPos = bounds.GetCenter() - forward * mOrbitDistance;
		mAnimating = true;
	}

	RenderCamera EditorCamera::GetRenderCamera(float aspect) const
	{
		return RenderCamera::Make(position, GetRotation(), math::ToRadians(fov), aspect, 0.05f, 2000.0f);
	}
}
