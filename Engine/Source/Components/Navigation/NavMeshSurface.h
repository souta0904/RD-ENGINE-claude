#pragma once
#include "Scene/Component/Component.h"
#include <string>

namespace rd
{
	// 焼くときに集める物（Unity の NavMeshSurface の Collect Objects）
	//  All: シーン全体 / Volume: Center と Size の箱の中 / Children: このアクターと子孫
	enum class NavMeshCollectObjects { All, Volume, Children };

	// ナビメッシュの形（Unity の NavMeshSurface）
	//  Bake で、集めた物の MeshRenderer の三角形から歩ける面を焼き、シーンの隣のフォルダへ JSON で保存して
	//  data にそのパスを入れる（Unity に頼らない）。Unity から書き出した JSON を data に入れても読める。
	//  エディタでの表示は、シーンビューの「NavMesh」が オン のときか、このコンポーネントを持つ
	//  アクターを選んでいるときに、エディタがまとめて描く（Editor::DrawSceneGizmos）
	class RD_API NavMeshSurface final : public Component
	{
		RD_COMPONENT(NavMeshSurface)
	public:
		// エージェント（Unity の Navigation の Agents。既定は Humanoid と同じ）
		float agentRadius = 0.5f;
		float agentHeight = 2.0f;
		float agentMaxSlope = 45.0f;
		float agentStepHeight = 0.75f;

		NavMeshCollectObjects collectObjects = NavMeshCollectObjects::All;
		Vector3 size = Vector3(10.0f, 10.0f, 10.0f);
		Vector3 center = Vector3(0.0f, 2.0f, 0.0f);
		int32_t layerMask = -1;
		int32_t defaultArea = 0;
		// オフなら voxelSize は agentRadius / 3（Unity と同じ）
		bool overrideVoxelSize = false;
		float voxelSize = 0.1666667f;
		float minRegionArea = 2.0f;

		// 焼いた結果（または Unity から書き出した）三角形の JSON
		std::string data;

		void Reflect(PropertyVisitor& v) override;
		void Awake() override;

		// 焼いて保存し、シーンのナビメッシュを読み直す。保存先は GetBakePath()
		bool Bake();
		// 焼いた結果を外す（ファイルは消さない）
		void ClearData();
		// 焼いた結果の保存先（シーンが Assets/A/Stage.scene なら Assets/A/Stage/NavMesh-<アクター名>.json）
		std::string GetBakePath() const;
		float GetVoxelSize() const { return overrideVoxelSize ? voxelSize : agentRadius / 3.0f; }

		// 最後に焼いた結果（エディタのインスペクタに出す。保存はしない）
		struct BakeInfo
		{
			bool done = false;
			uint32_t triangles = 0;
			int meshes = 0;
			double ms = 0.0;
		};
		const BakeInfo& GetLastBake() const { return mLastBake; }

	private:
		BakeInfo mLastBake;
	};
}
