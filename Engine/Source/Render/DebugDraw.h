#pragma once
#include "Math/Color.h"
#include "Math/Geometry.h"
#include <vector>

namespace rd
{
	// デバッグ用の線描画（1 フレームだけ表示）
	//  depthTest=false で手前に重ねて描く
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

		void Clear() { mDepth.clear(); mOverlay.clear(); }
		const std::vector<Vertex>& GetDepthTested() const { return mDepth; }
		const std::vector<Vertex>& GetOverlay() const { return mOverlay; }

	private:
		std::vector<Vertex>& Target(bool depthTest) { return depthTest ? mDepth : mOverlay; }
		std::vector<Vertex> mDepth;
		std::vector<Vertex> mOverlay;
	};
}
