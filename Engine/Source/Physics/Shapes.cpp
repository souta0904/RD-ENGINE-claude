#include "RdPch.h"
#include "Physics/Shapes.h"

namespace rd
{
	// ==================================================
	// 幾何ユーティリティ
	// ==================================================
	Vector3 ClosestPointOnSegment(const Vector3& p, const Vector3& a, const Vector3& b, float* outT)
	{
		Vector3 ab = b - a;
		float len2 = LengthSq(ab);
		float t = len2 > 1.0e-12f ? math::Saturate(Dot(p - a, ab) / len2) : 0.0f;
		if (outT) { *outT = t; }
		return a + ab * t;
	}

	void ClosestPointsSegmentSegment(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2, Vector3& c1, Vector3& c2)
	{
		// Real-Time Collision Detection 5.1.9
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

	Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c)
	{
		// Real-Time Collision Detection 5.1.5
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

	void WorldShape::ComputeBounds()
	{
		switch (type)
		{
		case ShapeType::Sphere:
			bounds = AABB::FromCenterExtents(center, Vector3(radius, radius, radius));
			break;
		case ShapeType::Box:
		{
			Vector3 e;
			for (int i = 0; i < 3; ++i) { e += Abs(axes[i]) * halfExtents[i]; }
			bounds = AABB::FromCenterExtents(center, e);
			break;
		}
		case ShapeType::Capsule:
			bounds = AABB(Min(p0, p1) - Vector3(radius, radius, radius), Max(p0, p1) + Vector3(radius, radius, radius));
			break;
		case ShapeType::Mesh:
			bounds = mesh ? mesh->GetBounds() : AABB();
			break;
		}
	}

	// ==================================================
	// MeshCollisionData（BVH）
	// ==================================================
	void MeshCollisionData::Build(std::vector<Vector3> positions, std::vector<uint32_t> indices)
	{
		mPositions = std::move(positions);
		mIndices = std::move(indices);
		uint32_t triCount = uint32_t(mIndices.size() / 3);
		mTriangles.resize(triCount);
		mTriBounds.resize(triCount);
		mTriCenters.resize(triCount);
		for (uint32_t i = 0; i < triCount; ++i)
		{
			Vector3 a, b, c;
			GetTriangle(i, a, b, c);
			mTriangles[i] = i;
			mTriBounds[i] = AABB(Min(a, Min(b, c)), Max(a, Max(b, c)));
			mTriCenters[i] = (a + b + c) / 3.0f;
		}
		mNodes.clear();
		mNodes.reserve(size_t(triCount) / 2 + 1);
		if (triCount > 0) { BuildNode(0, triCount, 0); }
	}

	void MeshCollisionData::GetTriangle(uint32_t index, Vector3& a, Vector3& b, Vector3& c) const
	{
		a = mPositions[mIndices[size_t(index) * 3 + 0]];
		b = mPositions[mIndices[size_t(index) * 3 + 1]];
		c = mPositions[mIndices[size_t(index) * 3 + 2]];
	}

	uint32_t MeshCollisionData::BuildNode(uint32_t start, uint32_t count, int depth)
	{
		uint32_t index = uint32_t(mNodes.size());
		mNodes.emplace_back();
		AABB bounds, centers;
		for (uint32_t i = start; i < start + count; ++i)
		{
			bounds.Expand(mTriBounds[mTriangles[i]]);
			centers.Expand(mTriCenters[mTriangles[i]]);
		}
		mNodes[index].bounds = bounds;

		if (count <= 4 || depth > 32)
		{
			mNodes[index].start = start;
			mNodes[index].count = count;
			return index;
		}
		// 中心の広がりが最大の軸で中央値分割
		Vector3 extent = centers.max - centers.min;
		int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);
		uint32_t mid = start + count / 2;
		std::nth_element(mTriangles.begin() + start, mTriangles.begin() + mid, mTriangles.begin() + start + count,
			[&](uint32_t a, uint32_t b) { return mTriCenters[a][axis] < mTriCenters[b][axis]; });

		uint32_t left = BuildNode(start, mid - start, depth + 1);
		uint32_t right = BuildNode(mid, start + count - mid, depth + 1);
		mNodes[index].left = left;
		mNodes[index].right = right;
		return index;
	}

	void MeshCollisionData::Query(const AABB& box, const std::function<void(uint32_t)>& callback) const
	{
		if (mNodes.empty()) { return; }
		uint32_t stack[96];
		int sp = 0;
		stack[sp++] = 0;
		while (sp > 0)
		{
			const Node& n = mNodes[stack[--sp]];
			if (!n.bounds.Overlaps(box)) { continue; }
			if (n.count > 0)
			{
				for (uint32_t i = n.start; i < n.start + n.count; ++i)
				{
					if (mTriBounds[mTriangles[i]].Overlaps(box)) { callback(mTriangles[i]); }
				}
			}
			else if (sp + 2 <= 96)
			{
				stack[sp++] = n.left;
				stack[sp++] = n.right;
			}
		}
	}

	bool MeshCollisionData::Raycast(const Ray& ray, float maxDist, float& outT, Vector3& outNormal) const
	{
		if (mNodes.empty()) { return false; }
		bool hit = false;
		float best = maxDist;
		uint32_t stack[96];
		int sp = 0;
		stack[sp++] = 0;
		while (sp > 0)
		{
			const Node& n = mNodes[stack[--sp]];
			float tBox;
			if (!RayIntersectsAABB(ray, n.bounds, tBox) || tBox > best) { continue; }
			if (n.count > 0)
			{
				for (uint32_t i = n.start; i < n.start + n.count; ++i)
				{
					Vector3 a, b, c;
					GetTriangle(mTriangles[i], a, b, c);
					float t;
					if (RayIntersectsTriangle(ray, a, b, c, t) && t < best)
					{
						best = t;
						Vector3 nrm = Normalize(Cross(b - a, c - a));
						outNormal = Dot(nrm, ray.direction) > 0.0f ? -nrm : nrm;
						hit = true;
					}
				}
			}
			else if (sp + 2 <= 96)
			{
				stack[sp++] = n.left;
				stack[sp++] = n.right;
			}
		}
		if (hit) { outT = best; }
		return hit;
	}

	// ==================================================
	// 狭域判定
	// ==================================================
	namespace
	{
		// 2 つの球（a, b は中心）。法線は a -> b
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

		// 球と箱。法線は球 -> 箱
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
			// 中心が箱の中: 最も浅い面から押し出す
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

		// 箱の 8 頂点
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

		bool InsideBox(const Vector3& p, const WorldShape& b, float slop)
		{
			Vector3 d = p - b.center;
			for (int i = 0; i < 3; ++i)
			{
				if (std::fabs(Dot(d, b.axes[i])) > b.halfExtents[i] + slop) { return false; }
			}
			return true;
		}

		// 箱と箱（分離軸判定）。法線は a -> b
		bool BoxBox(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
		{
			Vector3 t = b.center - a.center;
			float minOverlap = math::kInfinity;
			Vector3 bestAxis;
			auto test = [&](Vector3 axis, float bias) -> bool
				{
					float len2 = LengthSq(axis);
					if (len2 < 1.0e-8f) { return true; }// 平行な辺同士は無視
					axis /= std::sqrt(len2);
					float overlap = ProjectBox(a, axis) + ProjectBox(b, axis) - std::fabs(Dot(t, axis));
					if (overlap < 0.0f) { return false; }
					// 辺同士の軸はわずかに不利にして面の接触を優先する
					if (overlap * bias < minOverlap)
					{
						minOverlap = overlap * bias;
						bestAxis = Dot(t, axis) < 0.0f ? -axis : axis;
					}
					return true;
				};
			for (int i = 0; i < 3; ++i)
			{
				if (!test(a.axes[i], 1.0f) || !test(b.axes[i], 1.0f)) { return false; }
			}
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					if (!test(Cross(a.axes[i], b.axes[j]), 1.05f)) { return false; }
				}
			}
			float depth = ProjectBox(a, bestAxis) + ProjectBox(b, bestAxis) - std::fabs(Dot(t, bestAxis));

			// 相手の中にめり込んだ頂点の平均を接触点にする
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
			out.push_back({ point, bestAxis, depth });
			return true;
		}

		// 線分（カプセル軸）と箱の最近点を反復で求める
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

		// 線分と三角形の最近点
		void ClosestSegmentTriangle(const Vector3& p0, const Vector3& p1, const Vector3& a, const Vector3& b, const Vector3& c,
			Vector3& onSeg, Vector3& onTri)
		{
			// 線分が三角形を貫通している場合
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

		// 箱と三角形（分離軸判定、三角形は表側からのみ押し返す）。法線は箱 -> 三角形
		bool BoxTriangle(const WorldShape& box, const Vector3& a, const Vector3& b, const Vector3& c, std::vector<ContactPoint>& out)
		{
			Vector3 n = Cross(b - a, c - a);
			float nlen = Length(n);
			if (nlen < 1.0e-8f) { return false; }
			n /= nlen;
			float rBox = ProjectBox(box, n);
			float s = Dot(box.center - a, n);
			if (std::fabs(s) > rBox) { return false; }
			float minOverlap = rBox - s;// 表側へ押し出す
			Vector3 bestAxis = -n;

			Vector3 edges[3] = { b - a, c - b, a - c };
			auto test = [&](Vector3 axis) -> bool
				{
					float len2 = LengthSq(axis);
					if (len2 < 1.0e-8f) { return true; }
					axis /= std::sqrt(len2);
					float bc = Dot(box.center, axis);
					float r = ProjectBox(box, axis);
					float ta = Dot(a, axis), tb = Dot(b, axis), tc = Dot(c, axis);
					float tmin = std::min(ta, std::min(tb, tc)), tmax = std::max(ta, std::max(tb, tc));
					float o1 = (bc + r) - tmin;// 三角形が +axis 側
					float o2 = tmax - (bc - r);// 三角形が -axis 側
					if (o1 < 0.0f || o2 < 0.0f) { return false; }
					float o = std::min(o1, o2) * 1.05f;
					if (o < minOverlap)
					{
						minOverlap = o;
						bestAxis = o1 < o2 ? axis : -axis;
					}
					return true;
				};
			for (int i = 0; i < 3; ++i) { if (!test(box.axes[i])) { return false; } }
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j) { if (!test(Cross(box.axes[i], edges[j]))) { return false; } }
			}

			// 押し出し方向の反対側（三角形側）にある箱の頂点の平均
			Vector3 v[8];
			BoxVertices(box, v);
			float maxD = -math::kInfinity;
			for (auto& p : v) { maxD = std::max(maxD, Dot(p, bestAxis)); }
			Vector3 sum;
			int count = 0;
			for (auto& p : v)
			{
				if (Dot(p, bestAxis) > maxD - 0.02f) { sum += p; ++count; }
			}
			out.push_back({ sum / float(std::max(count, 1)), bestAxis, minOverlap });
			return true;
		}

		bool CollideWithMesh(const WorldShape& s, const MeshCollisionData& mesh, std::vector<ContactPoint>& out)
		{
			size_t before = out.size();
			mesh.Query(s.bounds, [&](uint32_t tri)
				{
					Vector3 a, b, c;
					mesh.GetTriangle(tri, a, b, c);
					switch (s.type)
					{
					case ShapeType::Sphere:
					{
						Vector3 q = ClosestPointOnTriangle(s.center, a, b, c);
						Vector3 d = q - s.center;
						float dist2 = LengthSq(d);
						if (dist2 < s.radius * s.radius && dist2 > 1.0e-12f)
						{
							float dist = std::sqrt(dist2);
							out.push_back({ q, d / dist, s.radius - dist });
						}
						break;
					}
					case ShapeType::Capsule:
					{
						Vector3 onSeg, onTri;
						ClosestSegmentTriangle(s.p0, s.p1, a, b, c, onSeg, onTri);
						Vector3 d = onTri - onSeg;
						float dist2 = LengthSq(d);
						if (dist2 < s.radius * s.radius)
						{
							float dist = std::sqrt(dist2);
							Vector3 n = dist > 1.0e-6f ? d / dist : -Normalize(Cross(b - a, c - a));
							out.push_back({ onTri, n, s.radius - dist });
						}
						break;
					}
					case ShapeType::Box:
						BoxTriangle(s, a, b, c, out);
						break;
					default:
						break;
					}
				});
			// 接触点が多すぎると不安定になるので深いものから 4 つに絞る
			if (out.size() - before > 4)
			{
				std::sort(out.begin() + ptrdiff_t(before), out.end(), [](auto& x, auto& y) { return x.depth > y.depth; });
				out.resize(before + 4);
			}
			return out.size() > before;
		}

		bool CollideOrdered(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
		{
			// a.type <= b.type が保証されている
			switch (a.type)
			{
			case ShapeType::Sphere:
				switch (b.type)
				{
				case ShapeType::Sphere: return SphereSphere(a.center, a.radius, b.center, b.radius, out);
				case ShapeType::Box: return SphereBox(a.center, a.radius, b, out);
				case ShapeType::Capsule:
				{
					Vector3 q = ClosestPointOnSegment(a.center, b.p0, b.p1);
					return SphereSphere(a.center, a.radius, q, b.radius, out);
				}
				case ShapeType::Mesh: return b.mesh && CollideWithMesh(a, *b.mesh, out);
				}
				break;
			case ShapeType::Box:
				switch (b.type)
				{
				case ShapeType::Box: return BoxBox(a, b, out);
				case ShapeType::Capsule:
				{
					// 箱 -> カプセル: カプセル側の最近点を球として扱う
					Vector3 onSeg, onBox;
					ClosestSegmentBox(b.p0, b.p1, a, onSeg, onBox);
					size_t before = out.size();
					if (!SphereBox(onSeg, b.radius, a, out)) { return false; }
					// SphereBox の法線は球 -> 箱なので反転
					for (size_t i = before; i < out.size(); ++i) { out[i].normal = -out[i].normal; }
					return true;
				}
				case ShapeType::Mesh: return b.mesh && CollideWithMesh(a, *b.mesh, out);
				default: break;
				}
				break;
			case ShapeType::Capsule:
				switch (b.type)
				{
				case ShapeType::Capsule:
				{
					Vector3 c1, c2;
					ClosestPointsSegmentSegment(a.p0, a.p1, b.p0, b.p1, c1, c2);
					return SphereSphere(c1, a.radius, c2, b.radius, out);
				}
				case ShapeType::Mesh: return b.mesh && CollideWithMesh(a, *b.mesh, out);
				default: break;
				}
				break;
			default:
				break;
			}
			return false;
		}
	}

	bool CollideShapes(const WorldShape& a, const WorldShape& b, std::vector<ContactPoint>& out)
	{
		if (!a.bounds.Overlaps(b.bounds)) { return false; }
		if (a.type <= b.type) { return CollideOrdered(a, b, out); }
		size_t before = out.size();
		bool hit = CollideOrdered(b, a, out);
		for (size_t i = before; i < out.size(); ++i) { out[i].normal = -out[i].normal; }
		return hit;
	}

	// ==================================================
	// レイキャスト
	// ==================================================
	namespace
	{
		bool RaySphere(const Ray& ray, const Vector3& c, float r, float& t)
		{
			Vector3 m = ray.origin - c;
			float b = Dot(m, ray.direction);
			float cc = Dot(m, m) - r * r;
			if (cc > 0.0f && b > 0.0f) { return false; }
			float disc = b * b - cc;
			if (disc < 0.0f) { return false; }
			t = std::max(0.0f, -b - std::sqrt(disc));
			return true;
		}
	}

	bool RaycastShape(const WorldShape& s, const Ray& ray, float maxDist, float& outT, Vector3& outNormal)
	{
		float t = 0.0f;
		switch (s.type)
		{
		case ShapeType::Sphere:
			if (!RaySphere(ray, s.center, s.radius, t) || t > maxDist) { return false; }
			outT = t;
			outNormal = Normalize(ray.GetPoint(t) - s.center);
			return true;

		case ShapeType::Box:
		{
			// 箱のローカル空間でスラブ判定
			Vector3 o = ray.origin - s.center;
			Ray local{ { Dot(o, s.axes[0]), Dot(o, s.axes[1]), Dot(o, s.axes[2]) },
				{ Dot(ray.direction, s.axes[0]), Dot(ray.direction, s.axes[1]), Dot(ray.direction, s.axes[2]) } };
			AABB box(-s.halfExtents, s.halfExtents);
			if (!RayIntersectsAABB(local, box, t) || t > maxDist) { return false; }
			Vector3 p = local.GetPoint(t);
			int axis = 0;
			float best = 0.0f;
			for (int i = 0; i < 3; ++i)
			{
				float d = std::fabs(p[i]) / std::max(s.halfExtents[i], 1.0e-6f);
				if (d > best) { best = d; axis = i; }
			}
			outT = t;
			outNormal = s.axes[axis] * (p[axis] >= 0.0f ? 1.0f : -1.0f);
			return true;
		}

		case ShapeType::Capsule:
		{
			// 両端の球と円柱部分の最小
			bool hit = false;
			float best = maxDist;
			Vector3 normal;
			for (const Vector3& c : { s.p0, s.p1 })
			{
				if (RaySphere(ray, c, s.radius, t) && t < best) { best = t; normal = Normalize(ray.GetPoint(t) - c); hit = true; }
			}
			Vector3 axis = s.p1 - s.p0;
			float len = Length(axis);
			if (len > 1.0e-6f)
			{
				Vector3 n = axis / len;
				Vector3 m = ray.origin - s.p0;
				Vector3 dp = ray.direction - n * Dot(ray.direction, n);
				Vector3 mp = m - n * Dot(m, n);
				float a = Dot(dp, dp), b = 2.0f * Dot(dp, mp), c = Dot(mp, mp) - s.radius * s.radius;
				float disc = b * b - 4.0f * a * c;
				if (a > 1.0e-8f && disc >= 0.0f)
				{
					float tc = (-b - std::sqrt(disc)) / (2.0f * a);
					if (tc >= 0.0f && tc < best)
					{
						float h = Dot(ray.GetPoint(tc) - s.p0, n);
						if (h >= 0.0f && h <= len)
						{
							best = tc;
							Vector3 onAxis = s.p0 + n * h;
							normal = Normalize(ray.GetPoint(tc) - onAxis);
							hit = true;
						}
					}
				}
			}
			if (hit) { outT = best; outNormal = normal; }
			return hit;
		}

		case ShapeType::Mesh:
			return s.mesh && s.mesh->Raycast(ray, maxDist, outT, outNormal);
		}
		return false;
	}
}
