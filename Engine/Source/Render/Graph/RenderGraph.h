#pragma once
#include "Math/Color.h"
#include "RHI/D3D12Common.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class GpuTexture;
	class RenderTexture;
	class DepthTexture;
}

// レンダーグラフ
//  1 回の描画で行うパスを「何を読んで何に書くか」と一緒に登録してから、まとめて実行する。
//   - 順番は登録した順。リソースの状態の切り替え（バリア）はグラフが入れる
//   - 結果が最後の出力に届かないパスは飛ばす（Cull）
//   - 途中でだけ使うテクスチャ（Transient）は、使う期間が重ならなければ同じ実体を使い回す。
//     実体はグラフが持っていて、次の描画でも使い回す（しばらく使われなければ捨てる）
//  使い方:
//    graph.Reset();
//    auto out = graph.Import("Output", output, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
//    auto hdr = graph.Create("HDR", { width, height, kHdrFormat });
//    graph.AddPass("Main", [&](rg::PassBuilder& b) { b.WriteNew(hdr); }, [=](rg::PassContext& c) { ... c.Color(hdr) ... });
//    graph.AddPass("Tonemap", [&](rg::PassBuilder& b) { b.Read(hdr); b.WriteNew(out); }, ...);
//    graph.Compile();
//    graph.Execute(cmd);
namespace rd::rg
{
	// 途中でだけ使うテクスチャの作り方（同じものどうしは実体を使い回せる）
	//  rtv / srv が UNKNOWN なら format と同じ。depth なら深度バッファ（format は見ない）
	struct TextureDesc
	{
		uint32_t width = 1;
		uint32_t height = 1;
		DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN;
		DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
		bool depth = false;
		Color clearColor = Color(0, 0, 0, 0);

		bool SameAs(const TextureDesc& o) const;
	};

	struct TextureHandle
	{
		static constexpr uint32_t kInvalid = UINT32_MAX;
		uint32_t index = kInvalid;
		bool IsValid() const { return index != kInvalid; }
		bool operator==(const TextureHandle&) const = default;
	};

	// パスがテクスチャをどう使うか
	enum class Access : uint8_t
	{
		ShaderRead,   // シェーダーで読む
		RenderTarget, // 色を書く
		DepthWrite,   // 深度を書く
		DepthRead,    // 深度を比べるだけ（書かない）
		CopySource,   // コピー元
	};

	D3D12_RESOURCE_STATES ToState(Access access);
	const char* ToString(Access access);

	class RenderGraph;

	// パスを登録するときに、読むもの・書くものを伝える
	class RD_API PassBuilder
	{
	public:
		// シェーダーで読む
		TextureHandle Read(TextureHandle h) { return Use(h, Access::ShaderRead, false); }
		// 深度を比べるだけで書かない（シェーダーでも読むなら Read も呼ぶ）
		TextureHandle ReadDepth(TextureHandle h) { return Use(h, Access::DepthRead, false); }
		// 今の中身の上に描く
		TextureHandle Write(TextureHandle h) { return Use(h, Access::RenderTarget, false); }
		// 中身をすべて書き直す（消してから描く。前のパスの結果は要らない）
		TextureHandle WriteNew(TextureHandle h) { return Use(h, Access::RenderTarget, true); }
		TextureHandle WriteDepth(TextureHandle h, bool clear = false) { return Use(h, Access::DepthWrite, clear); }
		TextureHandle CopyFrom(TextureHandle h) { return Use(h, Access::CopySource, false); }
		// 出力に届かなくても飛ばさない（スクリーンショットの読み戻しなど）
		void SideEffect();

	private:
		friend class RenderGraph;
		PassBuilder(RenderGraph& graph, uint32_t pass) : mGraph(graph), mPass(pass) {}
		TextureHandle Use(TextureHandle h, Access access, bool overwrite);
		RenderGraph& mGraph;
		uint32_t mPass;
	};

	// パスを実行するときに渡すもの（ハンドルから実際のテクスチャを取り出す）
	class RD_API PassContext
	{
	public:
		ID3D12GraphicsCommandList* cmd = nullptr;
		RenderTexture& Color(TextureHandle h) const;
		DepthTexture& Depth(TextureHandle h) const;
		GpuTexture& Texture(TextureHandle h) const;

	private:
		friend class RenderGraph;
		explicit PassContext(RenderGraph& graph) : mGraph(graph) {}
		RenderGraph& mGraph;
	};

	using SetupFn = std::function<void(PassBuilder&)>;
	using ExecuteFn = std::function<void(PassContext&)>;

	// エディタで見せるための、組み立てた結果の写し
	struct GraphInfo
	{
		struct Use
		{
			uint32_t resource;
			Access access;
			bool overwrite;
		};
		struct Pass
		{
			std::string name;
			bool culled = false;
			bool sideEffect = false;
			std::vector<Use> uses;
		};
		struct Resource
		{
			std::string name;
			bool imported = false;
			bool output = false;
			bool depth = false;
			uint32_t width = 0;
			uint32_t height = 0;
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
			// 実体の番号（使い回すものは同じ番号。読み込んだものと使われないものは -1）
			int physical = -1;
		};
		std::vector<Pass> passes;
		std::vector<Resource> resources;
		uint32_t physicalCount = 0;
	};

	class RD_API RenderGraph
	{
	public:
		RenderGraph();
		~RenderGraph();
		RenderGraph(const RenderGraph&) = delete;
		RenderGraph& operator=(const RenderGraph&) = delete;

		// 前の描画で登録したものを消す（テクスチャの実体は残して使い回す）
		void Reset();

		// 途中でだけ使うテクスチャ
		TextureHandle Create(const char* name, const TextureDesc& desc);
		// 外で持っているテクスチャ。最後に finalState にする。output なら、これに届くパスを残す
		//  texture を nullptr にすると GPU なしで組み立てだけ試せる（テスト用）
		TextureHandle Import(const char* name, GpuTexture* texture, D3D12_RESOURCE_STATES finalState, bool output = false, bool depth = false);

		// name は GPU の計測にそのまま渡すので、文字列リテラルにする
		void AddPass(const char* name, const SetupFn& setup, ExecuteFn execute);

		// 飛ばすパス、テクスチャの使い回し、状態の切り替えを決める（GPU は使わない）
		void Compile();
		// 実体を用意して、パスを順に実行する
		void Execute(ID3D12GraphicsCommandList* cmd);

		// ---- Compile の結果（テストとエディタ用）
		uint32_t GetPassCount() const { return uint32_t(mPasses.size()); }
		bool IsCulled(uint32_t pass) const { return mPasses[pass].culled; }
		// 使い回した結果の実体の番号（読み込んだものと、どのパスも使わないものは -1）
		int GetPhysical(TextureHandle h) const { return mResources[h.index].physical; }
		uint32_t GetPhysicalCount() const { return uint32_t(mPhysicalDescs.size()); }
		// パスの前に、テクスチャをどの状態にするか
		std::vector<std::pair<TextureHandle, D3D12_RESOURCE_STATES>> GetPassStates(uint32_t pass) const;
		GraphInfo GetInfo() const;
		// 持っている実体の数（使い回しの確認用）
		uint32_t GetPooledCount() const { return uint32_t(mPool.size()); }

	private:
		friend class PassBuilder;
		friend class PassContext;

		struct Use
		{
			uint32_t resource;
			Access access;
			bool overwrite;
		};
		struct Pass
		{
			const char* name;
			std::vector<Use> uses;
			ExecuteFn execute;
			bool sideEffect = false;
			bool culled = false;
		};
		struct Resource
		{
			std::string name;
			TextureDesc desc;
			GpuTexture* imported = nullptr;
			bool isImported = false;
			bool output = false;
			D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_COMMON;
			int physical = -1;
			GpuTexture* texture = nullptr; // Execute の間だけ有効
		};
		struct PoolEntry;

		GpuTexture& Resolve(TextureHandle h) const;
		PoolEntry& Acquire(const TextureDesc& desc, std::vector<bool>& taken);

		std::vector<Pass> mPasses;
		std::vector<Resource> mResources;
		std::vector<TextureDesc> mPhysicalDescs;
		std::vector<std::unique_ptr<PoolEntry>> mPool;
		uint64_t mExecuteCount = 0;
		bool mCompiled = false;
	};
}
