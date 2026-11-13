#include "TestFramework.h"
#include "Physics/Shapes.h"

using namespace rd;

namespace
{
	WorldShape Sphere(const Vector3& c, float r)
	{
		WorldShape s;
		s.type = ShapeType::Sphere;
		s.center = c;
		s.radius = r;
		s.ComputeBounds();
		return s;
	}
	WorldShape Box(const Vector3& c, const Vector3& half)
	{
		WorldShape s;
		s.type = ShapeType::Box;
		s.center = c;
		s.halfExtents = half;
		s.ComputeBounds();
		return s;
	}
	// 外向きの面を持つ箱のメッシュ（エンジンと同じく、表は Cross(c - a, b - a) の向き）
	void BuildBoxMesh(MeshCollisionData& mesh, const Vector3& half)
	{
		std::vector<Vector3> positions;
		std::vector<uint32_t> indices;
		const Vector3 normals[6] = { Vector3::kUnitX, -Vector3::kUnitX, Vector3::kUnitY, -Vector3::kUnitY, Vector3::kUnitZ, -Vector3::kUnitZ };
		for (const Vector3& n : normals)
		{
			Vector3 u = std::fabs(n.y) > 0.5f ? Vector3::kUnitX : Vector3::kUnitY;
			Vector3 v = Cross(u, n);// Cross(v, u) = n になる
			Vector3 p = n * half, su = u * half, sv = v * half;
			uint32_t base = uint32_t(positions.size());
			positions.insert(positions.end(), { p - su - sv, p - su + sv, p + su - sv, p + su + sv });
			indices.insert(indices.end(), { base, base + 2, base + 1, base + 1, base + 2, base + 3 });
		}
		mesh.Build(std::move(positions), std::move(indices));
	}

	WorldShape MeshShape(const MeshCollisionData& mesh, const Vector3& position, const Quaternion& rotation = Quaternion::kIdentity)
	{
		WorldShape s;
		s.type = ShapeType::Mesh;
		s.mesh = &mesh;
		s.meshPosition = position;
		s.meshRotation = rotation;
		s.center = position;
		s.ComputeBounds();
		return s;
	}

	WorldShape Capsule(const Vector3& p0, const Vector3& p1, float r)
	{
		WorldShape s;
		s.type = ShapeType::Capsule;
		s.p0 = p0;
		s.p1 = p1;
		s.center = (p0 + p1) * 0.5f;
		s.radius = r;
		s.ComputeBounds();
		return s;
	}
}

RD_TEST(Physics, SphereSphere)
{
	std::vector<ContactPoint> contacts;
	RD_REQUIRE(CollideShapes(Sphere({ 0, 0, 0 }, 1), Sphere({ 1.5f, 0, 0 }, 1), contacts));
	RD_REQUIRE(!contacts.empty());
	RD_CHECK_NEAR(contacts[0].depth, 0.5f, 1.0e-4f);
	RD_CHECK_NEAR(contacts[0].normal.x, 1.0f, 1.0e-4f);// A から B へ
	contacts.clear();
	RD_CHECK(!CollideShapes(Sphere({ 0, 0, 0 }, 1), Sphere({ 2.5f, 0, 0 }, 1), contacts));
}

RD_TEST(Physics, SphereBoxAndBoxBox)
{
	std::vector<ContactPoint> contacts;
	RD_REQUIRE(CollideShapes(Box({ 0, 0, 0 }, { 1, 1, 1 }), Sphere({ 0, 1.8f, 0 }, 1), contacts));
	RD_REQUIRE(!contacts.empty());
	RD_CHECK_NEAR(contacts[0].depth, 0.2f, 1.0e-3f);
	RD_CHECK_NEAR(contacts[0].normal.y, 1.0f, 1.0e-3f);

	contacts.clear();
	RD_CHECK(CollideShapes(Box({ 0, 0, 0 }, { 1, 1, 1 }), Box({ 1.9f, 0.5f, 0 }, { 1, 1, 1 }), contacts));
	contacts.clear();
	RD_CHECK(!CollideShapes(Box({ 0, 0, 0 }, { 1, 1, 1 }), Box({ 2.1f, 0, 0 }, { 1, 1, 1 }), contacts));
}

RD_TEST(Physics, CapsuleOnBox)
{
	std::vector<ContactPoint> contacts;
	// 横倒しのカプセルが箱の上面に少しめり込んでいる
	RD_REQUIRE(CollideShapes(Box({ 0, 0, 0 }, { 5, 1, 5 }), Capsule({ -1, 1.4f, 0 }, { 1, 1.4f, 0 }, 0.5f), contacts));
	RD_REQUIRE(!contacts.empty());
	for (const auto& c : contacts)
	{
		RD_CHECK_NEAR(c.depth, 0.1f, 1.0e-3f);
		RD_CHECK_NEAR(c.normal.y, 1.0f, 1.0e-3f);
	}
}

RD_TEST(Physics, RaycastShapes)
{
	Ray ray{ { 0, 0, -10 }, { 0, 0, 1 } };
	float t = 0;
	Vector3 n;
	RD_CHECK(RaycastShape(Sphere({ 0, 0, 0 }, 1), ray, 100.0f, t, n));
	RD_CHECK_NEAR(t, 9.0f, 1.0e-4f);
	RD_CHECK_NEAR(n.z, -1.0f, 1.0e-4f);
	RD_CHECK(!RaycastShape(Sphere({ 0, 0, 0 }, 1), ray, 5.0f, t, n));// 届かない
	RD_CHECK(RaycastShape(Box({ 0, 0, 0 }, { 2, 2, 2 }), ray, 100.0f, t, n));
	RD_CHECK_NEAR(t, 8.0f, 1.0e-4f);
}

RD_TEST(Physics, MeshBvhRaycastAndQuery)
{
	// 10x10 マスの床（200 三角形）
	std::vector<Vector3> positions;
	std::vector<uint32_t> indices;
	const int n = 10;
	for (int z = 0; z <= n; ++z)
	{
		for (int x = 0; x <= n; ++x) { positions.push_back({ float(x), 0.0f, float(z) }); }
	}
	for (int z = 0; z < n; ++z)
	{
		for (int x = 0; x < n; ++x)
		{
			uint32_t i = uint32_t(z * (n + 1) + x);
			indices.insert(indices.end(), { i, i + n + 1, i + 1, i + 1, i + n + 1, i + n + 2 });
		}
	}
	MeshCollisionData mesh;
	mesh.Build(positions, indices);
	RD_CHECK_EQ(mesh.GetTriangleCount(), size_t(200));

	float t = 0;
	Vector3 normal;
	RD_REQUIRE(mesh.Raycast({ { 3.3f, 5, 7.7f }, { 0, -1, 0 } }, 100.0f, t, normal));
	RD_CHECK_NEAR(t, 5.0f, 1.0e-4f);
	RD_CHECK(!mesh.Raycast({ { 30, 5, 30 }, { 0, -1, 0 } }, 100.0f, t, normal));

	int hits = 0;
	mesh.Query(AABB({ 2.1f, -1, 2.1f }, { 2.9f, 1, 2.9f }), [&](uint32_t) { ++hits; });
	RD_CHECK(hits >= 2 && hits < 20);// 1 マス分の 2 三角形 + BVH の余り（全部は調べない）

	std::vector<ContactPoint> contacts;
	RD_CHECK(CollideShapes(Sphere({ 5, 0.8f, 5 }, 1), MeshShape(mesh, Vector3::kZero), contacts));
}

RD_TEST(Physics, MovedAndRotatedMesh)
{
	// 動くメッシュ: 三角形はローカルのまま、位置と回転だけを変える
	MeshCollisionData box;
	BuildBoxMesh(box, { 1, 1, 1 });
	WorldShape mesh = MeshShape(box, { 10, 0, 0 }, Quaternion::AngleAxis(Vector3::kUnitY, math::ToRadians(45.0f)));
	RD_CHECK_NEAR(mesh.bounds.GetCenter().x, 10.0f, 1.0e-4f);
	RD_CHECK_NEAR(mesh.bounds.GetExtents().x, std::sqrt(2.0f), 1.0e-3f);// 45 度回すと広がる

	std::vector<ContactPoint> contacts;
	RD_REQUIRE(CollideShapes(Sphere({ 10, 1.3f, 0 }, 0.5f), mesh, contacts));
	RD_CHECK_NEAR(contacts[0].depth, 0.2f, 1.0e-3f);
	RD_CHECK_NEAR(contacts[0].normal.y, -1.0f, 1.0e-3f);// 球 → メッシュ
	contacts.clear();
	RD_CHECK(!CollideShapes(Sphere({ 0, 1.3f, 0 }, 0.5f), mesh, contacts));// 元の場所にはもう無い

	float t = 0;
	Vector3 n;
	RD_REQUIRE(RaycastShape(mesh, { { 10, 5, 0 }, { 0, -1, 0 } }, 100.0f, t, n));
	RD_CHECK_NEAR(t, 4.0f, 1.0e-4f);
	RD_CHECK_NEAR(n.y, 1.0f, 1.0e-4f);
	// 横から: 45 度回した箱の角は √2 の位置
	RD_REQUIRE(RaycastShape(mesh, { { 0, 0, 0 }, { 1, 0, 0 } }, 100.0f, t, n));
	RD_CHECK_NEAR(t, 10.0f - std::sqrt(2.0f), 1.0e-3f);
}

RD_TEST(Physics, MeshOnMesh)
{
	// 箱のメッシュが床のメッシュに少しめり込んでいる
	MeshCollisionData box, floor;
	BuildBoxMesh(box, { 0.5f, 0.5f, 0.5f });
	BuildBoxMesh(floor, { 10.0f, 1.0f, 10.0f });
	WorldShape a = MeshShape(box, { 0, 1.45f, 0 }, Quaternion::AngleAxis(Vector3::kUnitY, 0.3f));
	WorldShape b = MeshShape(floor, Vector3::kZero);
	std::vector<ContactPoint> contacts;
	RD_REQUIRE(CollideShapes(a, b, contacts));
	RD_CHECK(contacts.size() >= 3);// 底の 4 頂点（最大 4）
	for (auto& c : contacts)
	{
		RD_CHECK_NEAR(c.depth, 0.05f, 1.0e-3f);
		RD_CHECK_NEAR(c.normal.y, -1.0f, 1.0e-3f);// A（箱）から B（床）へ
	}
	// 順番を入れ替えると法線も逆になる
	contacts.clear();
	RD_REQUIRE(CollideShapes(b, a, contacts));
	RD_CHECK_NEAR(contacts[0].normal.y, 1.0f, 1.0e-3f);
	// 離れていれば当たらない
	contacts.clear();
	RD_CHECK(!CollideShapes(MeshShape(box, { 0, 1.6f, 0 }), b, contacts));
}

RD_TEST(Physics, ConvexMeshThroughThinFloor)
{
	// 動く物体のメッシュ（凸包）が、薄い床（厚さ 2cm）を一歩で深く突き抜けても上へ押し返される
	MeshCollisionData box;
	BuildBoxMesh(box, { 0.5f, 0.5f, 0.5f });
	RD_CHECK_EQ(box.GetHullPoints().size(), size_t(8));// 同じ位置の頂点はまとめる
	RD_CHECK_EQ(box.GetHullPlanes().size(), size_t(6));
	WorldShape floor = Box({ 0, 0, 0 }, { 30, 0.01f, 30 });
	WorldShape cube = MeshShape(box, { 0.2f, 0.3f, 0 }, Quaternion::AngleAxis(Vector3::kUnitY, 0.5f));// 底が床の下 0.2
	cube.meshConvex = true;
	std::vector<ContactPoint> contacts;
	RD_REQUIRE(CollideShapes(cube, floor, contacts));
	RD_CHECK_EQ(contacts.size(), size_t(4));// 底の 4 隅
	for (auto& c : contacts)
	{
		RD_CHECK_NEAR(c.normal.y, -1.0f, 1.0e-3f);// 箱 → 床 = 下向き（箱は上へ押し返される）
		RD_CHECK(std::fabs(c.point.x - 0.2f) < 0.8f);// 接触点は箱の近く（床の遠い角ではない）
	}

	// 球が凸包の中に入り込んでいても外へ押し出す
	contacts.clear();
	RD_REQUIRE(CollideShapes(Sphere({ 0.2f, 0.65f, 0 }, 0.2f), cube, contacts));
	RD_CHECK(contacts[0].normal.y < -0.9f);// 球 → 箱 = 下向き（球は上へ）

	// 凸包同士
	WorldShape other = MeshShape(box, { 0.2f, 1.2f, 0 });
	other.meshConvex = true;
	contacts.clear();
	RD_REQUIRE(CollideShapes(other, cube, contacts));
	RD_CHECK(contacts[0].normal.y < -0.9f);
	RD_CHECK_NEAR(contacts[0].depth, 0.1f, 1.0e-3f);
}

RD_TEST(Physics, MeshOnThinBox)
{
	// 薄い箱の床に、傾いた箱のメッシュの角が刺さっている
	MeshCollisionData box;
	BuildBoxMesh(box, { 0.5f, 0.5f, 0.5f });
	WorldShape floor = Box({ 0, 0, 0 }, { 5, 0.01f, 5 });
	WorldShape cube = MeshShape(box, { 0, 0.6f, 0 }, Quaternion::FromEuler({ math::ToRadians(20.0f), 0.0f, math::ToRadians(10.0f) }));
	std::vector<ContactPoint> contacts;
	RD_REQUIRE(CollideShapes(cube, floor, contacts));
	for (auto& c : contacts) { RD_CHECK(c.normal.y < -0.9f); }// 箱（A）から床（B）へ = 下向き
	contacts.clear();
	RD_REQUIRE(CollideShapes(floor, cube, contacts));
	for (auto& c : contacts) { RD_CHECK(c.normal.y > 0.9f); }
}
