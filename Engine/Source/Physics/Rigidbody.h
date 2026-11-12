#pragma once
#include "Scene/Component.h"
#include "Math/Math.h"

namespace rd
{
	enum class ForceMode : uint8_t
	{
		Force,// 質量を考慮して継続的に加える（N）
		Acceleration,// 質量を無視して継続的に加える
		Impulse,// 質量を考慮して瞬間的に加える（N*s）
		VelocityChange// 質量を無視して瞬間的に速度を変える
	};

	// 剛体（物理シミュレーションで動くようにする）
	class Rigidbody final : public Component
	{
		RD_COMPONENT(Rigidbody)
	public:
		float mass = 1.0f;
		float drag = 0.05f;
		float angularDrag = 0.05f;
		bool useGravity = true;
		bool isKinematic = false;// 物理では動かさず、スクリプトで動かす
		bool freezeRotation = false;
		Vector3 velocity;
		Vector3 angularVelocity;

		void Reflect(PropertyVisitor& v) override;

		void AddForce(const Vector3& force, ForceMode mode = ForceMode::Force);
		void AddTorque(const Vector3& torque, ForceMode mode = ForceMode::Force);
		void AddForceAtPosition(const Vector3& force, const Vector3& position, ForceMode mode = ForceMode::Force);

		bool IsDynamic() const { return !isKinematic && mass > 0.0f; }
		float GetInverseMass() const { return IsDynamic() ? 1.0f / mass : 0.0f; }
		// 直前の物理ステップで地面（上向きの面）に触れていたか
		bool IsGrounded() const { return mGrounded; }
		const Vector3& GetGroundNormal() const { return mGroundNormal; }

	private:
		friend class PhysicsWorld;
		Vector3 mForce;
		Vector3 mTorque;
		Vector3 mInvInertiaLocal;
		bool mGrounded = false;
		Vector3 mGroundNormal = Vector3::kUnitY;
	};
}
