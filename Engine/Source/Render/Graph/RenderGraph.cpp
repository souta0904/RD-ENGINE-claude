#include "RdPch.h"
#include "Render/Graph/RenderGraph.h"
#include "Core/Util/StringUtil.h"
#include "RHI/Device/GpuScope.h"
#include "RHI/Texture/DepthTexture.h"
#include "RHI/Texture/RenderTexture.h"

namespace rd::rg
{
	namespace
	{
		// この回数だけ続けて使われなかった実体は捨てる（大きさを変えたときの古いものなど）
		constexpr uint64_t kEvictAfter = 300;

		bool IsWrite(Access a) { return a == Access::RenderTarget || a == Access::DepthWrite; }
	}

	bool TextureDesc::SameAs(const TextureDesc& o) const
	{
		if (depth != o.depth || width != o.width || height != o.height) { return false; }
		if (depth) { return true; }
		return format == o.format && rtvFormat == o.rtvFormat && srvFormat == o.srvFormat &&
			clearColor.r == o.clearColor.r && clearColor.g == o.clearColor.g && clearColor.b == o.clearColor.b && clearColor.a == o.clearColor.a;
	}

	D3D12_RESOURCE_STATES ToState(Access access)
	{
		switch (access)
		{
		// どの段のシェーダーからも読めるようにする（頂点シェーダーからも見える枠に置くことがある）
		case Access::ShaderRead: return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		case Access::RenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case Access::DepthWrite: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case Access::DepthRead: return D3D12_RESOURCE_STATE_DEPTH_READ;
		case Access::CopySource: return D3D12_RESOURCE_STATE_COPY_SOURCE;
		}
		return D3D12_RESOURCE_STATE_COMMON;
	}

	const char* ToString(Access access)
	{
		switch (access)
		{
		case Access::ShaderRead: return "Read";
		case Access::RenderTarget: return "Write";
		case Access::DepthWrite: return "Depth Write";
		case Access::DepthRead: return "Depth Read";
		case Access::CopySource: return "Copy";
		}
		return "";
	}

	// ---------------------------------------------------------------- PassBuilder / PassContext

	void PassBuilder::SideEffect() { mGraph.mPasses[mPass].sideEffect = true; }

	TextureHandle PassBuilder::Use(TextureHandle h, Access access, bool overwrite)
	{
		RD_ASSERT(h.IsValid() && h.index < mGraph.mResources.size(), "Render graph: invalid texture handle");
		mGraph.mPasses[mPass].uses.push_back({ h.index, access, overwrite });
		return h;
	}

	RenderTexture& PassContext::Color(TextureHandle h) const { return static_cast<RenderTexture&>(mGraph.Resolve(h)); }
	DepthTexture& PassContext::Depth(TextureHandle h) const { return static_cast<DepthTexture&>(mGraph.Resolve(h)); }
	GpuTexture& PassContext::Texture(TextureHandle h) const { return mGraph.Resolve(h); }

	// ---------------------------------------------------------------- 実体

	struct RenderGraph::PoolEntry
	{
		TextureDesc desc;
		std::wstring name;
		std::string debugName;
		std::unique_ptr<RenderTexture> color;
		std::unique_ptr<DepthTexture> depth;
		uint64_t lastUsed = 0;

		GpuTexture& Get() { return color ? static_cast<GpuTexture&>(*color) : static_cast<GpuTexture&>(*depth); }
	};

	RenderGraph::RenderGraph() = default;
	RenderGraph::~RenderGraph() = default;

	void RenderGraph::Reset()
	{
		mPasses.clear();
		mResources.clear();
		mPhysicalDescs.clear();
		mCompiled = false;
	}

	TextureHandle RenderGraph::Create(const char* name, const TextureDesc& desc)
	{
		Resource r;
		r.name = name;
		r.desc = desc;
		r.desc.width = std::max(1u, desc.width);
		r.desc.height = std::max(1u, desc.height);
		mResources.push_back(std::move(r));
		return { uint32_t(mResources.size() - 1) };
	}

	TextureHandle RenderGraph::Import(const char* name, GpuTexture* texture, D3D12_RESOURCE_STATES finalState, bool output, bool depth)
	{
		Resource r;
		r.name = name;
		r.imported = texture;
		r.isImported = true;
		r.output = output;
		r.finalState = finalState;
		r.desc.depth = depth;
		if (texture)
		{
			r.desc.width = texture->GetWidth();
			r.desc.height = texture->GetHeight();
			r.desc.format = texture->GetResource() ? texture->GetResource()->GetDesc().Format : DXGI_FORMAT_UNKNOWN;
		}
		mResources.push_back(std::move(r));
		return { uint32_t(mResources.size() - 1) };
	}

	void RenderGraph::AddPass(const char* name, const SetupFn& setup, ExecuteFn execute)
	{
		Pass p;
		p.name = name;
		p.execute = std::move(execute);
		mPasses.push_back(std::move(p));
		PassBuilder builder(*this, uint32_t(mPasses.size() - 1));
		if (setup) { setup(builder); }
	}

	// ---------------------------------------------------------------- Compile

	void RenderGraph::Compile()
	{
		// 1. 後ろから見て、必要なテクスチャに書くパスだけを残す
		//    残したパスが読むものは必要になる。中身を全部書き直すなら、それより前の中身は要らない
		std::vector<bool> needed(mResources.size(), false);
		for (size_t i = 0; i < mResources.size(); ++i) { needed[i] = mResources[i].output; }
		for (size_t pi = mPasses.size(); pi-- > 0;)
		{
			Pass& p = mPasses[pi];
			bool alive = p.sideEffect;
			for (const Use& u : p.uses)
			{
				if (IsWrite(u.access) && needed[u.resource]) { alive = true; }
			}
			p.culled = !alive;
			if (!alive) { continue; }
			for (const Use& u : p.uses)
			{
				if (IsWrite(u.access) && u.overwrite) { needed[u.resource] = false; }
			}
			for (const Use& u : p.uses)
			{
				if (!IsWrite(u.access) || !u.overwrite) { needed[u.resource] = true; }
			}
		}

		// 2. 途中でだけ使うテクスチャの、最初と最後に使うパス
		constexpr uint32_t kNone = UINT32_MAX;
		std::vector<uint32_t> first(mResources.size(), kNone), last(mResources.size(), 0);
		for (uint32_t pi = 0; pi < mPasses.size(); ++pi)
		{
			if (mPasses[pi].culled) { continue; }
			for (const Use& u : mPasses[pi].uses)
			{
				if (first[u.resource] == kNone) { first[u.resource] = pi; }
				last[u.resource] = pi;
			}
		}

		// 3. 使い始めの早い順に、作り方が同じで使い終わった実体があればそれを使う
		std::vector<uint32_t> order;
		for (uint32_t i = 0; i < mResources.size(); ++i)
		{
			mResources[i].physical = -1;
			if (!mResources[i].isImported && first[i] != kNone) { order.push_back(i); }
		}
		std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return first[a] < first[b]; });
		mPhysicalDescs.clear();
		std::vector<uint32_t> physicalLast;
		for (uint32_t r : order)
		{
			int slot = -1;
			for (size_t s = 0; s < mPhysicalDescs.size(); ++s)
			{
				if (physicalLast[s] < first[r] && mPhysicalDescs[s].SameAs(mResources[r].desc)) { slot = int(s); break; }
			}
			if (slot < 0)
			{
				mPhysicalDescs.push_back(mResources[r].desc);
				physicalLast.push_back(0);
				slot = int(mPhysicalDescs.size() - 1);
			}
			physicalLast[slot] = last[r];
			mResources[r].physical = slot;
		}
		mCompiled = true;
	}

	std::vector<std::pair<TextureHandle, D3D12_RESOURCE_STATES>> RenderGraph::GetPassStates(uint32_t pass) const
	{
		std::vector<std::pair<TextureHandle, D3D12_RESOURCE_STATES>> states;
		for (const Use& u : mPasses[pass].uses)
		{
			auto it = std::find_if(states.begin(), states.end(), [&](auto& s) { return s.first.index == u.resource; });
			D3D12_RESOURCE_STATES state = ToState(u.access);
			if (it == states.end()) { states.push_back({ TextureHandle{ u.resource }, state }); }
			// 深度を比べながらシェーダーでも読むときは、両方の状態を合わせる
			else if (it->second != state) { it->second = D3D12_RESOURCE_STATES(it->second | state); }
		}
		return states;
	}

	GraphInfo RenderGraph::GetInfo() const
	{
		GraphInfo info;
		for (const Pass& p : mPasses)
		{
			GraphInfo::Pass gp;
			gp.name = p.name;
			gp.culled = p.culled;
			gp.sideEffect = p.sideEffect;
			for (const Use& u : p.uses) { gp.uses.push_back({ u.resource, u.access, u.overwrite }); }
			info.passes.push_back(std::move(gp));
		}
		for (const Resource& r : mResources)
		{
			GraphInfo::Resource gr;
			gr.name = r.name;
			gr.imported = r.isImported;
			gr.output = r.output;
			gr.depth = r.desc.depth;
			gr.width = r.desc.width;
			gr.height = r.desc.height;
			gr.format = r.desc.depth ? DepthTexture::kDsvFormat : r.desc.format;
			gr.physical = r.physical;
			info.resources.push_back(std::move(gr));
		}
		info.physicalCount = uint32_t(mPhysicalDescs.size());
		return info;
	}

	// ---------------------------------------------------------------- Execute

	RenderGraph::PoolEntry& RenderGraph::Acquire(const TextureDesc& desc, std::vector<bool>& taken)
	{
		for (size_t i = 0; i < mPool.size(); ++i)
		{
			if (!taken[i] && mPool[i]->desc.SameAs(desc))
			{
				taken[i] = true;
				return *mPool[i];
			}
		}
		auto entry = std::make_unique<PoolEntry>();
		entry->desc = desc;
		entry->name = L"RenderGraph Texture";
		if (desc.depth)
		{
			entry->depth = std::make_unique<DepthTexture>();
			entry->depth->Create(desc.width, desc.height, entry->name.c_str());
		}
		else
		{
			entry->color = std::make_unique<RenderTexture>();
			entry->color->Create({ desc.width, desc.height, desc.format, desc.rtvFormat, desc.srvFormat, desc.clearColor, entry->name.c_str() });
		}
		mPool.push_back(std::move(entry));
		taken.push_back(true);
		return *mPool.back();
	}

	void RenderGraph::Execute(ID3D12GraphicsCommandList* cmd)
	{
		RD_ASSERT(mCompiled, "Render graph: call Compile before Execute");
		++mExecuteCount;

		// 実体を割り当てる（同じ描画の中で 2 つの番号が同じ実体を取らないように）
		std::vector<bool> taken(mPool.size(), false);
		std::vector<PoolEntry*> physical(mPhysicalDescs.size(), nullptr);
		for (size_t s = 0; s < mPhysicalDescs.size(); ++s)
		{
			physical[s] = &Acquire(mPhysicalDescs[s], taken);
			physical[s]->lastUsed = mExecuteCount;
		}
		for (size_t i = 0; i < mResources.size(); ++i)
		{
			Resource& r = mResources[i];
			if (r.isImported) { r.texture = r.imported; }
			else if (r.physical >= 0)
			{
				PoolEntry& e = *physical[r.physical];
				r.texture = &e.Get();
#if RD_DEBUG
				// PIX などで見える名前を、今使っているものの名前にする（変わったときだけ）
				if (e.debugName != r.name)
				{
					e.debugName = r.name;
					SetDebugName(e.Get().GetResource(), ToWide(r.name).c_str());
				}
#endif
			}
			else { r.texture = nullptr; }
		}

		PassContext ctx(*this);
		ctx.cmd = cmd;
		for (uint32_t pi = 0; pi < mPasses.size(); ++pi)
		{
			Pass& p = mPasses[pi];
			if (p.culled) { continue; }
			for (auto& [h, state] : GetPassStates(pi))
			{
				if (GpuTexture* tex = mResources[h.index].texture) { tex->Transition(cmd, state); }
			}
			GpuScope scope(p.name);
			if (p.execute) { p.execute(ctx); }
		}

		for (Resource& r : mResources)
		{
			if (r.isImported && r.texture && r.texture->GetResource()) { r.texture->Transition(cmd, r.finalState); }
			r.texture = nullptr;
		}

		// しばらく使っていない実体を捨てる（GPU が使い終わるまでは DeferRelease が持っている）
		std::erase_if(mPool, [&](const std::unique_ptr<PoolEntry>& e) { return e->lastUsed + kEvictAfter < mExecuteCount; });
	}

	GpuTexture& RenderGraph::Resolve(TextureHandle h) const
	{
		RD_ASSERT(h.IsValid() && h.index < mResources.size() && mResources[h.index].texture,
			"Render graph: texture '{}' is not available in this pass", h.IsValid() && h.index < mResources.size() ? mResources[h.index].name : "?");
		return *mResources[h.index].texture;
	}
}
