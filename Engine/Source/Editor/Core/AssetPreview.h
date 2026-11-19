#pragma once
#include "Asset/Animation/Skeleton.h"
#include "Asset/Asset.h"
#include "Math/Geometry/AABB.h"
#include "Render/Material/Material.h"
#include "Render/Renderer/Renderer.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Model;
	class Scene;
	class Texture;

	// プレビュー用に、何かを専用の描画先へ描くものの共通部分
	//  カメラは描くものを包む球が収まる距離から、yaw / pitch（度）の向きで見る。zoom を小さくすると寄る
	class PreviewView
	{
	public:
		float yaw = 210.0f;
		float pitch = 20.0f;
		float zoom = 1.0f;

		virtual ~PreviewView() = default;
		// 描ける状態になった
		virtual bool IsReady() const = 0;
		// テクスチャまで全部読み終わった（失敗も含む）。サムネイルを描き直すのをやめる目安
		virtual bool IsFullyLoaded() = 0;
		// 描けたら true（結果は GetOutput）
		virtual bool Render(uint32_t width, uint32_t height) = 0;
		const GpuTexture& GetOutput() const { return mTargets.output; }

	protected:
		// mList に積んだものを、bounds が収まるカメラと 2 つのライトで描く
		void DrawList(const AABB& bounds, uint32_t width, uint32_t height);

		RenderTargets mTargets;
		RenderList mList;
	};

	// モデル 1 つだけを描く（マテリアルはモデルの既定値）。スキンメッシュはクリップを選ぶと動く
	class ModelView final : public PreviewView
	{
	public:
		// -1 ならバインドポーズ
		int clip = -1;
		float time = 0.0f;

		void SetModel(const std::string& path);
		const std::string& GetPath() const { return mPath; }
		Model* GetModel() const { return mModel.get(); }
		bool IsReady() const override;
		bool IsFullyLoaded() override;
		bool Render(uint32_t width, uint32_t height) override;

	private:
		void SyncMaterials();
		D3D12_GPU_VIRTUAL_ADDRESS BuildPalette();

		AssetRef<Model> mModel;
		std::string mPath;
		std::vector<Material> mMaterials;
		const Model* mSynced = nullptr;
		std::vector<TransformTRS> mLocal;
		std::vector<Matrix4> mGlobal;
		std::vector<Matrix4> mPalette;
		std::vector<int32_t> mBinding;
		int mBindingClip = -2;
	};

	// プレハブを描く（プレビュー専用の小さなシーンに置いて、そのシーンの描画物をそのまま描く）
	//  エンジンの形式（JSON）のプレハブだけ読める。Unity のプレハブ（YAML）は読めない
	class PrefabView final : public PreviewView
	{
	public:
		PrefabView();
		~PrefabView() override;

		void SetPrefab(const std::string& path);
		const std::string& GetPath() const { return mPath; }
		// 読めなかったときの理由（読めたら空）
		const std::string& GetError() const { return mError; }
		size_t GetActorCount() const;
		bool IsReady() const override;
		bool IsFullyLoaded() override;
		bool Render(uint32_t width, uint32_t height) override;

	private:
		AABB ComputeBounds() const;

		std::unique_ptr<Scene> mScene;
		std::string mPath;
		std::string mError;
	};

	// プレビューパネルのモデル（残りの広さいっぱいに描く。左ドラッグで回す・ホイールで寄る・ダブルクリックで元の向き）
	//  アニメーションがあればクリップを選んで再生できる
	class ModelPreviewWidget
	{
	public:
		void Draw(const std::string& path);

	private:
		ModelView mView;
		bool mPlaying = true;
	};

	// プレビューパネルのプレハブ（操作はモデルと同じ）
	class PrefabPreviewWidget
	{
	public:
		void Draw(const std::string& path);

	private:
		PrefabView mView;
	};

	// プロジェクトパネルのモデルとプレハブのサムネイル（1 フレームに少しずつ描いて覚えておく）
	class AssetThumbnails
	{
	public:
		// まだ描けていなければ nullptr（そのときは描く予約だけする）
		const GpuTexture* Get(const std::string& path);

	private:
		struct Entry
		{
			std::unique_ptr<PreviewView> view;
			bool rendered = false;
			bool done = false;
			int lastUsed = 0;
		};
		void Evict();

		std::unordered_map<std::string, Entry> mEntries;
		int mFrame = -1;
		int mRenderedThisFrame = 0;
	};

	// テクスチャを市松模様の上に、縦横比を保って幅いっぱいに描く（マウスを乗せると拡大鏡）
	void DrawTexturePreview(const Texture& texture, float maxHeight);
}
