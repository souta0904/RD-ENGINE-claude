#include "TestFramework.h"
#include "AI/Navigation/NavMesh.h"
#include "AI/Navigation/NavMeshBuilder.h"

using namespace rd;

// ナビメッシュを焼く（NavMeshBuilder）を、簡単な形で確かめる
namespace
{
	// 四角形（a→b→c→d が上から見て時計回りなら上向き）を 2 枚の三角形で足す
	void AddQuad(NavMeshBuilder& b, const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3)
	{
		b.AddTriangles({ p0, p1, p2, p3 }, { 0, 1, 2, 0, 2, 3 });
	}

	// 床: x0～x1, z0～z1 の高さ y の上向きの面
	void AddFloor(NavMeshBuilder& b, float x0, float z0, float x1, float z1, float y)
	{
		AddQuad(b, { x0, y, z0 }, { x0, y, z1 }, { x1, y, z1 }, { x1, y, z0 });
	}

	// 箱（上面と 4 つの側面。底は床に重なるので省く）
	void AddBox(NavMeshBuilder& b, const Vector3& mn, const Vector3& mx)
	{
		AddFloor(b, mn.x, mn.z, mx.x, mx.z, mx.y);
		AddQuad(b, { mn.x, mn.y, mn.z }, { mn.x, mx.y, mn.z }, { mx.x, mx.y, mn.z }, { mx.x, mn.y, mn.z });
		AddQuad(b, { mx.x, mn.y, mx.z }, { mx.x, mx.y, mx.z }, { mn.x, mx.y, mx.z }, { mn.x, mn.y, mx.z });
		AddQuad(b, { mn.x, mn.y, mx.z }, { mn.x, mx.y, mx.z }, { mn.x, mx.y, mn.z }, { mn.x, mn.y, mn.z });
		AddQuad(b, { mx.x, mn.y, mn.z }, { mx.x, mx.y, mn.z }, { mx.x, mx.y, mx.z }, { mx.x, mn.y, mx.z });
	}

	NavMeshBuildSettings Settings()
	{
		NavMeshBuildSettings s;
		s.agentRadius = 0.5f;
		s.agentHeight = 2.0f;
		s.agentMaxSlope = 45.0f;
		s.agentClimb = 0.4f;
		s.cellSize = 0.1f;
		s.minRegionArea = 1.0f;
		return s;
	}

	// 焼いた三角形を上から見た面積の合計
	float TopArea(const NavMeshBuildResult& r)
	{
		float sum = 0.0f;
		for (size_t i = 0; i + 2 < r.indices.size(); i += 3)
		{
			const Vector3& a = r.vertices[r.indices[i]];
			const Vector3& b = r.vertices[r.indices[i + 1]];
			const Vector3& c = r.vertices[r.indices[i + 2]];
			sum += std::fabs((b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x)) * 0.5f;
		}
		return sum;
	}

	float PathLength(const NavMeshPath& path)
	{
		float len = 0.0f;
		for (size_t i = 1; i < path.corners.size(); ++i) { len += Length(path.corners[i] - path.corners[i - 1]); }
		return len;
	}
}

// 10m 四方の床は、縁から半径 0.5m を削った 9m 四方になる
RD_TEST(NavMeshBuilder, FlatFloorIsErodedByRadius)
{
	NavMeshBuilder builder;
	AddFloor(builder, 0, 0, 10, 10, 0);
	NavMeshBuildResult result;
	RD_REQUIRE(builder.Build(Settings(), result));
	RD_CHECK_NEAR(TopArea(result), 81.0f, 2.0f);
	for (const Vector3& v : result.vertices)
	{
		RD_CHECK(v.x >= 0.45f && v.x <= 9.55f && v.z >= 0.45f && v.z <= 9.55f);
		RD_CHECK_NEAR(v.y, 0.0f, 0.06f);
	}
	// 平らなので四角形はわずかな数にまとまる（1 辺 4.8m まで）
	RD_CHECK(result.GetTriangleCount() <= 32u);
}

// 下を向いた面（天井）と急な坂は歩けない
RD_TEST(NavMeshBuilder, CeilingAndSteepSlopeAreNotWalkable)
{
	NavMeshBuilder ceiling;
	AddQuad(ceiling, { 0, 0, 0 }, { 10, 0, 0 }, { 10, 0, 10 }, { 0, 0, 10 });
	NavMeshBuildResult result;
	RD_CHECK(!ceiling.Build(Settings(), result));

	NavMeshBuilder slope;
	AddQuad(slope, { 0, 0, 0 }, { 0, 10, 5 }, { 10, 10, 5 }, { 10, 0, 0 }); // 約 63 度
	RD_CHECK(!slope.Build(Settings(), result));

	NavMeshBuilder gentle;
	AddQuad(gentle, { 0, 0, 0 }, { 0, 2, 10 }, { 10, 2, 10 }, { 10, 0, 0 }); // 約 11 度
	RD_REQUIRE(gentle.Build(Settings(), result));
	// 坂に沿った高さになっている
	for (const Vector3& v : result.vertices) { RD_CHECK_NEAR(v.y, v.z * 0.2f, 0.15f); }
}

// 真ん中の壁を回り込む経路が見つかる
RD_TEST(NavMeshBuilder, PathGoesAroundWall)
{
	NavMeshBuilder builder;
	AddFloor(builder, 0, 0, 10, 10, 0);
	AddBox(builder, { 4.5f, 0, 0 }, { 5.5f, 3, 7 });
	NavMeshBuildResult result;
	RD_REQUIRE(builder.Build(Settings(), result));

	NavMesh mesh;
	mesh.Build(result.vertices, result.indices, result.areas);
	NavMeshPath path;
	RD_REQUIRE(mesh.CalculatePath({ 2, 0, 2 }, { 8, 0, 2 }, path));
	RD_CHECK(path.status == NavMeshPath::Status::Complete);
	// 壁（z < 7）の先、半径の分だけ離れた z >= 7.4 あたりを通る
	bool around = false;
	for (const Vector3& c : path.corners) { around |= c.z > 7.3f; }
	RD_CHECK(around);
	RD_CHECK(PathLength(path) > 12.0f);
	// 壁の上（高さ 3m）は幅 1m しかないので、削られて残らない
	for (const Vector3& v : result.vertices) { RD_CHECK(v.y < 0.5f); }
}

// 段差は agentClimb 以内なら登れて、超えると行けない
RD_TEST(NavMeshBuilder, StepIsClimbableWithinClimbHeight)
{
	auto build = [](float climb, NavMeshPath& path)
	{
		NavMeshBuilder builder;
		AddFloor(builder, 0, 0, 5, 6, 0);
		AddBox(builder, { 5, 0, 0 }, { 10, 0.3f, 6 });
		NavMeshBuildSettings s = Settings();
		s.agentClimb = climb;
		NavMeshBuildResult result;
		if (!builder.Build(s, result)) { return false; }
		NavMesh mesh;
		mesh.Build(result.vertices, result.indices, result.areas);
		return mesh.CalculatePath({ 2, 0, 3 }, { 8, 0.3f, 3 }, path);
	};

	NavMeshPath path;
	RD_REQUIRE(build(0.4f, path));
	RD_CHECK(path.status == NavMeshPath::Status::Complete);
	RD_CHECK_NEAR(path.corners.back().y, 0.3f, 0.1f);

	RD_REQUIRE(build(0.2f, path));
	RD_CHECK(path.status == NavMeshPath::Status::Partial);
}

// 背より低い隙間（テーブルの下など）は通れない
RD_TEST(NavMeshBuilder, LowCeilingBlocksPassage)
{
	NavMeshBuilder builder;
	AddFloor(builder, 0, 0, 10, 4, 0);
	// 高さ 1m のところに板（下向きの面も上向きの面もある厚い板）で床の真ん中をふさぐ
	AddBox(builder, { 4, 1.0f, -1 }, { 6, 1.2f, 5 });
	NavMeshBuildResult result;
	RD_REQUIRE(builder.Build(Settings(), result));
	NavMesh mesh;
	mesh.Build(result.vertices, result.indices, result.areas);
	NavMeshPath path;
	RD_REQUIRE(mesh.CalculatePath({ 1, 0, 2 }, { 9, 0, 2 }, path));
	RD_CHECK(path.status == NavMeshPath::Status::Partial);
}
