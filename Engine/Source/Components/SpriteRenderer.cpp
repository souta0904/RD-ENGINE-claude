#include "RdPch.h"
#include "Components/SpriteRenderer.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture.h"
#include "Render/RenderTypes.h"
#include "Scene/Actor.h"

namespace rd
{
	void SpriteRenderer::Reflect(PropertyVisitor& v)
	{
		v.Property("Texture", texturePath, { .flags = PropertyMeta::AssetTexture });
		v.Property("Size", size, { .speed = 1.0f });
		v.Property("Pivot", pivot, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Color", color);
		v.Property("UV Rect", uvRect, { .speed = 0.01f });
		v.Property("Layer", layer);
		v.Property("Flip X", flipX);
		v.Property("Flip Y", flipY);
	}

	void SpriteRenderer::OnRender(RenderList& list)
	{
		if (!texturePath.empty() && (!mTexture || mTexture->GetPath() != texturePath))
		{
			mTexture = AssetManager::Get().LoadTexture(texturePath);
		}
		const Transform& t = GetTransform();
		Vector3 scale = t.GetLossyScale();
		SpriteDrawItem s;
		s.texture = mTexture.get();
		s.position = t.GetPosition().XY();
		s.size = Vector2(size.x * scale.x, size.y * scale.y);
		s.pivot = pivot;
		s.rotation = t.GetRotation().ToEuler().z;
		s.color = color;
		s.uvRect = uvRect;
		if (flipX) { std::swap(s.uvRect.x, s.uvRect.z); }
		if (flipY) { std::swap(s.uvRect.y, s.uvRect.w); }
		s.layer = layer;
		list.sprites.push_back(s);
	}
}
