#include "RdPch.h"
#include "Physics/Collision/ClosestPoint.h"
#include "Physics/Collision/CollisionDetail.h"

namespace rd::collision
{
	namespace
	{
		// 分離軸判定で、今までで一番浅い重なりとその軸
		struct SatResult
		{
			float overlap = math::kInfinity;
			Vector3 axis;
		};

		bool InsideBox(const Vector3& p, const WorldShape& b, float slop)
		{
			Vector3 d = p - b.center;
			for (int i = 0; i < 3; ++i)
			{
				if (std::fabs(Dot(d, b.axes[i])) > b.halfExtents[i] + slop) { return false; }
			}
			return true;
		}

		// 箱と箱をこの軸に投影して重なりを調べる。離れていれば false（平行な辺から作った長さ 0 の軸は無視）
		//  辺同士の軸は bias でわずかに不利にして、面での接触を優先する
		bool TestBoxBoxAxis(const WorldShape& a, const WorldShape& b, const Vector3& t, Vector3 axis, float bias, SatResult& best)
		{
			float len2 = LengthSq(axis);
			if (len2 < 1.0e-8f) { return true; }
			axis /= std::sqrt(len2);
			float overlap = ProjectBox(a, axis) + ProjectBox(b, axis) - std::fabs(Dot(t, axis));
			if (overlap < 0.0f) { return false; }
			if (overlap * bias < best.overlap)
			{
				best.overlap = overlap * bias;
				best.axis = Dot(t, axis) < 0.0f ? -axis : axis;
			}
			return true;
		}

		// 箱と三角形をこの軸に投影する。三角形が +axis 側にあるときの重なりが o1、-axis 側が o2
		bool TestBoxTriangleAxis(const WorldShape& box, const Vector3& a, const Vector3& b, const Vector3& c, Vector3 axis, SatResult& best)
		{
			float len2 = LengthSq(axis);
			if (len2 < 1.0e-8f) { return true; }
			axis /= std::sqrt(len2);
			float bc = Dot(box.center, axis);
			float r = ProjectBox(box, axis);
			float ta = Dot(a, axis), tb = Dot(b, axis), tc = Dot(c, axis);
			float tmin = std::min(ta, std::min(tb, tc)), tmax = std::max(ta, std::max(tb, tc));
			float o1 = (bc + r) - tmin;
			float o2 = tmax - (bc - r);
			if (o1 < 0.0f || o2 < 0.0f) { return false; }
			float o = std::min(o1, o2) * 1.05f;
			if (o < best.overlap)
			{
				best.overlap = o;
				best.axis = o1 < o2 ? axis : -axis;
			}
			return true;
		}
	}

	bool SphereSphere(const Vector3& a, float ra, const Vector3& b, float rb, std::vector<ContactPoint>& out)
	{
		Vector3 d = b - a;
		float dist2 = LengthSq(d);
		float r = ra + rb;
		if (dist2 >= r * r) { return false; }
		float dist = std::sqrt(dist2);
		Vector3 n = dist > 1.0e-6f ? d / dist : Vector3::kUnitY;
		out.push_back({ a + n * (ra - (r - dist) * 0.5f), n, r - dist });
		return true;
	}

	// 中心が箱の中にあるときは、一番浅い面から押し出す
	bool SphereBox(const Vector3& c, float r, const WorldShape& box, std::vector<ContactPoint>& out)
	{
		Vector3 q = ClosestPointOnBox(c, box);
		Vector3 d = q - c;
		float dist2 = LengthSq(d);
		if (dist2 > r * r) { return false; }
		if (dist2 > 1.0e-10f)
		{
			float dist = std::sqrt(dist2);
			out.push_back({ q, d / dist, r - dist });
			return true;
		}
		Vector3 local = c - box.center;
		int axis = 0;
		float minPen = math::kInfinity;
		float sign = 1.0f;
		for (int i = 0; i < 3; ++i)
		{
			float p = Dot(local, box.axes[i]);
			float pen = box.halfExtents[i] - std::fabs(p);
			if (pen < minPen) { minPen = pen; axis = i; sign = p >= 0.0f ? 1.0f : -1.0f; }
		}
		out.push_back({ c, -box.axes[axis] * sign, minPen + r });
		return true;
	}

	void BoxVertices(const WorldShape& b, Vector3 out[8])
	{
		for (int i = 0; i < 8; ++i)
		{
			out[i] = b.center
				+ b.axes[0] * ((i & 1) ? b.halfExtents.x : -b.halfExtents.x)
				+ b.axes[1] * ((i & 2) ? b.halfExtents.y : -b.halfExtents.y)
				+ b.axes[2] * ((i & 4) ? b.halfExtents.z : -b.halfExtents.z);
		}
	}

	float ProjectBox(const WorldShape& b, const Vector3& axis)
	{
		return b.halfExtents.x * std::fabs(Dot(b.axes[0], axis)) +
			b.halfExtents.y * std::fabs(Dot(b.axes[1], axis)) +
			b.halfExtents.z * std::fabs(Dot(b.axes[2], axis));
	}

	// 分離軸判定（両方の面の向き 6 本と、辺同士の外積 9 本）。接触点は相手の中にめり込んだ頂点の平均
	bool BoxBox(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
	{
		Vector3 t = b.center - a.center;
		SatResult best;
		for (int i = 0; i < 3; ++i)
		{
			if (!TestBoxBoxAxis(a, b, t, a.axes[i], 1.0f, best) || !TestBoxBoxAxis(a, b, t, b.axes[i], 1.0f, best)) { return false; }
		}
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				if (!TestBoxBoxAxis(a, b, t, Cross(a.axes[i], b.axes[j]), 1.05f, best)) { return false; }
			}
		}
		float depth = ProjectBox(a, best.axis) + ProjectBox(b, best.axis) - std::fabs(Dot(t, best.axis));

		Vector3 va[8], vb[8];
		BoxVertices(a, va);
		BoxVertices(b, vb);
		Vector3 sum;
		int count = 0;
		for (int i = 0; i < 8; ++i)
		{
			if (InsideBox(va[i], b, 0.01f)) { sum += va[i]; ++count; }
			if (InsideBox(vb[i], a, 0.01f)) { sum += vb[i]; ++count; }
		}
		Vector3 point = count > 0 ? sum / float(count) : (a.center + b.center) * 0.5f;
		out.push_back({ point, best.axis, depth });
		return true;
	}

	// 線分と箱の最近点は、互いの最近点を何回か行き来して近づける
	void ClosestSegmentBox(const Vector3& p0, const Vector3& p1, const WorldShape& box, Vector3& onSeg, Vector3& onBox)
	{
		onSeg = ClosestPointOnSegment(box.center, p0, p1);
		for (int i = 0; i < 4; ++i)
		{
			onBox = ClosestPointOnBox(onSeg, box);
			onSeg = ClosestPointOnSegment(onBox, p0, p1);
		}
		onBox = ClosestPointOnBox(onSeg, box);
	}

	// 線分が三角形を貫いていればその点、そうでなければ互いの最近点を何回か行き来して近づける
	void ClosestSegmentTriangle(const Vector3& p0, const Vector3& p1, const Vector3& a, const Vector3& b, const Vector3& c,
		Vector3& onSeg, Vector3& onTri)
	{
		Vector3 n = Cross(b - a, c - a);
		float d0 = Dot(p0 - a, n), d1 = Dot(p1 - a, n);
		if (d0 * d1 < 0.0f)
		{
			Vector3 hit = p0 + (p1 - p0) * (d0 / (d0 - d1));
			if (LengthSq(ClosestPointOnTriangle(hit, a, b, c) - hit) < 1.0e-8f)
			{
				onSeg = onTri = hit;
				return;
			}
		}
		onSeg = ClosestPointOnSegment((a + b + c) / 3.0f, p0, p1);
		for (int i = 0; i < 4; ++i)
		{
			onTri = ClosestPointOnTriangle(onSeg, a, b, c);
			onSeg = ClosestPointOnSegment(onTri, p0, p1);
		}
		onTri = ClosestPointOnTriangle(onSeg, a, b, c);
	}

	// 分離軸判定（三角形の法線、箱の 3 軸、箱の軸と辺の外積 9 本）
	//  三角形の法線の向きは表側へ押し出す。接触点は押し出す向きの反対側（三角形側）にある箱の頂点の平均
	bool BoxTriangle(const WorldShape& box, const Vector3& a, const Vector3& b, const Vector3& c, std::vector<ContactPoint>& out)
	{
		Vector3 n = Cross(b - a, c - a);
		float nlen = Length(n);
		if (nlen < 1.0e-8f) { return false; }
		n /= nlen;
		float rBox = ProjectBox(box, n);
		float s = Dot(box.center - a, n);
		if (std::fabs(s) > rBox) { return false; }
		SatResult best;
		best.overlap = rBox - s;
		best.axis = -n;

		Vector3 edges[3] = { b - a, c - b, a - c };
		for (int i = 0; i < 3; ++i) { if (!TestBoxTriangleAxis(box, a, b, c, box.axes[i], best)) { return false; } }
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j) { if (!TestBoxTriangleAxis(box, a, b, c, Cross(box.axes[i], edges[j]), best)) { return false; } }
		}

		Vector3 v[8];
		BoxVertices(box, v);
		float maxD = -math::kInfinity;
		for (auto& p : v) { maxD = std::max(maxD, Dot(p, best.axis)); }
		Vector3 sum;
		int count = 0;
		for (auto& p : v)
		{
			if (Dot(p, best.axis) > maxD - 0.02f) { sum += p; ++count; }
		}
		out.push_back({ sum / float(std::max(count, 1)), best.axis, best.overlap });
		return true;
	}

	void LimitContacts(std::vector<ContactPoint>& out, size_t before, size_t max)
	{
		if (out.size() - before > max)
		{
			std::sort(out.begin() + ptrdiff_t(before), out.end(), [](auto& x, auto& y) { return x.depth > y.depth; });
			out.resize(before + max);
		}
	}
}
