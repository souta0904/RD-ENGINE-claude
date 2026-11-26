#include "RdPch.h"
#include "Components/UI/Canvas.h"
#include "Components/UI/CanvasGroup.h"
#include "Components/UI/RectTransform.h"
#include "Components/UI/UIImage.h"
#include "Components/UI/UIText.h"
#include "Render/RenderList/RenderList.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	namespace
	{
		// 画面の UI はほかのスプライトより手前に出す
		constexpr int32_t kCanvasLayerBase = 1000;

		float SrgbToLinear(float c)
		{
			return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
		}

		bool HasCanvasAbove(const Actor& actor)
		{
			for (Actor* p = actor.GetParent(); p; p = p->GetParent())
			{
				if (auto c = p->GetComponent<Canvas>(); c && c->IsEnabled()) { return true; }
			}
			return false;
		}
	}

	Color UIColorToLinear(const Color& srgb)
	{
		return { SrgbToLinear(srgb.r), SrgbToLinear(srgb.g), SrgbToLinear(srgb.b), srgb.a };
	}

	// 長方形の中心・横向き・縦向きを変換して、中心と大きさと回転のスプライトにする（画面は左上原点で下が +Y）
	void UIDrawContext::DrawQuad(const UIRect& rect, const Texture* texture, const Vector4& uv, const Color& color) const
	{
		if (color.a <= 0.0f || rect.Width() <= 0.0f || rect.Height() <= 0.0f) { return; }
		Vector2 center = (rect.min + rect.max) * 0.5f;
		Vector2 c = transform.Apply(center);
		Vector2 ex = transform.Apply({ rect.max.x, center.y }) - c;
		Vector2 ey = transform.Apply({ center.x, rect.max.y }) - c;
		SpriteDrawItem s;
		s.texture = texture;
		s.position = Vector2(c.x * screenScale, (canvasHeight - c.y) * screenScale);
		s.size = Vector2(2.0f * Length(ex) * screenScale, 2.0f * Length(ey) * screenScale);
		s.rotation = -std::atan2(ex.y, ex.x);
		s.color = color;
		s.uvRect = uv;
		s.layer = layer;
		list->sprites.push_back(s);
	}

	// ---- RectTransform ----

	void RectTransform::Reflect(PropertyVisitor& v)
	{
		v.Property("Anchor Min", anchorMin, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Anchor Max", anchorMax, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Pivot", pivot, { .speed = 0.01f });
		v.Property("Anchored Position", anchoredPosition, { .speed = 1.0f });
		v.Property("Size Delta", sizeDelta, { .speed = 1.0f });
	}

	UIRect RectTransform::ComputeRect(const UIRect& parent) const
	{
		Vector2 aMin = parent.Lerp(anchorMin), aMax = parent.Lerp(anchorMax);
		Vector2 size = (aMax - aMin) + sizeDelta;
		Vector2 reference = { math::Lerp(aMin.x, aMax.x, pivot.x), math::Lerp(aMin.y, aMax.y, pivot.y) };
		Vector2 pivotPos = reference + anchoredPosition;
		UIRect r;
		r.min = { pivotPos.x - size.x * pivot.x, pivotPos.y - size.y * pivot.y };
		r.max = r.min + size;
		return r;
	}

	// ---- Canvas ----

	void Canvas::Reflect(PropertyVisitor& v)
	{
		v.Property("Sort Order", sortOrder);
		v.Property("Reference Resolution", referenceResolution, { .speed = 1.0f, .min = 1.0f, .max = 16384.0f });
		v.Property("Match", match, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
	}

	// CanvasScaler の Scale With Screen Size（幅と高さの倍率を対数で混ぜる）
	float Canvas::ComputeScale(const Vector2& screen) const
	{
		float w = std::log2(std::max(screen.x, 1.0f) / std::max(referenceResolution.x, 1.0f));
		float h = std::log2(std::max(screen.y, 1.0f) / std::max(referenceResolution.y, 1.0f));
		return std::pow(2.0f, math::Lerp(w, h, math::Saturate(match)));
	}

	void Canvas::OnRender(RenderList& list)
	{
		Vector2 screen = list.uiScreenSize;
		if (screen.x <= 0.0f || screen.y <= 0.0f) { return; }
		Actor* self = GetActor();
		if (!self->IsActive() || HasCanvasAbove(*self)) { return; }

		mScreenScale = ComputeScale(screen);
		mCanvasSize = screen / mScreenScale;
		UIDrawContext ctx;
		ctx.list = &list;
		ctx.screenScale = mScreenScale;
		ctx.canvasHeight = mCanvasSize.y;
		ctx.layer = kCanvasLayerBase + sortOrder;
		if (auto group = GetComponent<CanvasGroup>(); group && group->IsEnabled()) { ctx.alpha = group->alpha; }
		if (ctx.alpha <= 0.0f) { return; }

		UIRect root{ Vector2(), mCanvasSize };
		if (auto rt = GetComponent<RectTransform>()) { rt->mLastRect = root; }
		for (Actor* child : self->GetChildren()) { DrawActor(*child, root, ctx); }
	}

	// 親より先に自分を描き、子は兄弟の順に重ねる（後の兄弟ほど手前）
	void Canvas::DrawActor(Actor& actor, const UIRect& parentRect, UIDrawContext ctx)
	{
		if (!actor.IsActiveSelf() || actor.IsPendingDestroy()) { return; }
		auto rt = actor.GetComponent<RectTransform>();
		UIRect rect = rt ? rt->ComputeRect(parentRect) : parentRect;

		const Transform& t = actor.GetTransform();
		Vector3 scale = t.GetLocalScale();
		float angle = t.GetLocalRotation().ToEuler().z;
		if (scale.x != 1.0f || scale.y != 1.0f || angle != 0.0f)
		{
			Vector2 pivot = rect.Lerp(rt ? rt->pivot : Vector2(0.5f, 0.5f));
			ctx.transform = ctx.transform * UIAffine::AroundPivot(pivot, scale.XY(), angle);
		}
		if (auto group = actor.GetComponent<CanvasGroup>(); group && group->IsEnabled()) { ctx.alpha *= group->alpha; }
		if (rt)
		{
			Vector2 a = ctx.transform.Apply(rect.min), b = ctx.transform.Apply(rect.max);
			rt->mLastRect = { Min(a, b), Max(a, b) };
		}
		if (ctx.alpha <= 0.0f) { return; }

		if (auto image = actor.GetComponent<UIImage>(); image && image->IsEnabled()) { image->Draw(rect, ctx); }
		if (auto text = actor.GetComponent<UIText>(); text && text->IsEnabled()) { text->Draw(rect, ctx); }
		for (Actor* child : actor.GetChildren()) { DrawActor(*child, rect, ctx); }
	}
}
