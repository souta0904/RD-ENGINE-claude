#include "RdPch.h"
#include "Editor/Core/ImGuiLayer.h"
#include "Editor/Style/EditorStyle.h"
#include "RHI/Device/GraphicsDevice.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx12.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/ImGuizmo.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace rd
{
	namespace
	{
		DescriptorHandle gFontSrv;
	}

	void ImGuiLayer::Initialize(HWND hwnd)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
		io.IniFilename = "Logs/imgui.ini";

		float dpiScale = float(GetDpiForWindow(hwnd)) / 96.0f;
		LoadFonts(15.0f * dpiScale);
		EditorStyle::Apply(dpiScale);

		auto& device = GraphicsDevice::Get();
		gFontSrv = device.GetSrvHeap().Allocate();
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(device.GetDevice(), int(kFrameCount), GraphicsDevice::kBackBufferFormat,
			device.GetSrvHeap().Get(), gFontSrv.cpu, gFontSrv.gpu);
		mInitialized = true;
	}

	// 英数字は Roboto、日本語はメイリオを合わせる（メイリオは再配布できないので Windows のフォントフォルダから読む）
	void ImGuiLayer::LoadFonts(float size)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (!std::filesystem::exists("Assets/Engine/Fonts/Roboto-Regular.ttf"))
		{
			io.Fonts->AddFontDefault();
			return;
		}
		io.Fonts->AddFontFromFileTTF("Assets/Engine/Fonts/Roboto-Regular.ttf", size);
		wchar_t windir[MAX_PATH] = {};
		GetWindowsDirectoryW(windir, MAX_PATH);
		std::filesystem::path japanese = std::filesystem::path(windir) / L"Fonts" / L"meiryo.ttc";
		if (std::filesystem::exists(japanese))
		{
			ImFontConfig config;
			config.MergeMode = true;
			io.Fonts->AddFontFromFileTTF(japanese.string().c_str(), size, &config, io.Fonts->GetGlyphRangesJapanese());
		}
	}

	void ImGuiLayer::Shutdown()
	{
		if (!mInitialized) { return; }
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		GraphicsDevice::Get().GetSrvHeap().Free(gFontSrv);
		mInitialized = false;
	}

	void ImGuiLayer::BeginFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::Render()
	{
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GraphicsDevice::Get().GetCommandList());
	}

	// メッセージは Input でも使いたいので、いつも false（処理済みにしない）を返す
	bool ImGuiLayer::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (!mInitialized) { return false; }
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
		return false;
	}
}
