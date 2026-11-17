#include "RdPch.h"
#include "Components/Navigation/NavMeshAgent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Render/Debug/DebugDraw.h"
#include "Scene/Actor/Actor.h"
#include "Scene/Scene.h"

namespace rd
{
	namespace
	{
		constexpr float kArriveDistance = 0.1f;
		// 足もとのナビメッシュを探す距離
		constexpr float kGroundSearch = 2.0f;

		float HorizontalLength(const Vector3& v) { return std::sqrt(v.x * v.x + v.z * v.z); }

		Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maxDelta)
		{
			Vector3 diff = target - current;
			float length = Length(diff);
			if (length <= maxDelta || length <= 1.0e-6f) { return target; }
			return current + diff / length * maxDelta;
		}

		Quaternion RotateTowards(const Quaternion& from, const Quaternion& to, float maxDegrees)
		{
			float dot = std::fabs(Dot(from, to));
			float angle = math::ToDegrees(2.0f * std::acos(std::min(dot, 1.0f)));
			if (angle <= maxDegrees || angle <= 1.0e-4f) { return to; }
			return Slerp(from, to, maxDegrees / angle);
		}
	}

	void NavMeshAgent::Reflect(PropertyVisitor& v)
	{
		v.Property("Speed", speed, { .speed = 0.1f, .min = 0.0f, .max = 1000.0f });
		v.Property("Angular Speed", angularSpeed, { .speed = 1.0f, .min = 0.0f, .max = 10000.0f });
		v.Property("Acceleration", acceleration, { .speed = 0.1f, .min = 0.0f, .max = 10000.0f });
		v.Property("Stopping Distance", stoppingDistance, { .speed = 0.05f, .min = 0.0f, .max = 1000.0f });
		v.Property("Radius", radius, { .speed = 0.05f, .min = 0.0f, .max = 100.0f });
		v.Property("Height", height, { .speed = 0.05f, .min = 0.0f, .max = 100.0f });
		v.Property("Base Offset", baseOffset);
		v.Property("Auto Braking", autoBraking);
	}

	NavMesh* NavMeshAgent::GetMesh() const
	{
		NavMesh& mesh = GetScene()->GetSystem<NavigationSystem>().GetMesh();
		return mesh.IsEmpty() ? nullptr : &mesh;
	}

	Vector3 NavMeshAgent::GetAgentPosition() const
	{
		return GetTransform().GetPosition() - Vector3(0.0f, baseOffset, 0.0f);
	}

	void NavMeshAgent::SetAgentPosition(const Vector3& position)
	{
		GetTransform().SetPosition(position + Vector3(0.0f, baseOffset, 0.0f));
	}

	bool NavMeshAgent::CalculatePath(const Vector3& destination, NavMeshPath& outPath)
	{
		NavMesh* mesh = GetMesh();
		if (!mesh)
		{
			outPath.Clear();
			return false;
		}
		return mesh->CalculatePath(GetAgentPosition(), destination, outPath);
	}

	bool NavMeshAgent::SetDestination(const Vector3& destination)
	{
		mDestination = destination;
		NavMeshPath path;
		if (!CalculatePath(destination, path)) { return false; }
		return SetPath(path);
	}

	bool NavMeshAgent::SetPath(const NavMeshPath& path)
	{
		if (path.status == NavMeshPath::Status::Invalid || path.corners.empty())
		{
			ResetPath();
			return false;
		}
		mPath = path;
		mCorner = std::min<size_t>(1, mPath.corners.size() - 1);
		mHasPath = true;
		mOnLink = false;
		// 新しい経路を渡されたら動き出す（止めたあとに目的地を決め直すと、また歩きだす）
		isStopped = false;
		return true;
	}

	void NavMeshAgent::ResetPath()
	{
		mPath.Clear();
		mCorner = 0;
		mHasPath = false;
		mOnLink = false;
	}

	bool NavMeshAgent::Warp(const Vector3& position)
	{
		ResetPath();
		mVelocity = Vector3::kZero;
		Vector3 onMesh = position;
		mTriangle = -1;
		if (NavMesh* mesh = GetMesh()) { mesh->SamplePosition(position, kGroundSearch, onMesh, &mTriangle); }
		SetAgentPosition(onMesh);
		return true;
	}

	float NavMeshAgent::GetRemainingDistance() const
	{
		if (!mHasPath || mPath.corners.empty()) { return 0.0f; }
		float distance = Length(mPath.corners[mCorner] - GetAgentPosition());
		for (size_t i = mCorner + 1; i < mPath.corners.size(); ++i)
		{
			distance += Length(mPath.corners[i] - mPath.corners[i - 1]);
		}
		return distance;
	}

	void NavMeshAgent::CompleteOffMeshLink()
	{
		if (!mOnLink) { return; }
		mOnLink = false;
		SetAgentPosition(mLinkData.endPos);
		// 着地した面を探し直す
		mTriangle = -1;
		if (NavMesh* mesh = GetMesh()) { mesh->SamplePosition(mLinkData.endPos, kGroundSearch, mLinkData.endPos, &mTriangle); }
		// 着地点（リンクの出口）は通り過ぎたので、その次の曲がり角へ向かう
		if (mCorner + 1 < mPath.corners.size()) { ++mCorner; }
	}

	void NavMeshAgent::Update(float dt)
	{
		if (!mHasPath || mOnLink || !updatePosition) { return; }
		if (isStopped)
		{
			mVelocity = Vector3::kZero;
			return;
		}

		Vector3 position = GetAgentPosition();
		size_t last = mPath.corners.size() - 1;

		// 着いた曲がり角は飛ばす。リンクの入口に着いたら止まって、跳ぶのを待つ
		while (true)
		{
			if (mCorner > 0 && mPath.links[mCorner - 1] >= 0)
			{
				mOnLink = true;
				mLinkData.startPos = mPath.corners[mCorner - 1];
				mLinkData.endPos = mPath.corners[mCorner];
				mVelocity = Vector3::kZero;
				return;
			}
			if (mCorner < last && HorizontalLength(mPath.corners[mCorner] - position) <= std::max(kArriveDistance, speed * dt))
			{
				++mCorner;
				continue;
			}
			break;
		}

		Vector3 target = mPath.corners[mCorner];
		Vector3 toTarget = target - position;
		toTarget.y = 0.0f;
		float distance = Length(toTarget);
		float remaining = GetRemainingDistance();

		float desired = speed;
		if (mCorner == last)
		{
			if (remaining <= std::max(stoppingDistance, kArriveDistance)) { desired = 0.0f; }
			else if (autoBraking) { desired = std::min(speed, std::sqrt(2.0f * acceleration * remaining)); }
		}
		Vector3 direction = distance > 1.0e-5f ? toTarget / distance : Vector3::kZero;
		mVelocity = MoveTowards(mVelocity, direction * desired, acceleration * dt);

		Vector3 step = mVelocity * dt;
		step.y = 0.0f;
		if (Length(step) > distance) { step = toTarget; }
		Vector3 next = position + step;
		if (NavMesh* mesh = GetMesh())
		{
			// 今いる三角形からつながった面の上を動く（高さを合わせ、ナビメッシュの外へは出ない）
			next = mesh->MoveAlongSurface(mTriangle, next);
		}
		SetAgentPosition(next);

		if (updateRotation)
		{
			Vector3 facing(mVelocity.x, 0.0f, mVelocity.z);
			if (LengthSq(facing) > 0.01f)
			{
				Transform& t = GetTransform();
				t.SetRotation(RotateTowards(t.GetRotation(), Quaternion::LookRotation(Normalize(facing)), angularSpeed * dt));
			}
		}
	}

#if RD_EDITOR
	void NavMeshAgent::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		if (!selected || !mHasPath) { return; }
		for (size_t i = 1; i < mPath.corners.size(); ++i)
		{
			bool link = mPath.links[i - 1] >= 0;
			draw.Line(mPath.corners[i - 1], mPath.corners[i], link ? Color(1.0f, 0.6f, 0.1f, 1.0f) : Color(0.2f, 1.0f, 0.4f, 1.0f), false);
		}
	}
#endif
}
