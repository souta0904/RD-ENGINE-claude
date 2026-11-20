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
	class MaterialAsset;
	class Model;
	class Scene;
	class Texture;

	// プレビュー用に、何かを専用の描画先へ描くものの共通部分
	//  カメラは描くものを包む球が収まる距離から、yaw / pitch（度）の向きで見る。zoom を小さくすると寄る
	class RD_API PreviewView
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
		// 前に描いた後で中身が変わった（サムネイルを描き直す）
		virtual bool NeedsRefresh() const { return false; }
		const GpuTexture& GetOutput() const { return mTargets.output; }

	protected:
		// mList に積んだものを、bounds が収まるカメラと 2 つのライトで描く
		void DrawList(const AABB& bounds, uint32_t width, uint32_t height);

		RenderTargets mTargets;
		RenderList mList;
	};

	// モデル 1 つだけを描く（マテリアルはモデルの既定値）。スキンメッシュはクリップを選ぶと動く
	class RD_API ModelView final : public PreviewView
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
	class RD_API PrefabView final : public PreviewView
	{
	public:
		PrefabView();
		~PrefabView() override;

		void SetPrefab(const std::string& path);
		// 同じプレハブを置き直す（ゲームのコードを読み直したとき）
		void Reload();
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

	// マテリアル（.rdmat）を球に塗って描く（Unity のマテリアルのプレビューと同じ）
	//  値を変えると GetVersion が増えるので、サムネイルはそれを見て描き直す
	class RD_API MaterialView final : public PreviewView
	{
	public:
		void SetMaterial(const std::string& path);
		bool IsReady() const override;
		bool IsFullyLoaded() override;
		bool Render(uint32_t width, uint32_t height) override;
		bool NeedsRefresh() const override;

	private:
		AssetRef<Model> mSphere;
		AssetRef<MaterialAsset> mAsset;
		std::string mPath;
		uint32_t mRenderedVersion = ~0u;
	};

	// プレビューパネルのマテリアル（操作はモデルと同じ）
	class RD_API MaterialPreviewWidget
	{
	public:
		void Draw(const std::string& path);

	private:
		MaterialView mView;
	};

	// プレビューパネルのモデル（残りの広さいっぱいに描く。左ドラッグで回す・ホイールで寄る・ダブルクリックで元の向き）
	//  アニメーションがあればクリップを選んで再生できる
	class RD_API ModelPreviewWidget
	{
	public:
		void Draw(const std::string& path);

	private:
		ModelView mView;
		bool mPlaying = true;
	};

	// プレビューパネルのプレハブ（操作はモデルと同じ）
	class RD_API PrefabPreviewWidget
	{
	public:
		void Draw(const std::string& path);
		void Reload() { mView.Reload(); }

	private:
		PrefabView mView;
	};

	// プロジェクトパネルのモデルとプレハブのサムネイル（1 フレームに少しずつ描いて覚えておく）
	class RD_API AssetThumbnails
	{
	public:
		AssetThumbnails() = default;
		// RD_API で DLL から出すと、コピーできないメンバーのコピーまで作ろうとするので禁止しておく
		AssetThumbnails(const AssetThumbnails&) = delete;
		AssetThumbnails& operator=(const AssetThumbnails&) = delete;

		// まだ描けていなければ nullptr（そのときは描く予約だけする）
		const GpuTexture* Get(const std::string& path);
		// プレハブのサムネイルを捨てる（次に見えたときに描き直す。ゲームのコードを読み直したとき）
		void ForgetPrefabs();

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
	RD_API void DrawTexturePreview(const Texture& texture, float maxHeight);
}
