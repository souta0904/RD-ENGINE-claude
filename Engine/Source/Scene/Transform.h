#pragma once
#include "Core/Reflection.h"
#include "Math/Math.h"

namespace rd
{
	class Actor;

	// アクターの姿勢（ローカル値を持ち、ワールド行列は必要になった時に計算する）
	class Transform
	{
	public:
		explicit Transform(Actor* owner) : mOwner(owner) {}

		// ローカル
		const Vector3& GetLocalPosition() const { return mPosition; }
		const Quaternion& GetLocalRotation() const { return mRotation; }
		const Vector3& GetLocalScale() const { return mScale; }
		void SetLocalPosition(const Vector3& p) { mPosition = p; MarkDirty(); }
		void SetLocalRotation(const Quaternion& r) { mRotation = Normalize(r); MarkDirty(); }
		void SetLocalScale(const Vector3& s) { mScale = s; MarkDirty(); }
		void SetLocal(const Vector3& p, const Quaternion& r, const Vector3& s);
		Matrix4 GetLocalMatrix() const { return Matrix4::CreateAffine(mScale, mRotation, mPosition); }

		// ワールド
		const Matrix4& GetWorldMatrix() const;
		Vector3 GetPosition() const { return GetWorldMatrix().GetTranslation(); }
		Quaternion GetRotation() const;
		Vector3 GetLossyScale() const { return GetWorldMatrix().GetScale(); }
		void SetPosition(const Vector3& p);
		void SetRotation(const Quaternion& r);
		// ワールド行列を指定（親を考慮してローカル値へ変換）
		void SetWorldMatrix(const Matrix4& m);

		// 向き
		Vector3 GetForward() const { return Normalize(GetWorldMatrix().GetAxisZ()); }
		Vector3 GetRight() const { return Normalize(GetWorldMatrix().GetAxisX()); }
		Vector3 GetUp() const { return Normalize(GetWorldMatrix().GetAxisY()); }

		void Translate(const Vector3& delta, bool world = true);
		void Rotate(const Quaternion& delta, bool world = true);
		void LookAt(const Vector3& target, const Vector3& up = Vector3::kUnitY);

		void Reflect(PropertyVisitor& v);
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
