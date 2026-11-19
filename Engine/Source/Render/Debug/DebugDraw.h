#pragma once
#include "Math/Color.h"
#include "Math/Geometry/AABB.h"
#include <vector>

namespace rd
{
	// デバッグ用の線と面（そのフレームだけ表示される。depthTest=false なら手前に重ねて描く）
	//  面は半透明で塗るためのもの。線より先に描くので、同じ場所の線は面の上に見える
	class DebugDraw
	{
	public:
		struct Vertex
		{
			Vector3 position;
			Color color;
		};

		void Line(const Vector3& a, const Vector3& b, const Color& color, bool depthTest = true);
		void Ray(const Vector3& origin, const Vector3& dir, const Color& color, bool depthTest = true) { Line(origin, origin + dir, color, depthTest); }
		void Arrow(const Vector3& from, const Vector3& to, const Color& color, bool depthTest = true);
		void Box(const AABB& box, const Color& color, bool depthTest = true);
		// 単位立方体（-0.5～0.5）を matrix で変換した箱
		void Box(const Matrix4& matrix, const Color& color, bool depthTest = true);
		void Sphere(const Vector3& center, float radius, const Color& color, bool depthTest = true, uint32_t segments = 24);
		void Circle(const Vector3& center, const Vector3& normal, float radius, const Color& color, bool depthTest = true, uint32_t segments = 32);
		void Capsule(const Vector3& a, const Vector3& b, float radius, const Color& color, bool depthTest = true);
		void Cone(const Vector3& apex, const Vector3& dir, float length, float angle, const Color& color, bool depthTest = true);
		void Frustum(const Matrix4& invViewProj, const Color& color, bool depthTest = true);
		void Grid(float size, float step, const Color& color);
		void Axes(const Matrix4& matrix, float length, bool depthTest = false);
		// 塗りつぶした三角形（裏からも見える。半透明にするなら color.a を 1 より小さく）
		void Triangle(const Vector3& a, const Vector3& b, const Vector3& c, const Color& color, bool depthTest = true);
		// 立てた円柱の輪郭（base が底面の中心）
		void Cylinder(const Vector3& base, float radius, float height, const Color& color, bool depthTest = true);

		void Clear() { mDepth.clear(); mOverlay.clear(); mDepthTris.clear(); mOverlayTris.clear(); }
		// 線（2 頂点で 1 本）
		const std::vector<Vertex>& GetDepthTested() const { return mDepth; }
		const std::vector<Vertex>& GetOverlay() const { return mOverlay; }
		// 三角形（3 頂点で 1 枚）
		const std::vector<Vertex>& GetDepthTestedTriangles() const { return mDepthTris; }
		const std::vector<Vertex>& GetOverlayTriangles() const { return mOverlayTris; }

	private:
		std::vector<Vertex>& Target(bool depthTest) { return depthTest ? mDepth : mOverlay; }
		std::vector<Vertex> mDepth;
		std::vector<Vertex> mOverlay;
		std::vector<Vertex> mDepthTris;
		std::vector<Vertex> mOverlayTris;
	};
}
