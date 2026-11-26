#pragma once
#include "Math/Geometry/AABB.h"
#include "Render/GpuData.h"
#include "Render/Material/Material.h"
#include "Render/RenderList/DrawItems.h"
#include "RHI/D3D12Common.h"
#include <unordered_map>
#include <vector>

namespace rd
{
	class RenderShared;
	class ShaderAsset;
	struct FrameContext;

	// メッシュ（MeshDrawItem）を描く道具。影・フォワード・G バッファーの機能が共有する
	//  パイプラインはマテリアルの組み合わせごとに初めて使うときに作って覚えておく
	class RD_API MeshDrawer
	{
	public:
		// どのパスで描くか（パイプラインの選び方が変わる）
		enum class Pass : uint8_t { Forward, Shadow, GBuffer };

		// 覚えたパイプラインを捨てる（起動時と、シェーダーを読み直したとき）
		void Reset(const RenderShared& shared);

		// 描く先とメッシュ用のルートシグネチャ（FrameContext::BindMeshPass）は呼ぶ側が置く
		void Draw(ID3D12GraphicsCommandList* cmd, const std::vector<const MeshDrawItem*>& items, Pass pass, FrameContext& frame);

	private:
		// custom は自作のシェーダー（nullptr なら標準の Mesh.hlsl）
		ID3D12PipelineState* GetPipeline(bool skinned, BlendMode blend, bool doubleSided, Pass pass, bool wireframe, const ShaderAsset* custom);
		void BindShaderProperties(ID3D12GraphicsCommandList* cmd, const Material& mat, const ShaderAsset& shader);
		// Lights Per Object が 0 より大きいとき、物に効く追加のライトを選ぶ
		static void SelectObjectLights(const AABB& bounds, GpuObjectData& obj, const FrameContext& frame);

		const RenderShared* mShared = nullptr;
		std::unordered_map<uint64_t, ComPtr<ID3D12PipelineState>> mPipelines;
		// 自作のシェーダーの値を詰める作業用（毎回確保しないように使い回す）
		std::vector<Vector4> mPropertyScratch;
	};
}
