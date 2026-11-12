#include "RdPch.h"
#include "Render/DebugDraw.h"

namespace rd
{
	namespace
	{
		// 法線に垂直な 2 軸
		void Basis(const Vector3& n, Vector3& u, Vector3& v)
		{
			u = Normalize(Cross(std::fabs(n.y) < 0.99f ? Vector3::kUnitY : Vector3::kUnitX, n));
			v = Cross(n, u);
		}
	}

	void DebugDraw::Line(const Vector3& a, const Vector3& b, const Color& color, bool depthTest)
	{
		auto& t = Target(depthTest);
		t.push_back({ a, color });
		t.push_back({ b, color });
	}

	void DebugDraw::Arrow(const Vector3& from, const Vector3& to, const Color& color, bool depthTest)
	{
		Line(from, to, color, depthTest);
		Vector3 dir = to - from;
		float len = Length(dir);
		if (len < 1.0e-5f) { return; }
		dir /= len;
		Vector3 u, v;
		Basis(dir, u, v);
		float head = len * 0.15f;
		Vector3 base = to - dir * head;
		Line(to, base + u * head * 0.4f, color, depthTest);
		Line(to, base - u * head * 0.4f, color, depthTest);
		Line(to, base + v * head * 0.4f, color, depthTest);
		Line(to, base - v * head * 0.4f, color, depthTest);
	}

	void DebugDraw::Box(const AABB& box, const Color& color, bool depthTest)
	{
		if (!box.IsValid()) { return; }
		Vector3 size = box.max - box.min;
		Box(Matrix4::CreateScale(size) * Matrix4::CreateTranslate(box.GetCenter()), color, depthTest);
	}

	void DebugDraw::Box(const Matrix4& m, const Color& color, bool depthTest)
	{
		Vector3 p[8];
		for (int i = 0; i < 8; ++i)
		{
			Vector3 c((i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f);
			p[i] = TransformPoint(c, m);
		}
		const int edges[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
		for (auto& e : edges) { Line(p[e[0]], p[e[1]], color, depthTest); }
	}

	void DebugDraw::Circle(const Vector3& center, const Vector3& normal, float radius, const Color& color, bool depthTest, uint32_t segments)
	{
		Vector3 u, v;
		Basis(Normalize(normal), u, v);
		Vector3 prev = center + u * radius;
		for (uint32_t i = 1; i <= segments; ++i)
		{
			float a = math::k2Pi * float(i) / float(segments);
			Vector3 p = center + (u * std::cos(a) + v * std::sin(a)) * radius;
			Line(prev, p, color, depthTest);
			prev = p;
		}
	}

	void DebugDraw::Sphere(const Vector3& center, float radius, const Color& color, bool depthTest, uint32_t segments)
	{
		Circle(center, Vector3::kUnitX, radius, color, depthTest, segments);
		Circle(center, Vector3::kUnitY, radius, color, depthTest, segments);
		Circle(center, Vector3::kUnitZ, radius, color, depthTest, segments);
	}

	void DebugDraw::Capsule(const Vector3& a, const Vector3& b, float radius, const Color& color, bool depthTest)
	{
		Vector3 axis = b - a;
		float len = Length(axis);
		Vector3 n = len > 1.0e-5f ? axis / len : Vector3::kUnitY;
		Vector3 u, v;
		Basis(n, u, v);
		Circle(a, n, radius, color, depthTest);
		Circle(b, n, radius, color, depthTest);
		for (Vector3 d : { u, -u, v, -v }) { Line(a + d * radius, b + d * radius, color, depthTest); }
		// 半球の輪郭
		const uint32_t seg = 12;
		for (Vector3 d : { u, v })
		{
			Vector3 prevA = a + d * radius, prevB = b + d * radius;
			for (uint32_t i = 1; i <= seg; ++i)
			{
				float t = math::kPi * float(i) / float(seg);
				Vector3 offA = (d * std::cos(t) - n * std::sin(t)) * radius;
				Vector3 offB = (d * std::cos(t) + n * std::sin(t)) * radius;
				Line(prevA, a + offA, color, depthTest);
				Line(prevB, b + offB, color, depthTest);
				prevA = a + offA;
				prevB = b + offB;
			}
		}
	}

	void DebugDraw::Cone(const Vector3& apex, const Vector3& dir, float length, float angle, const Color& color, bool depthTest)
	{
		Vector3 n = Normalize(dir);
		Vector3 u, v;
		Basis(n, u, v);
		float r = std::tan(angle) * length;
		Vector3 base = apex + n * length;
		Circle(base, n, r, color, depthTest, 24);
		for (Vector3 d : { u, -u, v, -v }) { Line(apex, base + d * r, color, depthTest); }
	}

	void DebugDraw::Frustum(const Matrix4& invViewProj, const Color& color, bool depthTest)
	{
		Vector3 p[8];
		for (int i = 0; i < 8; ++i)
		{
			Vector3 ndc((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : 0.0f);
			p[i] = TransformCoord(ndc, invViewProj);
		}
		const int edges[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
		for (auto& e : edges) { Line(p[e[0]], p[e[1]], color, depthTest); }
	}

	void DebugDraw::Grid(float size, float step, const Color& color)
	{
		int n = int(size / step);
		for (int i = -n; i <= n; ++i)
		{
			float p = float(i) * step;
			Color c = color;
			if (i == 0) { c = Color(0.9f, 0.3f, 0.3f, 0.8f); }
			Line({ -size, 0.0f, p }, { size, 0.0f, p }, c, true);
			if (i == 0) { c = Color(0.3f, 0.5f, 0.9f, 0.8f); }
			Line({ p, 0.0f, -size }, { p, 0.0f, size }, c, true);
		}
	}

	void DebugDraw::Axes(const Matrix4& m, float length, bool depthTest)
	{
		Vector3 o = m.GetTranslation();
		Line(o, o + Normalize(m.GetAxisX()) * length, Color::kRed, depthTest);
		Line(o, o + Normalize(m.GetAxisY()) * length, Color::kGreen, depthTest);
		Line(o, o + Normalize(m.GetAxisZ()) * length, Color::kBlue, depthTest);
	}
}
