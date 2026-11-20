#pragma once
#include "Math/Color.h"
#include "Math/Vector3.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	class DebugDraw;

	// 経路探索の結果（Unity の NavMeshPath と同じ考え方）
	//  Complete: 目的地まで行ける / Partial: 行けないので、いちばん近づける場所まで / Invalid: 経路がない
	//  links[i] が 0 以上なら、corners[i] から corners[i + 1] へはそのリンクを跳んで渡る
	struct RD_API NavMeshPath
	{
		enum class Status { Complete, Partial, Invalid };

		Status status = Status::Invalid;
		std::vector<Vector3> corners;
		std::vector<int> links;

		void Clear() { status = Status::Invalid; corners.clear(); links.clear(); }
	};

	// 歩ける面を三角形で持つナビメッシュ
	//  頂点は位置で 1 つにまとめ、辺を共有する三角形どうしをつなぐ。タイルの境目のように頂点がずれて
	//  辺の一部だけが重なるところは、同じ直線上で重なる区間を通り道にする。
	//  リンク（離れた場所どうしを跳んで渡る道）は AddLink で足す。両端はいちばん近い三角形につなぐ。
	//  経路は三角形の上を A* で探し、通る辺の並びを糸を張るように縮めて（funnel）曲がり角を求める。
	class RD_API NavMesh
	{
	public:
		struct Link
		{
			Vector3 start;
			Vector3 end;
			bool bidirectional = true;
			float costModifier = 1.0f;
			int area = 0;
		};

		// Unity の NavMesh.CalculateTriangulation を書き出した JSON（Vertices / Indices / Areas）を読む
		bool LoadJson(const std::string& path);
		void Build(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices, const std::vector<int>& areas);
		void Clear();

		int AddLink(const Link& link);
		void ClearLinks();

		bool IsEmpty() const { return mTris.empty(); }
		uint32_t GetTriangleCount() const { return uint32_t(mTris.size()); }

		// maxDistance 以内でいちばん近いナビメッシュ上の点（なければ false）
		bool SamplePosition(const Vector3& position, float maxDistance, Vector3& outPoint, int* outTriangle = nullptr) const;
		// ioTriangle の三角形から、つながった三角形だけをたどって target の真下（上）の点を探す
		//  見つからなければ、たどった中で target にいちばん近い点で止める（壁に沿って止まる）。
		//  いちばん近い面へ吸い付くと、床のすぐ上にある別の面へ乗り移ってしまうのを防ぐ
		Vector3 MoveAlongSurface(int& ioTriangle, const Vector3& target) const;
		// start から end への経路。始点が乗らなければ false
		bool CalculatePath(const Vector3& start, const Vector3& end, NavMeshPath& outPath) const;

		// Unity のように見せる：歩ける面を区画の色で半透明に塗り、内側の三角形の辺は薄く、
		//  外周（隣の三角形がない辺）ははっきりした線で描く。リンクはオレンジの矢印
		void DrawGizmos(DebugDraw& draw) const;
		// 区画ごとの表示色（0: Walkable は青、1: Not Walkable は灰、2: Jump は黄、それ以外は番号から決めた色）
		static Color GetAreaColor(int area);
		// 外周の線分の数（テスト用）
		size_t GetOutlineSegmentCount() const { return mOutline.size(); }

		// 目的地をナビメッシュへ寄せるときに探す距離
		float destinationSearchDistance = 10.0f;
		// 始点をナビメッシュへ寄せるときに探す距離
		float startSearchDistance = 4.0f;
		// リンクの端をつなぐ三角形を探す距離
		float linkSearchDistance = 3.0f;

	private:
		struct Edge
		{
			int to = -1;
			Vector3 a, b;      // 通り道の両端（リンクなら始点と終点）
			int link = -1;     // リンクを渡るときはリンクの番号
			bool reverse = false;
		};
		struct Triangle
		{
			uint32_t v[3] = {};
			int area = 0;
			Vector3 center;
			std::vector<Edge> edges;
		};
		struct ResolvedLink
		{
			Link link;
			int startTri = -1;
			int endTri = -1;
			Vector3 startPoint;
			Vector3 endPoint;
		};
		struct Step
		{
			int tri = -1;
			const Edge* via = nullptr; // この三角形へ入ってきた辺（最初は nullptr）
		};

		void ConnectSharedEdges();
		void ConnectOverlappingEdges(const std::vector<std::pair<int, int>>& openEdges);
		void BuildGrid();
		void BuildOutline();
		void ConnectLink(int index);
		Vector3 ClosestPointOnTriangle(int tri, const Vector3& p) const;
		bool FindTriangles(const Vector3& start, const Vector3& end, std::vector<Step>& outSteps, bool& outComplete, Vector3& ioEnd, int endTri) const;
		void StringPull(const Vector3& start, const Vector3& end, const std::vector<Step>& steps, size_t first, size_t last, std::vector<Vector3>& out) const;
		int64_t CellKey(int x, int z) const { return (int64_t(x) << 32) ^ int64_t(uint32_t(z)); }

		std::vector<Vector3> mVertices;
		std::vector<Triangle> mTris;
		std::vector<ResolvedLink> mLinks;
		std::unordered_map<int64_t, std::vector<int>> mGrid;
		// 外周の線分（三角形の辺のうち、どの三角形ともつながっていない区間）。Build のときに 1 回だけ求める
		std::vector<std::pair<Vector3, Vector3>> mOutline;
		float mCellSize = 4.0f;
	};
}
