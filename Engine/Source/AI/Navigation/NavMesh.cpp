#include "RdPch.h"
#include "AI/Navigation/NavMesh.h"
#include "Core/Serialization/Json.h"
#include "Render/Debug/DebugDraw.h"
#include <map>
#include <queue>

namespace rd
{
	namespace
	{
		// 上から見た（XZ の）外積。正なら c は a→b の左側
		float Cross2(const Vector3& a, const Vector3& b, const Vector3& c)
		{
			float ux = b.x - a.x, uz = b.z - a.z;
			float vx = c.x - a.x, vz = c.z - a.z;
			return ux * vz - uz * vx;
		}

		bool NearlyEqual(const Vector3& a, const Vector3& b)
		{
			return LengthSq(a - b) < 1.0e-6f;
		}

		// Unity の既定の区画のコスト（0: Walkable, 1: Not Walkable, 2: Jump）
		float AreaCost(int area)
		{
			return area == 2 ? 2.0f : 1.0f;
		}
	}

	void NavMesh::Clear()
	{
		mVertices.clear();
		mTris.clear();
		mLinks.clear();
		mGrid.clear();
		mOutline.clear();
	}

	bool NavMesh::LoadJson(const std::string& path)
	{
		Json json;
		if (!LoadJsonFile(path, json))
		{
			Log::Error(LogCat::Scene, "NavMesh: failed to load {}", path);
			return false;
		}
		std::vector<Vector3> vertices;
		const Json& v = json.value("Vertices", Json::array());
		for (size_t i = 0; i + 2 < v.size(); i += 3)
		{
			vertices.emplace_back(v[i].get<float>(), v[i + 1].get<float>(), v[i + 2].get<float>());
		}
		std::vector<uint32_t> indices;
		for (const auto& i : json.value("Indices", Json::array())) { indices.push_back(i.get<uint32_t>()); }
		std::vector<int> areas;
		for (const auto& a : json.value("Areas", Json::array())) { areas.push_back(a.get<int>()); }

		Build(vertices, indices, areas);
		Log::Info(LogCat::Scene, "NavMesh: loaded {} ({} vertices, {} triangles)", path, mVertices.size(), mTris.size());
		return !mTris.empty();
	}

	// 位置が同じ頂点（1cm 未満の差）を 1 つにまとめてから三角形を作る
	void NavMesh::Build(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices, const std::vector<int>& areas)
	{
		Clear();
		std::map<std::tuple<int, int, int>, uint32_t> welded;
		std::vector<uint32_t> remap(vertices.size());
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			const Vector3& p = vertices[i];
			auto key = std::make_tuple(int(std::lround(p.x * 100.0f)), int(std::lround(p.y * 100.0f)), int(std::lround(p.z * 100.0f)));
			auto [it, inserted] = welded.emplace(key, uint32_t(mVertices.size()));
			if (inserted) { mVertices.push_back(p); }
			remap[i] = it->second;
		}

		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			Triangle t;
			for (int k = 0; k < 3; ++k) { t.v[k] = remap[indices[i + k]]; }
			if (t.v[0] == t.v[1] || t.v[1] == t.v[2] || t.v[0] == t.v[2]) { continue; }
			t.area = (i / 3) < areas.size() ? areas[i / 3] : 0;
			t.center = (mVertices[t.v[0]] + mVertices[t.v[1]] + mVertices[t.v[2]]) / 3.0f;
			mTris.push_back(std::move(t));
		}

		ConnectSharedEdges();
		BuildGrid();
		BuildOutline();
	}

	// 三角形の各辺について、隣の三角形へつながっている区間（Edge の a～b）を辺の上に並べ、
	// どこにも覆われていない隙間を外周として残す。
	//  ふつうの共有辺は辺全体が覆われるので外周にならない。タイルの境目のように辺の一部だけが
	//  別の三角形とつながっているときは、つながっていない残りの部分だけが外周になる。
	void NavMesh::BuildOutline()
	{
		constexpr float kLineTolerance = 0.06f; // 辺の直線から、これ以上離れた区間は別の辺のもの
		constexpr float kMinGap = 0.01f;        // これより短い隙間は外周として描かない

		mOutline.clear();
		std::vector<std::pair<float, float>> covered;
		for (const Triangle& t : mTris)
		{
			for (int k = 0; k < 3; ++k)
			{
				const Vector3& a = mVertices[t.v[k]];
				const Vector3& b = mVertices[t.v[(k + 1) % 3]];
				// 上から見た（XZ の）向きと長さで測る。つなぐときも同じ測り方をしている
				float dx = b.x - a.x, dz = b.z - a.z;
				float len = std::sqrt(dx * dx + dz * dz);
				if (len < 1.0e-4f) { continue; }
				dx /= len;
				dz /= len;
				auto param = [&](const Vector3& p) { return (p.x - a.x) * dx + (p.z - a.z) * dz; };
				auto offLine = [&](const Vector3& p) { return std::fabs((p.x - a.x) * dz - (p.z - a.z) * dx); };

				covered.clear();
				for (const Edge& e : t.edges)
				{
					if (e.link >= 0 || offLine(e.a) > kLineTolerance || offLine(e.b) > kLineTolerance) { continue; }
					float s0 = std::max(0.0f, std::min(param(e.a), param(e.b)));
					float s1 = std::min(len, std::max(param(e.a), param(e.b)));
					if (s1 > s0) { covered.push_back({ s0, s1 }); }
				}
				std::sort(covered.begin(), covered.end());

				// 0 から順に、覆われていない隙間を拾う
				float cursor = 0.0f;
				auto emit = [&](float s0, float s1)
				{
					if (s1 - s0 < kMinGap) { return; }
					mOutline.push_back({ a + (b - a) * (s0 / len), a + (b - a) * (s1 / len) });
				};
				for (const auto& [s0, s1] : covered)
				{
					if (s0 > cursor) { emit(cursor, s0); }
					cursor = std::max(cursor, s1);
				}
				if (cursor < len) { emit(cursor, len); }
			}
		}
	}

	void NavMesh::ConnectSharedEdges()
	{
		// 辺（小さい頂点番号, 大きい頂点番号）→ その辺を持つ三角形と辺の番号
		std::map<std::pair<uint32_t, uint32_t>, std::vector<std::pair<int, int>>> edges;
		for (int t = 0; t < int(mTris.size()); ++t)
		{
			for (int k = 0; k < 3; ++k)
			{
				uint32_t a = mTris[t].v[k], b = mTris[t].v[(k + 1) % 3];
				edges[{ std::min(a, b), std::max(a, b) }].push_back({ t, k });
			}
		}

		std::vector<std::pair<int, int>> open;
		for (auto& [key, owners] : edges)
		{
			if (owners.size() == 1)
			{
				open.push_back(owners[0]);
				continue;
			}
			for (size_t i = 0; i < owners.size(); ++i)
			{
				for (size_t j = 0; j < owners.size(); ++j)
				{
					if (i == j) { continue; }
					Edge e;
					e.to = owners[j].first;
					e.a = mVertices[key.first];
					e.b = mVertices[key.second];
					mTris[owners[i].first].edges.push_back(e);
				}
			}
		}
		ConnectOverlappingEdges(open);
	}

	// 相手のいない辺どうしで、同じ直線上にあって一部が重なるものをつなぐ（タイルの境目の T 字の頂点）
	void NavMesh::ConnectOverlappingEdges(const std::vector<std::pair<int, int>>& openEdges)
	{
		constexpr float kLineTolerance = 0.05f;
		constexpr float kHeightTolerance = 0.5f;
		constexpr float kMinOverlap = 0.05f;

		struct OpenEdge
		{
			int tri;
			Vector3 a, b;
			float minX, maxX, minZ, maxZ;
		};
		std::vector<OpenEdge> list;
		list.reserve(openEdges.size());
		for (auto [t, k] : openEdges)
		{
			const Triangle& tri = mTris[t];
			Vector3 a = mVertices[tri.v[k]], b = mVertices[tri.v[(k + 1) % 3]];
			list.push_back({ t, a, b, std::min(a.x, b.x), std::max(a.x, b.x), std::min(a.z, b.z), std::max(a.z, b.z) });
		}

		for (size_t i = 0; i < list.size(); ++i)
		{
			const OpenEdge& e1 = list[i];
			Vector3 d = e1.b - e1.a;
			float len = std::sqrt(d.x * d.x + d.z * d.z);
			if (len < 1.0e-4f) { continue; }
			float dx = d.x / len, dz = d.z / len;

			for (size_t j = i + 1; j < list.size(); ++j)
			{
				const OpenEdge& e2 = list[j];
				if (e2.tri == e1.tri) { continue; }
				if (e2.maxX < e1.minX - kLineTolerance || e2.minX > e1.maxX + kLineTolerance ||
					e2.maxZ < e1.minZ - kLineTolerance || e2.minZ > e1.maxZ + kLineTolerance) { continue; }

				// e2 の両端が e1 の直線上にあるか
				auto offLine = [&](const Vector3& p) { return std::fabs((p.x - e1.a.x) * dz - (p.z - e1.a.z) * dx); };
				if (offLine(e2.a) > kLineTolerance || offLine(e2.b) > kLineTolerance) { continue; }

				// e1 の向きで測った区間の重なり
				auto param = [&](const Vector3& p) { return (p.x - e1.a.x) * dx + (p.z - e1.a.z) * dz; };
				float s0 = std::max(0.0f, std::min(param(e2.a), param(e2.b)));
				float s1 = std::min(len, std::max(param(e2.a), param(e2.b)));
				if (s1 - s0 < kMinOverlap) { continue; }

				// 高さがそろっていて、2 つの三角形が辺の反対側にあること
				auto heightOn = [](const Vector3& a, const Vector3& b, float t) { return a.y + (b.y - a.y) * t; };
				auto e2Height = [&](float s)
				{
					float p0 = param(e2.a), p1 = param(e2.b);
					float t = std::fabs(p1 - p0) > 1.0e-5f ? (s - p0) / (p1 - p0) : 0.0f;
					return heightOn(e2.a, e2.b, t);
				};
				if (std::fabs(heightOn(e1.a, e1.b, s0 / len) - e2Height(s0)) > kHeightTolerance ||
					std::fabs(heightOn(e1.a, e1.b, s1 / len) - e2Height(s1)) > kHeightTolerance) { continue; }
				float side1 = Cross2(e1.a, e1.b, mTris[e1.tri].center);
				float side2 = Cross2(e1.a, e1.b, mTris[e2.tri].center);
				if (side1 * side2 >= 0.0f) { continue; }

				Vector3 p0 = e1.a + (e1.b - e1.a) * (s0 / len);
				Vector3 p1 = e1.a + (e1.b - e1.a) * (s1 / len);
				Edge forward;
				forward.to = e2.tri;
				forward.a = p0;
				forward.b = p1;
				mTris[e1.tri].edges.push_back(forward);
				Edge backward = forward;
				backward.to = e1.tri;
				mTris[e2.tri].edges.push_back(backward);
			}
		}
	}

	void NavMesh::BuildGrid()
	{
		mGrid.clear();
		for (int t = 0; t < int(mTris.size()); ++t)
		{
			const Triangle& tri = mTris[t];
			float minX = FLT_MAX, maxX = -FLT_MAX, minZ = FLT_MAX, maxZ = -FLT_MAX;
			for (uint32_t v : tri.v)
			{
				minX = std::min(minX, mVertices[v].x); maxX = std::max(maxX, mVertices[v].x);
				minZ = std::min(minZ, mVertices[v].z); maxZ = std::max(maxZ, mVertices[v].z);
			}
			int x0 = int(std::floor(minX / mCellSize)), x1 = int(std::floor(maxX / mCellSize));
			int z0 = int(std::floor(minZ / mCellSize)), z1 = int(std::floor(maxZ / mCellSize));
			for (int x = x0; x <= x1; ++x)
			{
				for (int z = z0; z <= z1; ++z) { mGrid[CellKey(x, z)].push_back(t); }
			}
		}
	}

	// 三角形の上でいちばん近い点（Ericson の Real-Time Collision Detection 5.1.5）
	Vector3 NavMesh::ClosestPointOnTriangle(int tri, const Vector3& p) const
	{
		const Vector3& a = mVertices[mTris[tri].v[0]];
		const Vector3& b = mVertices[mTris[tri].v[1]];
		const Vector3& c = mVertices[mTris[tri].v[2]];
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
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) { return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6))); }
		float denom = 1.0f / (va + vb + vc);
		return a + ab * (vb * denom) + ac * (vc * denom);
	}

	bool NavMesh::SamplePosition(const Vector3& position, float maxDistance, Vector3& outPoint, int* outTriangle) const
	{
		int x0 = int(std::floor((position.x - maxDistance) / mCellSize)), x1 = int(std::floor((position.x + maxDistance) / mCellSize));
		int z0 = int(std::floor((position.z - maxDistance) / mCellSize)), z1 = int(std::floor((position.z + maxDistance) / mCellSize));
		float best = maxDistance * maxDistance;
		int bestTri = -1;
		for (int x = x0; x <= x1; ++x)
		{
			for (int z = z0; z <= z1; ++z)
			{
				auto it = mGrid.find(CellKey(x, z));
				if (it == mGrid.end()) { continue; }
				for (int t : it->second)
				{
					Vector3 q = ClosestPointOnTriangle(t, position);
					float d = LengthSq(q - position);
					if (d <= best)
					{
						best = d;
						bestTri = t;
						outPoint = q;
					}
				}
			}
		}
		if (outTriangle) { *outTriangle = bestTri; }
		return bestTri >= 0;
	}

	Vector3 NavMesh::MoveAlongSurface(int& ioTriangle, const Vector3& target) const
	{
		if (ioTriangle < 0 || ioTriangle >= int(mTris.size()))
		{
			Vector3 p = target;
			SamplePosition(target, startSearchDistance, p, &ioTriangle);
			return p;
		}

		constexpr size_t kMaxVisit = 48;
		std::vector<int> queue{ ioTriangle };
		std::vector<int> visited{ ioTriangle };
		float bestDist = FLT_MAX;
		int bestTri = ioTriangle;
		Vector3 bestPoint = target;
		for (size_t head = 0; head < queue.size(); ++head)
		{
			int t = queue[head];
			const Vector3& a = mVertices[mTris[t].v[0]];
			const Vector3& b = mVertices[mTris[t].v[1]];
			const Vector3& c = mVertices[mTris[t].v[2]];

			// 上から見て三角形の中なら、面の高さに合わせて終わり
			float area = Cross2(a, b, c);
			if (std::fabs(area) > 1.0e-8f)
			{
				float wa = Cross2(b, c, target) / area;
				float wb = Cross2(c, a, target) / area;
				float wc = 1.0f - wa - wb;
				constexpr float kEps = -1.0e-4f;
				if (wa >= kEps && wb >= kEps && wc >= kEps)
				{
					ioTriangle = t;
					return Vector3(target.x, a.y * wa + b.y * wb + c.y * wc, target.z);
				}
			}

			Vector3 q = ClosestPointOnTriangle(t, target);
			float d = (q.x - target.x) * (q.x - target.x) + (q.z - target.z) * (q.z - target.z);
			if (d < bestDist)
			{
				bestDist = d;
				bestTri = t;
				bestPoint = q;
			}

			if (queue.size() >= kMaxVisit) { continue; }
			for (const Edge& e : mTris[t].edges)
			{
				if (e.link >= 0 || std::find(visited.begin(), visited.end(), e.to) != visited.end()) { continue; }
				visited.push_back(e.to);
				queue.push_back(e.to);
			}
		}
		ioTriangle = bestTri;
		return bestPoint;
	}

	int NavMesh::AddLink(const Link& link)
	{
		ResolvedLink r;
		r.link = link;
		mLinks.push_back(r);
		int index = int(mLinks.size()) - 1;
		ConnectLink(index);
		return index;
	}

	void NavMesh::ConnectLink(int index)
	{
		ResolvedLink& r = mLinks[index];
		if (!SamplePosition(r.link.start, linkSearchDistance, r.startPoint, &r.startTri) ||
			!SamplePosition(r.link.end, linkSearchDistance, r.endPoint, &r.endTri))
		{
			r.startTri = r.endTri = -1;
			return;
		}
		if (r.startTri == r.endTri) { return; }

		Edge forward;
		forward.to = r.endTri;
		forward.a = r.startPoint;
		forward.b = r.endPoint;
		forward.link = index;
		mTris[r.startTri].edges.push_back(forward);
		if (r.link.bidirectional)
		{
			Edge backward;
			backward.to = r.startTri;
			backward.a = r.endPoint;
			backward.b = r.startPoint;
			backward.link = index;
			backward.reverse = true;
			mTris[r.endTri].edges.push_back(backward);
		}
	}

	void NavMesh::ClearLinks()
	{
		for (Triangle& t : mTris)
		{
			std::erase_if(t.edges, [](const Edge& e) { return e.link >= 0; });
		}
		mLinks.clear();
	}

	// 三角形の並びを A* で探す（たどり着けなければ、目的地にいちばん近づける三角形まで）
	bool NavMesh::FindTriangles(const Vector3& start, const Vector3& end, std::vector<Step>& outSteps, bool& outComplete, Vector3& ioEnd, int endTri) const
	{
		int startTri = -1;
		Vector3 startPoint;
		if (!SamplePosition(start, startSearchDistance, startPoint, &startTri)) { return false; }

		struct Node
		{
			float g = FLT_MAX;
			int parent = -1;
			const Edge* via = nullptr;
			Vector3 pos;
			bool closed = false;
		};
		std::unordered_map<int, Node> nodes;
		using Item = std::pair<float, int>;
		std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;

		nodes[startTri] = { 0.0f, -1, nullptr, startPoint, false };
		open.push({ Length(end - startPoint), startTri });
		int best = startTri;
		float bestDist = LengthSq(ClosestPointOnTriangle(startTri, end) - end);
		int goal = -1;

		while (!open.empty())
		{
			int cur = open.top().second;
			open.pop();
			Node& node = nodes[cur];
			if (node.closed) { continue; }
			node.closed = true;
			if (cur == endTri) { goal = cur; break; }

			for (const Edge& e : mTris[cur].edges)
			{
				// 通り道の真ん中（リンクなら跳ぶ先）を通るとして距離を測る
				Vector3 entry = e.link >= 0 ? e.b : (e.a + e.b) * 0.5f;
				float step;
				if (e.link >= 0)
				{
					const Link& l = mLinks[e.link].link;
					step = Length(e.a - node.pos) + Length(e.b - e.a) * l.costModifier * AreaCost(l.area);
				}
				else
				{
					step = Length(entry - node.pos) * AreaCost(mTris[e.to].area);
				}
				float g = node.g + step;
				Node& next = nodes[e.to];
				if (next.closed || g >= next.g) { continue; }
				next.g = g;
				next.parent = cur;
				next.via = &e;
				next.pos = entry;
				open.push({ g + Length(end - entry), e.to });

				float d = LengthSq(ClosestPointOnTriangle(e.to, end) - end);
				if (d < bestDist)
				{
					bestDist = d;
					best = e.to;
				}
			}
		}

		outComplete = goal >= 0;
		int last = outComplete ? goal : best;
		if (!outComplete) { ioEnd = ClosestPointOnTriangle(last, end); }

		outSteps.clear();
		for (int t = last; t >= 0; t = nodes[t].parent)
		{
			outSteps.push_back({ t, nodes[t].via });
		}
		std::reverse(outSteps.begin(), outSteps.end());
		return true;
	}

	// steps[first..last] の三角形を通る、start から end までの最短の折れ線（Simple Stupid Funnel Algorithm）
	void NavMesh::StringPull(const Vector3& start, const Vector3& end, const std::vector<Step>& steps, size_t first, size_t last, std::vector<Vector3>& out) const
	{
		std::vector<std::pair<Vector3, Vector3>> portals; // (左, 右)
		portals.push_back({ start, start });
		for (size_t i = first + 1; i <= last; ++i)
		{
			const Edge* e = steps[i].via;
			const Vector3& from = mTris[steps[i - 1].tri].center;
			const Vector3& to = mTris[steps[i].tri].center;
			Vector3 dir = to - from;
			// 進む向きの左にある方を左の端にする
			float ca = dir.x * (e->a.z - from.z) - dir.z * (e->a.x - from.x);
			float cb = dir.x * (e->b.z - from.z) - dir.z * (e->b.x - from.x);
			if (ca >= cb) { portals.push_back({ e->a, e->b }); }
			else { portals.push_back({ e->b, e->a }); }
		}
		portals.push_back({ end, end });

		Vector3 apex = start, left = start, right = start;
		size_t apexIndex = 0, leftIndex = 0, rightIndex = 0;
		out.push_back(start);
		for (size_t i = 1; i < portals.size(); ++i)
		{
			const Vector3& l = portals[i].first;
			const Vector3& r = portals[i].second;

			// 右の端を内側へ寄せる
			if (Cross2(apex, right, r) >= 0.0f)
			{
				if (NearlyEqual(apex, right) || Cross2(apex, left, r) < 0.0f)
				{
					right = r;
					rightIndex = i;
				}
				else
				{
					// 左の端を越えたので、左の端が曲がり角になる
					out.push_back(left);
					apex = left;
					apexIndex = leftIndex;
					right = left = apex;
					rightIndex = leftIndex = apexIndex;
					i = apexIndex;
					continue;
				}
			}
			// 左の端を内側へ寄せる
			if (Cross2(apex, left, l) <= 0.0f)
			{
				if (NearlyEqual(apex, left) || Cross2(apex, right, l) > 0.0f)
				{
					left = l;
					leftIndex = i;
				}
				else
				{
					out.push_back(right);
					apex = right;
					apexIndex = rightIndex;
					right = left = apex;
					rightIndex = leftIndex = apexIndex;
					i = apexIndex;
					continue;
				}
			}
		}
		if (!NearlyEqual(out.back(), end)) { out.push_back(end); }
	}

	bool NavMesh::CalculatePath(const Vector3& start, const Vector3& end, NavMeshPath& outPath) const
	{
		outPath.Clear();
		if (mTris.empty()) { return false; }

		Vector3 goal;
		int endTri = -1;
		if (!SamplePosition(end, destinationSearchDistance, goal, &endTri)) { return false; }

		std::vector<Step> steps;
		bool complete = false;
		if (!FindTriangles(start, goal, steps, complete, goal, endTri)) { return false; }

		Vector3 from;
		SamplePosition(start, startSearchDistance, from);

		// リンクの前後で区切って、区間ごとに曲がり角を求める
		size_t segmentStart = 0;
		Vector3 segmentFrom = from;
		for (size_t i = 1; i <= steps.size(); ++i)
		{
			bool isLink = i < steps.size() && steps[i].via && steps[i].via->link >= 0;
			if (i < steps.size() && !isLink) { continue; }

			Vector3 segmentTo = isLink ? steps[i].via->a : goal;
			std::vector<Vector3> corners;
			StringPull(segmentFrom, segmentTo, steps, segmentStart, i - 1, corners);
			for (size_t c = 0; c < corners.size(); ++c)
			{
				// 区間のつなぎ目（リンクの着地点）は前の区間の最後と同じなので重ねない
				if (!outPath.corners.empty() && c == 0 && NearlyEqual(outPath.corners.back(), corners[0])) { continue; }
				outPath.corners.push_back(corners[c]);
				outPath.links.push_back(-1);
			}
			if (isLink)
			{
				outPath.links.back() = steps[i].via->link;
				segmentFrom = steps[i].via->b;
				outPath.corners.push_back(segmentFrom);
				outPath.links.push_back(-1);
				segmentStart = i;
			}
		}
		outPath.status = complete ? NavMeshPath::Status::Complete : NavMeshPath::Status::Partial;
		return true;
	}

	Color NavMesh::GetAreaColor(int area)
	{
		switch (area)
		{
		case 0: return Color(0.2f, 0.6f, 1.0f, 1.0f);
		case 1: return Color(0.5f, 0.5f, 0.5f, 1.0f);
		case 2: return Color(1.0f, 0.8f, 0.2f, 1.0f);
		default: break;
		}
		// 番号ごとに色相をずらす（黄金比で回すと、近い番号どうしでも色が離れる）
		float h = std::fmod(float(area) * 0.618034f, 1.0f) * 6.0f;
		float x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
		float r = 0.0f, g = 0.0f, b = 0.0f;
		switch (int(h))
		{
		case 0: r = 1.0f; g = x; break;
		case 1: r = x; g = 1.0f; break;
		case 2: g = 1.0f; b = x; break;
		case 3: g = x; b = 1.0f; break;
		case 4: r = x; b = 1.0f; break;
		default: r = 1.0f; b = x; break;
		}
		return Color(0.25f + r * 0.75f, 0.25f + g * 0.75f, 0.25f + b * 0.75f, 1.0f);
	}

	void NavMesh::DrawGizmos(DebugDraw& draw) const
	{
		// 床と同じ高さに描くと床と奥行きが同じになってちらつくので、少しだけ浮かせる
		const Vector3 fillLift(0.0f, 0.03f, 0.0f);
		const Vector3 lineLift(0.0f, 0.04f, 0.0f);
		const Color outline(0.55f, 0.85f, 1.0f, 0.95f);
		const Color linkColor(1.0f, 0.6f, 0.1f, 1.0f);
		for (const Triangle& t : mTris)
		{
			Color area = GetAreaColor(t.area);
			Color fill = area;
			fill.a = 0.35f;
			Color inner = area;
			inner.a = 0.15f;
			const Vector3& p0 = mVertices[t.v[0]];
			const Vector3& p1 = mVertices[t.v[1]];
			const Vector3& p2 = mVertices[t.v[2]];
			draw.Triangle(p0 + fillLift, p1 + fillLift, p2 + fillLift, fill);
			draw.Line(p0 + lineLift, p1 + lineLift, inner);
			draw.Line(p1 + lineLift, p2 + lineLift, inner);
			draw.Line(p2 + lineLift, p0 + lineLift, inner);
		}
		for (const auto& [a, b] : mOutline) { draw.Line(a + lineLift, b + lineLift, outline); }
		for (const ResolvedLink& l : mLinks)
		{
			if (l.startTri < 0) { continue; }
			draw.Arrow(l.startPoint, l.endPoint, linkColor);
		}
	}
}
