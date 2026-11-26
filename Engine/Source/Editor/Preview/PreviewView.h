#pragma once
#include "Asset/Asset.h"
#include "Math/Geometry/AABB.h"
#include "Render/Renderer/Renderer.h"

namespace rd
{
	class Texture;

	// プレビュー用に、何かを専用の描画先へ描くものの共通部分（ModelView・PrefabView・MaterialView）
	//  カメラは描くものを包む球が収まる距離から、yaw / pitch（度）の向きで見る。zoom を小さくすると寄る
	class RD_API PreviewView
	{
	public:
		static constexpr float kDefaultYaw = 210.0f;
		static constexpr float kDefaultPitch = 20.0f;

		float yaw = kDefaultYaw;
		float pitch = kDefaultPitch;
		float zoom = 1.0f;

		virtual ~PreviewView() = default;
		// 描ける状態になった
		virtual bool IsReady() const = 0;
		// テクスチャまで全部読み終わった（失敗も含む）。サムネイルを描き直すのをやめる目安
		virtual bool IsFullyLoaded() = 0;
		// 描けたら true（結果は GetOutput）
		virtual bool Render(uint32_t width, uint32_t height) = 0;
		// 前に描いた後で中身が変わった（サムネイルを描き直す）
		virtual bool NeedsRefresh() const { return false; }
		const GpuTexture& GetOutput() const { return mTargets.output; }

	protected:
		// mList に積んだものを、bounds が収まるカメラと 2 つのライトで描く
		void DrawList(const AABB& bounds, uint32_t width, uint32_t height);
		// テクスチャが読み終わった（ない・失敗も含む）
		static bool IsSettled(const AssetRef<Texture>& texture);

		RenderTargets mTargets;
		RenderList mList;
	};
}
