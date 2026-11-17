#pragma once
#include "Asset/Animation/Skeleton.h"
#include "Asset/Asset.h"
#include "Render/Material/Material.h"
#include "Render/Renderer/Renderer.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Model;
	class Texture;

	// モデル 1 つだけを専用の描画先へ描く（マテリアルはモデルの既定値）
	//  カメラは境界の球が収まる距離から、yaw / pitch（度）の向きで見る。スキンメッシュはクリップを選ぶと動く
	class ModelView
	{
	public:
		float yaw = 210.0f;
		float pitch = 20.0f;
		float zoom = 1.0f;
		// -1 ならバインドポーズ
		int clip = -1;
		float time = 0.0f;

		void SetModel(const std::string& path);
		const std::string& GetPath() const { return mPath; }
		Model* GetModel() const { return mModel.get(); }
		bool IsReady() const;
		// モデルもテクスチャも読み終わった（失敗も含む）
		bool IsFullyLoaded();

		// 描けたら true（結果は GetOutput）
		bool Render(uint32_t width, uint32_t height);
		const GpuTexture& GetOutput() const { return mTargets.output; }

	private:
		void SyncMaterials();
		D3D12_GPU_VIRTUAL_ADDRESS BuildPalette();

		RenderTargets mTargets;
		RenderList mList;
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

	// インスペクタのモデルのプレビュー（左ドラッグで回す・ホイールで寄る・ダブルクリックで元の向き）
	//  アニメーションがあればクリップを選んで再生できる
	class ModelPreviewWidget
	{
	public:
		void Draw(const std::string& path);

	private:
		ModelView mView;
		bool mPlaying = true;
	};

	// プロジェクトパネルのモデルのサムネイル（1 フレームに少しずつ描いて覚えておく）
	class ModelThumbnails
	{
	public:
		// まだ描けていなければ nullptr（そのときは描く予約だけする）
		const GpuTexture* Get(const std::string& path);

	private:
		struct Entry
		{
			std::unique_ptr<ModelView> view;
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
