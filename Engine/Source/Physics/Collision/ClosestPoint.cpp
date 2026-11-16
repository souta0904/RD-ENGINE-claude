#include "RdPch.h"
#include "Physics/Collision/ClosestPoint.h"

namespace rd
{
	Vector3 ClosestPointOnSegment(const Vector3& p, const Vector3& a, const Vector3& b, float* outT)
	{
		Vector3 ab = b - a;
		float len2 = LengthSq(ab);
		float t = len2 > 1.0e-12f ? math::Saturate(Dot(p - a, ab) / len2) : 0.0f;
		if (outT) { *outT = t; }
		return a + ab * t;
	}

	// 2 本の直線の最近点を求め、線分の範囲に収まるよう片方ずつ切り詰める（5.1.9）
	void ClosestPointsSegmentSegment(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2, Vector3& c1, Vector3& c2)
	{
		Vector3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
		float a = Dot(d1, d1), e = Dot(d2, d2), f = Dot(d2, r);
		float s = 0.0f, t = 0.0f;
		if (a <= 1.0e-10f && e <= 1.0e-10f) { c1 = p1; c2 = p2; return; }
		if (a <= 1.0e-10f) { t = math::Saturate(f / e); }
		else
		{
			float c = Dot(d1, r);
			if (e <= 1.0e-10f) { s = math::Saturate(-c / a); }
			else
			{
				float b = Dot(d1, d2);
				float denom = a * e - b * b;
				s = denom != 0.0f ? math::Saturate((b * f - c * e) / denom) : 0.0f;
				t = (b * s + f) / e;
				if (t < 0.0f) { t = 0.0f; s = math::Saturate(-c / a); }
				else if (t > 1.0f) { t = 1.0f; s = math::Saturate((b - c) / a); }
			}
		}
		c1 = p1 + d1 * s;
		c2 = p2 + d2 * t;
	}

	// p が三角形のどの領域（頂点・辺・面）の外側にあるかを順に調べ、重心座標で最近点を出す（5.1.5）
	Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c)
	{
		Vector3 ab = b - a, ac = c - a, ap = p - a;
		float d1 = Dot(ab, ap), d2 = Dot(ac, ap);
		if (d1 <= 0.0f && d2 <= 0.0f) { return a; }
		Vector3 bp = p - b;
		float d3 = Dot(ab, bp), d4 = Dot(ac, bp);
		if (d3 >= 0.0f && d4 <= d3) { return b; }
		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) { return a + ab * (d1 / (d1 - d3)); }
		Vector3 cp = p - c;
		float d5 = Dot(ab, cp), d6 = Dot(ac, cp);
		if (d6 >= 0.0f && d5 <= d6) { return c; }
		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) { return a + ac * (d2 / (d2 - d6)); }
		float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
		}
		float denom = 1.0f / (va + vb + vc);
		return a + ab * (vb * denom) + ac * (vc * denom);
	}

	Vector3 ClosestPointOnBox(const Vector3& p, const WorldShape& box)
	{
		Vector3 d = p - box.center;
		Vector3 q = box.center;
		for (int i = 0; i < 3; ++i)
		{
			float dist = math::Clamp(Dot(d, box.axes[i]), -box.halfExtents[i], box.halfExtents[i]);
			q += box.axes[i] * dist;
		}
		return q;
	}
}
