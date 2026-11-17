#pragma once
#include "AI/Navigation/NavMesh.h"
#include "Scene/Component/Component.h"

namespace rd
{
	class NavMesh;

	// ナビメッシュの上を目的地まで歩かせる（Unity の NavMeshAgent と同じ使い方）
	//  経路の曲がり角へ向かって加速し、updateRotation なら進む向きへ angularSpeed（度/秒）で向きを変える。
	//  高さは毎フレーム足もとのナビメッシュに合わせる。
	//  リンクの入口に着くと IsOnOffMeshLink が true になって止まる。跳ぶ動きは使う側が作り、
	//  終わったら CompleteOffMeshLink を呼ぶと、リンクの出口から続きを歩く。
	//  isStopped で止めても、SetDestination / SetPath で新しい経路を渡すとまた動きだす。
	class NavMeshAgent final : public Component
	{
		RD_COMPONENT(NavMeshAgent)
	public:
		struct OffMeshLinkData
		{
			Vector3 startPos;
			Vector3 endPos;
		};

		float speed = 3.5f;
		float angularSpeed = 120.0f;
		float acceleration = 8.0f;
		float stoppingDistance = 0.0f;
		float radius = 0.5f;
		float height = 2.0f;
		float baseOffset = 0.0f;
		bool autoBraking = true;

		bool isStopped = false;
		bool updatePosition = true;
		bool updateRotation = true;

		void Reflect(PropertyVisitor& v) override;
		void Update(float dt) override;
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif

		bool SetDestination(const Vector3& destination);
		bool CalculatePath(const Vector3& destination, NavMeshPath& outPath);
		bool SetPath(const NavMeshPath& path);
		void ResetPath();
		bool Warp(const Vector3& position);

		bool HasPath() const { return mHasPath; }
		bool IsPathPending() const { return false; }
		NavMeshPath::Status GetPathStatus() const { return mPath.status; }
		const Vector3& GetDestination() const { return mDestination; }
		float GetRemainingDistance() const;
		const Vector3& GetVelocity() const { return mVelocity; }
		void SetVelocity(const Vector3& velocity) { mVelocity = velocity; }

		bool IsOnOffMeshLink() const { return mOnLink; }
		const OffMeshLinkData& GetCurrentOffMeshLinkData() const { return mLinkData; }
		void CompleteOffMeshLink();

	private:
		NavMesh* GetMesh() const;
		Vector3 GetAgentPosition() const;
		void SetAgentPosition(const Vector3& position);

		NavMeshPath mPath;
		size_t mCorner = 0; // 次に向かう曲がり角
		int mTriangle = -1; // 足もとの三角形（わからなければ -1）
		Vector3 mDestination;
		Vector3 mVelocity;
		OffMeshLinkData mLinkData;
		bool mHasPath = false;
		bool mOnLink = false;
	};
}
