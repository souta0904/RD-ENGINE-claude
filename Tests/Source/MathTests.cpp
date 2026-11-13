#include "TestFramework.h"
#include "Math/Geometry.h"
#include "Math/Math.h"

using namespace rd;

namespace
{
	constexpr float kEps = 1.0e-4f;

	bool NearVec(const Vector3& a, const Vector3& b, float eps = kEps)
	{
		return rdtest::Near(a.x, b.x, eps) && rdtest::Near(a.y, b.y, eps) && rdtest::Near(a.z, b.z, eps);
	}
	bool NearMat(const Matrix4& a, const Matrix4& b, float eps = kEps)
	{
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				if (!rdtest::Near(a.m[i][j], b.m[i][j], eps)) { return false; }
			}
		}
		return true;
	}
	// q と -q は同じ回転
	bool SameRotation(const Quaternion& a, const Quaternion& b, float eps = kEps)
	{
		return rdtest::Near(std::fabs(Dot(Normalize(a), Normalize(b))), 1.0f, eps);
	}
}

RD_TEST(Math, Matrix4InverseOfGeneralMatrix)
{
	// アフィンでない（射影を含む）行列でも逆行列が求まる
	Matrix4 m = Matrix4::CreateAffine({ 2, 3, 0.5f }, Quaternion::FromEuler({ 0.3f, 1.1f, -0.7f }), { 4, -2, 9 })
		* Matrix4::CreatePerspectiveFov(1.0f, 16.0f / 9.0f, 0.1f, 100.0f);
	RD_CHECK(NearMat(m * Inverse(m), Matrix4::kIdentity, 1.0e-3f));
	RD_CHECK(NearMat(Inverse(m) * m, Matrix4::kIdentity, 1.0e-3f));
}

RD_TEST(Math, Matrix4Determinant)
{
	RD_CHECK_NEAR(Determinant(Matrix4::CreateScale({ 2, 3, 4 })), 24.0f, kEps);
	RD_CHECK_NEAR(Determinant(Matrix4::CreateRotate(Quaternion::FromEuler({ 0.4f, 0.2f, 1.3f }))), 1.0f, kEps);
}

RD_TEST(Math, Matrix3Inverse)
{
	// 旧エンジンでは行列式の式が間違っていた
	Matrix3 m = Matrix3::CreateAffine({ 2, 0.5f }, 0.8f, { 3, -4 });
	RD_CHECK_NEAR(Determinant(m), 1.0f, kEps);
	Matrix3 r = m * Inverse(m);
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j) { RD_CHECK_NEAR(r.m[i][j], i == j ? 1.0f : 0.0f, kEps); }
	}
}

RD_TEST(Math, AffineDecomposeRoundTrip)
{
	Vector3 s(1.5f, 2.0f, 0.75f), t(-3, 7, 2);
	Quaternion r = Quaternion::FromEuler({ 0.2f, -1.0f, 0.6f });
	Vector3 s2, t2;
	Quaternion r2;
	Matrix4::CreateAffine(s, r, t).Decompose(s2, r2, t2);
	RD_CHECK(NearVec(s, s2));
	RD_CHECK(NearVec(t, t2));
	RD_CHECK(SameRotation(r, r2));
}

RD_TEST(Math, QuaternionRotatesLikeMatrix)
{
	Quaternion q = Quaternion::AngleAxis(Vector3::kUnitY, math::ToRadians(90.0f));
	// 左手座標系: Y 軸まわりに +90 度で +Z は +X へ
	RD_CHECK(NearVec(Vector3::kUnitZ * q, Vector3::kUnitX));
	Vector3 v(1, 2, 3);
	RD_CHECK(NearVec(v * q, TransformVector(v, Matrix4::CreateRotate(q))));
}

RD_TEST(Math, QuaternionOrder)
{
	// a * b は「a の後に b」
	Quaternion a = Quaternion::AngleAxis(Vector3::kUnitX, 0.7f);
	Quaternion b = Quaternion::AngleAxis(Vector3::kUnitY, -1.2f);
	Vector3 v(0.3f, -1, 2);
	RD_CHECK(NearVec(v * (a * b), (v * a) * b));
	RD_CHECK(NearMat(Matrix4::CreateRotate(a * b), Matrix4::CreateRotate(a) * Matrix4::CreateRotate(b)));
}

RD_TEST(Math, EulerRoundTrip)
{
	Vector3 e(0.4f, -0.9f, 1.3f);
	RD_CHECK(SameRotation(Quaternion::FromEuler(Quaternion::FromEuler(e).ToEuler()), Quaternion::FromEuler(e)));
}

RD_TEST(Math, QuaternionFromMatrixAndLookRotation)
{
	Quaternion q = Quaternion::FromEuler({ 1.0f, 0.5f, -0.25f });
	RD_CHECK(SameRotation(Quaternion::FromMatrix(Matrix4::CreateRotate(q)), q));

	Vector3 dir = Normalize(Vector3(1, 1, -2));
	RD_CHECK(NearVec(Vector3::kUnitZ * Quaternion::LookRotation(dir), dir));
	RD_CHECK(NearVec(Vector3(0, 0, 1) * Quaternion::FromTo({ 0, 0, 1 }, { 0, 0, -1 }), { 0, 0, -1 }));
}

RD_TEST(Math, SlerpEndpointsAndMidpoint)
{
	Quaternion a = Quaternion::kIdentity;
	Quaternion b = Quaternion::AngleAxis(Vector3::kUnitZ, math::ToRadians(90.0f));
	RD_CHECK(SameRotation(Slerp(a, b, 0.0f), a));
	RD_CHECK(SameRotation(Slerp(a, b, 1.0f), b));
	RD_CHECK(SameRotation(Slerp(a, b, 0.5f), Quaternion::AngleAxis(Vector3::kUnitZ, math::ToRadians(45.0f))));
}

RD_TEST(Math, LookAtMovesTargetToForward)
{
	Vector3 eye(3, 4, -5), target(0, 1, 2);
	Matrix4 view = Matrix4::CreateLookAt(eye, target, Vector3::kUnitY);
	Vector3 p = TransformPoint(target, view);
	RD_CHECK_NEAR(p.x, 0.0f, kEps);
	RD_CHECK_NEAR(p.y, 0.0f, kEps);
	RD_CHECK_NEAR(p.z, Distance(eye, target), kEps);
}

RD_TEST(Geometry, FrustumCulling)
{
	Matrix4 vp = Matrix4::CreateLookAt({ 0, 0, 0 }, { 0, 0, 1 }, Vector3::kUnitY)
		* Matrix4::CreatePerspectiveFov(math::ToRadians(60.0f), 1.0f, 0.1f, 100.0f);
	Frustum f = Frustum::FromViewProj(vp);
	RD_CHECK(f.Intersects(AABB::FromCenterExtents({ 0, 0, 10 }, { 1, 1, 1 })));
	RD_CHECK(!f.Intersects(AABB::FromCenterExtents({ 0, 0, -10 }, { 1, 1, 1 })));// 後ろ
	RD_CHECK(!f.Intersects(AABB::FromCenterExtents({ 0, 0, 200 }, { 1, 1, 1 })));// 遠すぎる
	RD_CHECK(!f.Intersects(AABB::FromCenterExtents({ 50, 0, 10 }, { 1, 1, 1 })));// 横
}

RD_TEST(Geometry, RayIntersections)
{
	Ray ray{ { 0, 0, -10 }, { 0, 0, 1 } };
	float t = 0;
	RD_CHECK(RayIntersectsAABB(ray, AABB({ -1, -1, -1 }, { 1, 1, 1 }), t));
	RD_CHECK_NEAR(t, 9.0f, kEps);
	RD_CHECK(!RayIntersectsAABB(ray, AABB({ 2, 2, -1 }, { 3, 3, 1 }), t));

	RD_CHECK(RayIntersectsTriangle(ray, { -1, -1, 0 }, { 1, -1, 0 }, { 0, 1, 0 }, t));
	RD_CHECK_NEAR(t, 10.0f, kEps);
	RD_CHECK(!RayIntersectsTriangle(ray, { 1, 1, 0 }, { 2, 1, 0 }, { 1, 2, 0 }, t));
}

RD_TEST(Geometry, AABBTransform)
{
	AABB box({ -1, -1, -1 }, { 1, 1, 1 });
	AABB moved = box.Transform(Matrix4::CreateAffine({ 2, 2, 2 }, Quaternion::AngleAxis(Vector3::kUnitY, math::ToRadians(45.0f)), { 10, 0, 0 }));
	RD_CHECK(NearVec(moved.GetCenter(), { 10, 0, 0 }));
	RD_CHECK_NEAR(moved.GetExtents().x, 2.0f * std::sqrt(2.0f), 1.0e-3f);
	RD_CHECK_NEAR(moved.GetExtents().y, 2.0f, kEps);
}
