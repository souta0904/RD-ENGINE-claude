#include "RdPch.h"
#include "Scene/Transform.h"
#include "Scene/Actor.h"

namespace rd
{
	void Transform::SetLocal(const Vector3& p, const Quaternion& r, const Vector3& s)
	{
		mPosition = p;
		mRotation = Normalize(r);
		mScale = s;
		MarkDirty();
	}

	const Matrix4& Transform::GetWorldMatrix() const
	{
		if (mDirty)
		{
			Matrix4 local = GetLocalMatrix();
			Actor* parent = mOwner ? mOwner->GetParent() : nullptr;
			mWorld = parent ? local * parent->GetTransform().GetWorldMatrix() : local;
			mDirty = false;
		}
		return mWorld;
	}

	Quaternion Transform::GetRotation() const
	{
		Actor* parent = mOwner ? mOwner->GetParent() : nullptr;
		return parent ? mRotation * parent->GetTransform().GetRotation() : mRotation;
	}

	void Transform::SetPosition(const Vector3& p)
	{
		Actor* parent = mOwner ? mOwner->GetParent() : nullptr;
		SetLocalPosition(parent ? TransformPoint(p, Inverse(parent->GetTransform().GetWorldMatrix())) : p);
	}

	void Transform::SetRotation(const Quaternion& r)
	{
		Actor* parent = mOwner ? mOwner->GetParent() : nullptr;
		SetLocalRotation(parent ? r * Inverse(parent->GetTransform().GetRotation()) : r);
	}

	void Transform::SetWorldMatrix(const Matrix4& m)
	{
		Actor* parent = mOwner ? mOwner->GetParent() : nullptr;
		Matrix4 local = parent ? m * Inverse(parent->GetTransform().GetWorldMatrix()) : m;
		local.Decompose(mScale, mRotation, mPosition);
		MarkDirty();
	}

	void Transform::Translate(const Vector3& delta, bool world)
	{
		if (world) { SetPosition(GetPosition() + delta); }
		else { SetLocalPosition(mPosition + delta * mRotation); }
	}

	void Transform::Rotate(const Quaternion& delta, bool world)
	{
		if (world) { SetRotation(GetRotation() * delta); }
		else { SetLocalRotation(delta * mRotation); }
	}

	void Transform::LookAt(const Vector3& target, const Vector3& up)
	{
		Vector3 dir = target - GetPosition();
		if (LengthSq(dir) < 1.0e-8f) { return; }
		SetRotation(Quaternion::LookRotation(dir, up));
	}

	void Transform::Reflect(PropertyVisitor& v)
	{
		bool changed = false;
		changed |= v.Property("Position", mPosition, { .speed = 0.05f });
		changed |= v.Property("Rotation", mRotation, { .speed = 0.5f });
		changed |= v.Property("Scale", mScale, { .speed = 0.01f });
		if (changed)
		{
			mRotation = Normalize(mRotation);
			MarkDirty();
		}
	}

	void Transform::MarkDirty()
	{
		mDirty = true;
		if (!mOwner) { return; }
		for (Actor* child : mOwner->GetChildren())
		{
			child->GetTransform().MarkDirty();
		}
	}
}
