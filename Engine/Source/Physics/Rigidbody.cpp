#include "RdPch.h"
#include "Physics/Rigidbody.h"
#include "Scene/Actor.h"

namespace rd
{
	void Rigidbody::Reflect(PropertyVisitor& v)
	{
		v.Property("Mass", mass, { .speed = 0.05f, .min = 0.001f, .max = 100000.0f });
		v.Property("Drag", drag, { .speed = 0.01f, .min = 0.0f, .max = 100.0f });
		v.Property("Angular Drag", angularDrag, { .speed = 0.01f, .min = 0.0f, .max = 100.0f });
		v.Property("Use Gravity", useGravity);
		v.Property("Is Kinematic", isKinematic);
		v.Property("Freeze Rotation", freezeRotation);
		v.Property("Velocity", velocity);
		v.Property("Angular Velocity", angularVelocity);
	}

	void Rigidbody::AddForce(const Vector3& force, ForceMode mode)
	{
		if (!IsDynamic()) { return; }
		switch (mode)
		{
		case ForceMode::Force: mForce += force; break;
		case ForceMode::Acceleration: mForce += force * mass; break;
		case ForceMode::Impulse: velocity += force / mass; break;
		case ForceMode::VelocityChange: velocity += force; break;
		}
	}

	void Rigidbody::AddTorque(const Vector3& torque, ForceMode mode)
	{
		if (!IsDynamic() || freezeRotation) { return; }
		switch (mode)
		{
		case ForceMode::Force:
		case ForceMode::Acceleration: mTorque += torque; break;
		case ForceMode::Impulse:
		case ForceMode::VelocityChange: angularVelocity += torque; break;
		}
	}

	void Rigidbody::AddForceAtPosition(const Vector3& force, const Vector3& position, ForceMode mode)
	{
		AddForce(force, mode);
		AddTorque(Cross(position - GetTransform().GetPosition(), force), mode);
	}
}
