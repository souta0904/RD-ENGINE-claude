#include "RdPch.h"
#include "Editor/ImGuiLayer.h"
#include "RHI/GraphicsDevice.h"
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

		// フォント（英数字は Roboto、日本語はメイリオを合成）
		float dpiScale = float(GetDpiForWindow(hwnd)) / 96.0f;
		float size = 15.0f * dpiScale;
		if (std::filesystem::exists("Assets/Engine/Fonts/Roboto-Regular.ttf"))
		{
			io.Fonts->AddFontFromFileTTF("Assets/Engine/Fonts/Roboto-Regular.ttf", size);
			// メイリオは再配布できないので Windows のフォントフォルダから読む
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
		else
		{
			io.Fonts->AddFontDefault();
		}

		SetupStyle();
		ImGui::GetStyle().ScaleAllSizes(dpiScale);

		auto& device = GraphicsDevice::Get();
		gFontSrv = device.GetSrvHeap().Allocate();
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(device.GetDevice(), int(kFrameCount), GraphicsDevice::kBackBufferFormat,
			device.GetSrvHeap().Get(), gFontSrv.cpu, gFontSrv.gpu);
		mInitialized = true;
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

	bool ImGuiLayer::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (!mInitialized) { return false; }
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
		// メッセージ自体は Input でも処理したいので常に false を返す
		return false;
	}

	void ImGuiLayer::SetupStyle()
	{
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 4.0f;
		style.FrameRounding = 3.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 3.0f;
		style.WindowBorderSize = 1.0f;
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(6.0f, 5.0f);

		ImVec4* c = style.Colors;
		const ImVec4 accent(0.26f, 0.55f, 0.95f, 1.0f);
		c[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
		c[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
		c[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.14f, 0.98f);
		c[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
		c[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
		c[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.27f, 1.0f);
		c[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.32f, 1.0f);
		c[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.0f);
		c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
		c[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.0f);
		c[ImGuiCol_Header] = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
		c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.30f, 0.40f, 1.0f);
		c[ImGuiCol_HeaderActive] = accent;
		c[ImGuiCol_Button] = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.27f, 0.30f, 0.37f, 1.0f);
		c[ImGuiCol_ButtonActive] = accent;
		c[ImGuiCol_CheckMark] = accent;
		c[ImGuiCol_SliderGrab] = accent;
		c[ImGuiCol_Tab] = ImVec4(0.13f, 0.13f, 0.15f, 1.0f);
		c[ImGuiCol_TabHovered] = ImVec4(0.27f, 0.30f, 0.37f, 1.0f);
		c[ImGuiCol_TabActive] = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
		c[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
		c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.17f, 0.20f, 1.0f);
		c[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.55f, 0.95f, 0.5f);
	}
}
