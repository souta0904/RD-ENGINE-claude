#include "RdPch.h"
#include "FrameComposer.h"
#include "Components/CameraComponent.h"
#include "Platform/Window.h"
#include "Render/Renderer.h"
#include "RHI/GraphicsDevice.h"
#include "Runtime/FrameOverlay.h"
#include "Scene/Scene.h"

namespace rd
{
	void FrameComposer::Initialize(Window& window, GraphicsDevice& device, Renderer& renderer)
	{
		mWindow = &window;
		mDevice = &device;
		mRenderer = &renderer;
	}

	void FrameComposer::Render(Scene& scene, float realTime)
	{
		// カメラを決める（割り込み層のカメラ → シーンのメインカメラ → 既定位置）
		uint32_t viewW = mWindow->GetWidth(), viewH = mWindow->GetHeight();
		if (mOverlay) { mOverlay->GetViewportSize(viewW, viewH); }
		float aspect = float(std::max(1u, viewW)) / float(std::max(1u, viewH));
		RenderCamera camera;
		bool overridden = mOverlay && mOverlay->OverrideCamera(aspect, camera);
		if (!overridden)
		{
			if (CameraComponent* mainCam = CameraComponent::FindMain(scene))
			{
				camera = mainCam->GetRenderCamera(aspect);
			}
			else
			{
				camera = RenderCamera::Make({ 0.0f, 3.0f, -10.0f }, Quaternion::FromEuler({ math::ToRadians(10.0f), 0.0f, 0.0f }),
					math::ToRadians(60.0f), aspect, 0.1f, 1000.0f);
			}
		}

		// 描画
		auto& renderer = *mRenderer;
		{
			RD_PROFILE_SCOPE("Collect Render");
			scene.CollectRender(renderer.GetRenderList());
			if (mOverlay) { mOverlay->DrawGizmos(renderer.GetDebugDraw()); }
		}
		renderer.Render(camera, scene.GetEnvironment(), viewW, viewH, realTime);

		ComposeBackBuffer();
	}

	void FrameComposer::ComposeBackBuffer()
	{
		RD_GPU_SCOPE(mOverlay ? "Editor UI" : "Blit");
		auto& device = *mDevice;
		auto cmd = device.GetCommandList();
		TransitionBarrier(cmd, device.GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (mOverlay)
		{
			// 割り込み層の UI（ゲーム画面はその中にテクスチャとして描かれる）
			auto rtv = device.GetBackBufferRtv();
			cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
			float clear[4] = { 0.08f, 0.08f, 0.09f, 1.0f };
			cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);
			SetViewport(cmd, device.GetWidth(), device.GetHeight());
			mOverlay->RenderUI();
		}
		else
		{
			mRenderer->BlitToBackBuffer();
		}
		TransitionBarrier(cmd, device.GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}
}
