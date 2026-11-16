#pragma once
#include "Core/Reflection/PropertyVisitor.h"
#include "Math/Matrix4.h"

namespace rd
{
	class Actor;

	// アクターの姿勢（ローカルの値を持ち、ワールド行列は必要になったときに計算して覚えておく）
	class Transform
	{
	public:
		explicit Transform(Actor* owner) : mOwner(owner) {}

		const Vector3& GetLocalPosition() const { return mPosition; }
		const Quaternion& GetLocalRotation() const { return mRotation; }
		const Vector3& GetLocalScale() const { return mScale; }
		void SetLocalPosition(const Vector3& p) { mPosition = p; MarkDirty(); }
		void SetLocalRotation(const Quaternion& r) { mRotation = Normalize(r); MarkDirty(); }
		void SetLocalScale(const Vector3& s) { mScale = s; MarkDirty(); }
		void SetLocal(const Vector3& p, const Quaternion& r, const Vector3& s);
		Matrix4 GetLocalMatrix() const { return Matrix4::CreateAffine(mScale, mRotation, mPosition); }

		const Matrix4& GetWorldMatrix() const;
		Vector3 GetPosition() const { return GetWorldMatrix().GetTranslation(); }
		Quaternion GetRotation() const;
		Vector3 GetLossyScale() const { return GetWorldMatrix().GetScale(); }
		void SetPosition(const Vector3& p);
		void SetRotation(const Quaternion& r);
		// 親を考えてローカルの値に直す
		void SetWorldMatrix(const Matrix4& m);

		Vector3 GetForward() const { return Normalize(GetWorldMatrix().GetAxisZ()); }
		Vector3 GetRight() const { return Normalize(GetWorldMatrix().GetAxisX()); }
		Vector3 GetUp() const { return Normalize(GetWorldMatrix().GetAxisY()); }

		void Translate(const Vector3& delta, bool world = true);
		void Rotate(const Quaternion& delta, bool world = true);
		void LookAt(const Vector3& target, const Vector3& up = Vector3::kUnitY);

		void Reflect(PropertyVisitor& v);
		// 自分と子のワールド行列を計算し直させる
		void MarkDirty();

	private:
		Actor* mOwner;
		Vector3 mPosition;
		Quaternion mRotation;
		Vector3 mScale = Vector3::kOne;
		mutable Matrix4 mWorld;
		mutable bool mDirty = true;
	};
}
