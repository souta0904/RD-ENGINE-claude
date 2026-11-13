#include "RdPch.h"
#include "FrameComposer.h"
#include "Components/CameraComponent.h"
#include "Platform/Window.h"
#include "Render/Renderer.h"
#include "RHI/GraphicsDevice.h"
#include "Scene/Scene.h"
#if RD_EDITOR
#include "Editor/Editor.h"
#endif

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
		// カメラを決める（エディタカメラ → シーンのメインカメラ → 既定位置）
		uint32_t viewW = mWindow->GetWidth(), viewH = mWindow->GetHeight();
#if RD_EDITOR
		if (mEditor) { mEditor->GetViewportSize(viewW, viewH); }
#endif
		float aspect = float(std::max(1u, viewW)) / float(std::max(1u, viewH));
		RenderCamera camera;
#if RD_EDITOR
		if (mEditor && mEditor->UseEditorCamera())
		{
			camera = mEditor->GetEditorCamera(aspect);
		}
		else
#endif
		if (CameraComponent* mainCam = scene.GetMainCamera())
		{
			camera = mainCam->GetRenderCamera(aspect);
		}
		else
		{
			camera = RenderCamera::Make({ 0.0f, 3.0f, -10.0f }, Quaternion::FromEuler({ math::ToRadians(10.0f), 0.0f, 0.0f }),
				math::ToRadians(60.0f), aspect, 0.1f, 1000.0f);
		}

		// 描画
		auto& renderer = *mRenderer;
		scene.CollectRender(renderer.GetRenderList());
#if RD_EDITOR
		if (mEditor) { mEditor->DrawGizmos(renderer.GetDebugDraw()); }
#endif
		renderer.Render(camera, scene.GetEnvironment(), viewW, viewH, realTime);

		ComposeBackBuffer();
	}

	void FrameComposer::ComposeBackBuffer()
	{
		auto& device = *mDevice;
		auto cmd = device.GetCommandList();
		TransitionBarrier(cmd, device.GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
#if RD_EDITOR
		if (mEditor)
		{
			// エディタ UI（ゲーム画面はビューポート内にテクスチャとして描かれる）
			auto rtv = device.GetBackBufferRtv();
			cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
			float clear[4] = { 0.08f, 0.08f, 0.09f, 1.0f };
			cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);
			SetViewport(cmd, device.GetWidth(), device.GetHeight());
			mEditor->RenderUI();
		}
		else
#endif
		{
			mRenderer->BlitToBackBuffer();
		}
		TransitionBarrier(cmd, device.GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}
}
