#include "RdPch.h"
#include "Components/Navigation/NavMeshLink.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Render/Debug/DebugDraw.h"
#include "Scene/Actor/Actor.h"
#include "Scene/Scene.h"

namespace rd
{
	void NavMeshLink::Reflect(PropertyVisitor& v)
	{
		v.Property("Start Point", startPoint);
		v.Property("End Point", endPoint);
		v.Property("Width", width, { .speed = 0.1f, .min = 0.0f, .max = 100.0f });
		v.Property("Cost Modifier", costModifier);
		v.Property("Is Overriding Cost", isOverridingCost);
		v.Property("Bidirectional", bidirectional);
		v.Property("Activated", activated);
		v.Property("Area", area);
	}

	void NavMeshLink::Start()
	{
		Register();
	}

	void NavMeshLink::OnDestroy()
	{
		if (Scene* scene = GetScene()) { scene->GetSystem<NavigationSystem>().RemoveLink(this); }
	}

	void NavMeshLink::Register()
	{
		NavigationSystem& nav = GetScene()->GetSystem<NavigationSystem>();
		if (!activated || !IsEnabled())
		{
			nav.RemoveLink(this);
			return;
		}
		// 両端はアクターから見た位置なので、ワールドの位置にしてから登録する
		const Matrix4& m = GetTransform().GetWorldMatrix();
		NavMesh::Link link;
		link.start = TransformPoint(startPoint, m);
		link.end = TransformPoint(endPoint, m);
		link.bidirectional = bidirectional;
		link.costModifier = isOverridingCost ? costModifier : 1.0f;
		link.area = area;
		nav.SetLink(this, link);
	}

#if RD_EDITOR
	void NavMeshLink::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		const Matrix4& m = GetTransform().GetWorldMatrix();
		Color color = selected ? Color(1.0f, 0.8f, 0.2f, 1.0f) : Color(1.0f, 0.6f, 0.1f, 0.5f);
		draw.Arrow(TransformPoint(startPoint, m), TransformPoint(endPoint, m), color);
	}
#endif
}
