#include "RdPch.h"
#include "Editor/Preview/AssetThumbnails.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Preview/MaterialView.h"
#include "Editor/Preview/ModelView.h"
#include "Editor/Preview/PrefabView.h"
#include "Render/Material/MaterialAsset.h"
#include <ImGui/imgui.h>

namespace rd
{
	namespace
	{
		constexpr uint32_t kThumbnailSize = 128;
		// サムネイルを 1 フレームに描く数と、覚えておく数（超えたら最近見ていないものから捨てる）
		constexpr int kThumbnailsPerFrame = 2;
		constexpr size_t kMaxThumbnails = 256;
		constexpr int kThumbnailKeepFrames = 120;
	}

	const GpuTexture* AssetThumbnails::Get(const std::string& path)
	{
		int frame = ImGui::GetFrameCount();
		if (frame != mFrame)
		{
			mFrame = frame;
			mRenderedThisFrame = 0;
			if (mEntries.size() > kMaxThumbnails) { Evict(); }
		}
		Entry& e = mEntries[path];
		e.lastUsed = frame;
		if (!e.view)
		{
			if (MaterialAsset::IsMaterialPath(path))
			{
				auto view = std::make_unique<MaterialView>();
				view->SetMaterial(path);
				e.view = std::move(view);
			}
			else if (GetExtension(path) == ".prefab")
			{
				auto view = std::make_unique<PrefabView>();
				view->SetPrefab(path);
				e.view = std::move(view);
			}
			else
			{
				auto view = std::make_unique<ModelView>();
				view->SetModel(path);
				e.view = std::move(view);
			}
		}
		// テクスチャが読み終わるまでは描き直す。中身が変わったら（マテリアルの編集）また描き直す
		if (e.done && e.view->NeedsRefresh()) { e.done = false; }
		if (!e.done && mRenderedThisFrame < kThumbnailsPerFrame && e.view->IsReady())
		{
			bool loaded = e.view->IsFullyLoaded();
			if (e.view->Render(kThumbnailSize, kThumbnailSize))
			{
				++mRenderedThisFrame;
				e.rendered = true;
				e.done = loaded;
			}
		}
		return e.rendered ? &e.view->GetOutput() : nullptr;
	}

	void AssetThumbnails::ForgetPrefabs()
	{
		std::erase_if(mEntries, [](const auto& kv) { return GetExtension(kv.first) == ".prefab"; });
	}

	void AssetThumbnails::Evict()
	{
		std::erase_if(mEntries, [&](const auto& kv) { return kv.second.lastUsed < mFrame - kThumbnailKeepFrames; });
	}
}
