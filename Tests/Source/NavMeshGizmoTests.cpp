#include "TestFramework.h"
#include "AI/Navigation/NavMesh.h"
#include "Render/Debug/DebugDraw.h"

using namespace rd;

// ナビメッシュの外周（隣の三角形がない辺）の求め方を確かめる

// 2 枚の三角形で作った正方形：共有している対角線は外周にならず、4 辺だけが外周になる
RD_TEST(NavMeshGizmo, SquareOutlineHasFourSides)
{
	NavMesh mesh;
	mesh.Build({ { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } }, { 0, 1, 2, 0, 2, 3 }, {});
	RD_CHECK_EQ(mesh.GetOutlineSegmentCount(), size_t(4));
}

// タイルの境目（T 字）：左の大きな三角形の右辺が、右の 2 枚の三角形と半分ずつつながる
//  左の右辺は全体が覆われるので外周にならない
RD_TEST(NavMeshGizmo, TJunctionIsNotOutline)
{
	NavMesh mesh;
	// 左：(0,0)-(1,0)-(1,2)・(0,0)-(1,2)-(0,2) の 2 枚で 1x2 の長方形。右辺は x=1 の (1,0)～(1,2)
	// 右：x=1～2 を z=1 で上下に分けた長方形 2 つ（各 2 枚）。左辺の頂点 (1,1) は左側にない
	std::vector<Vector3> v = {
		{ 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 2 }, { 0, 0, 2 },
		{ 1, 0, 1 }, { 2, 0, 0 }, { 2, 0, 1 }, { 2, 0, 2 } };
	std::vector<uint32_t> idx = {
		0, 1, 2, 0, 2, 3,          // 左
		1, 5, 6, 1, 6, 4,          // 右下
		4, 6, 7, 4, 7, 2 };        // 右上
	mesh.Build(v, idx, {});

	// 外周は 2x2 の正方形の周り。左：上・下・左 の 3 本、右下：下・右 の 2 本、右上：右・上 の 2 本
	RD_CHECK_EQ(mesh.GetOutlineSegmentCount(), size_t(7));
}

// 描くと、三角形 1 枚につき塗りの 3 頂点ができる
RD_TEST(NavMeshGizmo, DrawFillsEveryTriangle)
{
	NavMesh mesh;
	mesh.Build({ { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } }, { 0, 1, 2, 0, 2, 3 }, { 0, 2 });
	DebugDraw draw;
	mesh.DrawGizmos(draw);
	RD_CHECK_EQ(draw.GetDepthTestedTriangles().size(), size_t(6));
	// 区画が違えば色も違う
	RD_CHECK(draw.GetDepthTestedTriangles()[0].color.b != draw.GetDepthTestedTriangles()[3].color.b);
}
