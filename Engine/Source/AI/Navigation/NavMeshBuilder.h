#pragma once
#include "Math/Vector3.h"
#include <string>
#include <vector>

namespace rd
{
	// ナビメッシュを焼く設定（Unity の NavMesh のエージェントの設定と同じ意味）
	struct NavMeshBuildSettings
	{
		float agentRadius = 0.5f;   // 壁からこれだけ離れたところまでを歩ける面にする
		float agentHeight = 2.0f;   // 天井までこれだけ空いていないところは通れない
		float agentMaxSlope = 45.0f; // これより急な坂（度）は歩けない
		float agentClimb = 0.75f;   // この高さまでの段差は乗り越えられる
		float cellSize = 0.1666667f; // ボクセルの横の大きさ（小さいほど細かいが遅い）
		float cellHeight = 0.0f;    // ボクセルの高さ（0 なら cellSize の半分）
		float minRegionArea = 2.0f; // これより狭い（m^2）離れ小島は捨てる
		float maxRectSize = 4.8f;   // 1 枚の四角形の辺の長さの上限（m）。大きすぎると経路の距離の見積もりが粗くなる
		// true なら、この箱（ワールド座標）の中だけで焼く（Unity の Collect Objects: Volume）
		bool useBounds = false;
		Vector3 boundsMin;
		Vector3 boundsMax;
	};

	// 焼いた結果（NavMesh::Build と、保存する JSON の形のまま）
	struct RD_API NavMeshBuildResult
	{
		std::vector<Vector3> vertices;
		std::vector<uint32_t> indices;
		std::vector<int> areas;

		uint32_t GetTriangleCount() const { return uint32_t(indices.size() / 3); }
		// NavMesh::LoadJson で読める JSON（Vertices / Indices / Areas）で保存する
		bool SaveJson(const std::string& path) const;
	};

	// 三角形の集まり（床・壁・家具など）からナビメッシュを焼く
	//  Recast と同じ流れ:
	//   1. 三角形をボクセルの柱（高さの区間）に塗る。上を向いた緩い面だけを「歩ける」とする
	//   2. 小さな段差を乗り越えられるようにし、頭がつかえるところを外す
	//   3. 上が空いている面どうしを、段差が agentClimb 以内なら隣としてつなぐ
	//   4. 縁（壁際・段の端）から agentRadius の内側までを削り、狭い離れ小島を捨てる
	//   5. 残ったマスを、高さが平面に乗る範囲で大きな四角形にまとめ、2 枚の三角形にする
	//  四角形どうしは辺の一部だけが重なる（T 字の頂点ができる）が、NavMesh はそれをつないで扱える
	class RD_API NavMeshBuilder
	{
	public:
		// ワールド座標の三角形を足す。area は歩ける面の区画の番号（0: Walkable。1: Not Walkable なら歩けない障害物として塗る）
		void AddTriangles(const std::vector<Vector3>& positions, const std::vector<uint32_t>& indices, int area = 0);
		void Clear();
		size_t GetInputTriangleCount() const { return mIndices.size() / 3; }

		// タイル（256 マス四方）に分けて、並べて焼く
		bool Build(const NavMeshBuildSettings& settings, NavMeshBuildResult& out) const;

	private:
		std::vector<Vector3> mPositions;
		std::vector<uint32_t> mIndices;
		std::vector<int> mAreas;
	};
}
