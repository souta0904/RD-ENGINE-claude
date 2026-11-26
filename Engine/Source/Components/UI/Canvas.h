#pragma once
#include "Components/UI/UIDraw.h"
#include "Scene/Component/Component.h"

namespace rd
{
	class Actor;

	// 画面に重ねる UI のいちばん上（Unity の Canvas の Screen Space - Overlay と CanvasScaler）
	//  画面の大きさを Reference Resolution に合わせて拡大・縮小し（Match 0 で幅、1 で高さに合わせる）、
	//  子の RectTransform を辿って UIImage・UIText を描く。親に Canvas があるときは親がまとめて描く
	class RD_API Canvas final : public Component
	{
		RD_COMPONENT(Canvas)
	public:
		int32_t sortOrder = 0;
		Vector2 referenceResolution = { 1920.0f, 1080.0f };
		float match = 0.5f;

		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;

		// 画面の大きさから、キャンバスの 1 単位のピクセル数
		float ComputeScale(const Vector2& screen) const;
		// 最後に描いたときのキャンバスの大きさと倍率（ワールドの位置を UI に重ねるときに使う）
		Vector2 GetCanvasSize() const { return mCanvasSize; }
		float GetScreenScale() const { return mScreenScale; }

	private:
		void DrawActor(Actor& actor, const UIRect& parentRect, UIDrawContext ctx);

		Vector2 mCanvasSize;
		float mScreenScale = 1.0f;
	};
}
