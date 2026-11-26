#include "RdPch.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Renderer/RendererCommon.h"
#include "RHI/Device/GpuScope.h"

namespace rd
{
	using namespace render;

	// カスケードシャドウの分割と行列を決める
	//  - 分割位置は等間隔と対数の間（lambda で混ぜる実用分割法）
	//  - 各区間の視錐台の 8 頂点を包む球で範囲を決める（カメラが回っても大きさが変わらないので影がちらつかない）
	//  - 中心をテクセル単位にそろえる（カメラが動いても影の輪郭が揺れない）
	void Renderer::SetupShadowCascades(const RenderCamera& camera, const Vector3& lightDir, GpuFrameData& f)
	{
		const uint32_t count = mSettings.shadowCascades;
		const float nearZ = camera.nearZ;
		const float farZ = std::max(nearZ + 0.1f, std::min(mSettings.shadowDistance, camera.farZ));
		const float lambda = math::Clamp(mSettings.cascadeSplitLambda, 0.0f, 1.0f);
		// 射影行列の m[0][0] = 1/(aspect*tan), m[1][1] = 1/tan
		const float tanHalfY = 1.0f / camera.proj.m[1][1];
		const float tanHalfX = 1.0f / camera.proj.m[0][0];
		const Vector3 forward = camera.GetForward(), right = camera.GetRight(), up = camera.GetUp();

		Vector3 lightUp = std::fabs(lightDir.y) > 0.99f ? Vector3::kUnitZ : Vector3::kUnitY;
		Matrix4 lightView = Matrix4::CreateLookAt(Vector3::kZero, lightDir, lightUp);

		float prevSplit = nearZ;
		for (uint32_t i = 0; i < count; ++i)
		{
			float t = float(i + 1) / float(count);
			float logSplit = nearZ * std::pow(farZ / nearZ, t);
			float uniSplit = nearZ + (farZ - nearZ) * t;
			float split = math::Lerp(uniSplit, logSplit, lambda);

			Vector3 corners[8];
			int n = 0;
			for (float d : { prevSplit, split })
			{
				for (float sy : { -1.0f, 1.0f })
				{
					for (float sx : { -1.0f, 1.0f })
					{
						corners[n++] = camera.position + forward * d + right * (sx * d * tanHalfX) + up * (sy * d * tanHalfY);
					}
				}
			}
			Vector3 center;
			for (auto& c : corners) { center += c; }
			center /= 8.0f;
			float radius = 0.0f;
			for (auto& c : corners) { radius = std::max(radius, Distance(c, center)); }
			radius = std::ceil(radius * 16.0f) / 16.0f;

			float texel = radius * 2.0f / float(mSettings.shadowMapSize);
			Vector3 c = TransformPoint(center, lightView);
			c.x = std::floor(c.x / texel) * texel;
			c.y = std::floor(c.y / texel) * texel;
			// 光の手前側は広めに取る（範囲の外にある高い物の影も落とせるように）
			constexpr float kCasterMargin = 100.0f;
			Matrix4 lightProj = Matrix4::CreateOrthographicOffCenter(c.x - radius, c.x + radius, c.y - radius, c.y + radius,
				c.z - radius - kCasterMargin, c.z + radius);
			f.shadowViewProj[i] = lightView * lightProj;
			(&f.cascadeSplits.x)[i] = split;
			(&f.cascadeTexelSizes.x)[i] = texel;
			mCascadeFrustums[i] = Frustum::FromViewProj(f.shadowViewProj[i]);
			prevSplit = split;
		}
		f.cascadeCount = count;
		f.shadowFadeStart = farZ * 0.9f;
		f.shadowEnabled = 1;
		f.debugCascades = mSettings.debugCascades ? 1u : 0u;
	}

	// i 番目のカスケードは i 番目のタイル（左上・右上・左下・右下）に描き、どの行列を使うかを定数で伝える
	void Renderer::ShadowPass(ID3D12GraphicsCommandList* cmd, DepthTexture& shadowMap, const CasterLists& casters)
	{
		auto& device = GraphicsDevice::Get();
		auto dsv = shadowMap.GetDsv();
		cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
		shadowMap.Clear(cmd);
		cmd->SetGraphicsRootSignature(mMeshRS.Get());
		cmd->SetGraphicsRootConstantBufferView(kRootLights, mLightCB);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const float tile = float(mSettings.shadowMapSize);
		for (uint32_t i = 0; i < mFrameData.cascadeCount; ++i)
		{
			D3D12_VIEWPORT vp{ float(i % 2) * tile, float(i / 2) * tile, tile, tile, 0.0f, 1.0f };
			D3D12_RECT scissor{ LONG(vp.TopLeftX), LONG(vp.TopLeftY), LONG(vp.TopLeftX + tile), LONG(vp.TopLeftY + tile) };
			cmd->RSSetViewports(1, &vp);
			cmd->RSSetScissorRects(1, &scissor);
			GpuFrameData frame = mFrameData;
			frame.shadowCascade = i;
			cmd->SetGraphicsRootConstantBufferView(kRootFrame, device.UploadConstants(frame));
			DrawMeshes(cmd, casters[i], MeshPass::Shadow);
		}
	}
}
