#include "RdPch.h"
#include "Platform/Input.h"
#include <windowsx.h>
#include <Xinput.h>

namespace rd
{
	namespace
	{
		XINPUT_STATE gPads[Input::kMaxGamepads] = {};
		XINPUT_STATE gPrevPads[Input::kMaxGamepads] = {};
		bool gPadConnected[Input::kMaxGamepads] = {};

		Vector2 Stick(SHORT x, SHORT y, SHORT deadZone)
		{
			Vector2 v(float(x) / 32767.0f, float(y) / 32767.0f);
			float len = Length(v);
			float dz = float(deadZone) / 32767.0f;
			if (len < dz) { return Vector2::kZero; }
			// デッドゾーンの外側を 0～1 に再マップ
			float scaled = std::min(1.0f, (len - dz) / (1.0f - dz));
			return v / len * scaled;
		}
	}

	void Input::Initialize(HWND hwnd)
	{
		// マウスの Raw Input（カメラ操作用の相対移動量）
		RAWINPUTDEVICE rid = {};
		rid.usUsagePage = 0x01;// Generic desktop
		rid.usUsage = 0x02;// Mouse
		rid.hwndTarget = hwnd;
		RegisterRawInputDevices(&rid, 1, sizeof(rid));
		ResetState();
	}

	void Input::NewFrame()
	{
		std::memcpy(sPrevKeys, sKeys, sizeof(sKeys));
		std::memcpy(sPrevMouse, sMouse, sizeof(sMouse));

		for (DWORD i = 0; i < kMaxGamepads; ++i)
		{
			gPrevPads[i] = gPads[i];
			// 未接続のパッドへの問い合わせは重いので、接続済みのものと 0 番だけ毎フレーム調べる
			if (gPadConnected[i] || i == 0)
			{
				gPadConnected[i] = XInputGetState(i, &gPads[i]) == ERROR_SUCCESS;
				if (!gPadConnected[i]) { gPads[i] = {}; }
			}
		}
	}

	void Input::LatchFrame()
	{
		sMouseDelta = sMouseDeltaAccum;
		sMouseDeltaAccum = Vector2::kZero;
		sWheel = sWheelAccum;
		sWheelAccum = 0.0f;
	}

	void Input::ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (wParam < 256)
			{
				sKeys[wParam] = true;
				// 左右の区別
				if (wParam == VK_SHIFT) { sKeys[GetKeyState(VK_LSHIFT) & 0x8000 ? VK_LSHIFT : VK_RSHIFT] = true; }
				if (wParam == VK_CONTROL) { sKeys[(lParam & (1 << 24)) ? VK_RCONTROL : VK_LCONTROL] = true; }
			}
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (wParam < 256)
			{
				sKeys[wParam] = false;
				if (wParam == VK_SHIFT) { sKeys[VK_LSHIFT] = sKeys[VK_RSHIFT] = false; }
				if (wParam == VK_CONTROL) { sKeys[VK_LCONTROL] = sKeys[VK_RCONTROL] = false; }
			}
			break;
		case WM_LBUTTONDOWN: sMouse[uint8_t(MouseButton::Left)] = true; SetCapture(hwnd); break;
		case WM_LBUTTONUP: sMouse[uint8_t(MouseButton::Left)] = false; ReleaseCapture(); break;
		case WM_RBUTTONDOWN: sMouse[uint8_t(MouseButton::Right)] = true; SetCapture(hwnd); break;
		case WM_RBUTTONUP: sMouse[uint8_t(MouseButton::Right)] = false; ReleaseCapture(); break;
		case WM_MBUTTONDOWN: sMouse[uint8_t(MouseButton::Middle)] = true; SetCapture(hwnd); break;
		case WM_MBUTTONUP: sMouse[uint8_t(MouseButton::Middle)] = false; ReleaseCapture(); break;
		case WM_XBUTTONDOWN:
			sMouse[uint8_t(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MouseButton::X1 : MouseButton::X2)] = true;
			break;
		case WM_XBUTTONUP:
			sMouse[uint8_t(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MouseButton::X1 : MouseButton::X2)] = false;
			break;
		case WM_MOUSEMOVE:
			sMousePos = Vector2(float(GET_X_LPARAM(lParam)), float(GET_Y_LPARAM(lParam)));
			break;
		case WM_MOUSEWHEEL:
			sWheelAccum += float(GET_WHEEL_DELTA_WPARAM(wParam)) / float(WHEEL_DELTA);
			break;
		case WM_INPUT:
		{
			RAWINPUT raw = {};
			UINT size = sizeof(raw);
			if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != UINT(-1) &&
				raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
			{
				sMouseDeltaAccum += Vector2(float(raw.data.mouse.lLastX), float(raw.data.mouse.lLastY));
			}
			break;
		}
		}
	}

	void Input::ResetState()
	{
		std::memset(sKeys, 0, sizeof(sKeys));
		std::memset(sMouse, 0, sizeof(sMouse));
		sMouseDeltaAccum = Vector2::kZero;
		sWheelAccum = 0.0f;
	}

	bool Input::GetKey(Key key) { return !sBlockKeyboard && sKeys[uint8_t(key)]; }
	bool Input::GetKeyDown(Key key) { return !sBlockKeyboard && sKeys[uint8_t(key)] && !sPrevKeys[uint8_t(key)]; }
	bool Input::GetKeyUp(Key key) { return !sBlockKeyboard && !sKeys[uint8_t(key)] && sPrevKeys[uint8_t(key)]; }

	bool Input::GetMouseButton(MouseButton b) { return !sBlockMouse && sMouse[uint8_t(b)]; }
	bool Input::GetMouseButtonDown(MouseButton b) { return !sBlockMouse && sMouse[uint8_t(b)] && !sPrevMouse[uint8_t(b)]; }
	bool Input::GetMouseButtonUp(MouseButton b) { return !sBlockMouse && !sMouse[uint8_t(b)] && sPrevMouse[uint8_t(b)]; }

	bool Input::IsGamepadConnected(uint32_t index) { return index < kMaxGamepads && gPadConnected[index]; }

	bool Input::GetGamepadButton(GamepadButton button, uint32_t index)
	{
		return IsGamepadConnected(index) && (gPads[index].Gamepad.wButtons & uint16_t(button));
	}

	bool Input::GetGamepadButtonDown(GamepadButton button, uint32_t index)
	{
		return IsGamepadConnected(index) &&
			(gPads[index].Gamepad.wButtons & uint16_t(button)) && !(gPrevPads[index].Gamepad.wButtons & uint16_t(button));
	}

	Vector2 Input::GetLeftStick(uint32_t index)
	{
		if (!IsGamepadConnected(index)) { return Vector2::kZero; }
		const auto& g = gPads[index].Gamepad;
		return Stick(g.sThumbLX, g.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	}

	Vector2 Input::GetRightStick(uint32_t index)
	{
		if (!IsGamepadConnected(index)) { return Vector2::kZero; }
		const auto& g = gPads[index].Gamepad;
		return Stick(g.sThumbRX, g.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
	}

	float Input::GetLeftTrigger(uint32_t index)
	{
		return IsGamepadConnected(index) ? float(gPads[index].Gamepad.bLeftTrigger) / 255.0f : 0.0f;
	}

	float Input::GetRightTrigger(uint32_t index)
	{
		return IsGamepadConnected(index) ? float(gPads[index].Gamepad.bRightTrigger) / 255.0f : 0.0f;
	}

	void Input::SetVibration(float left, float right, uint32_t index)
	{
		if (!IsGamepadConnected(index)) { return; }
		XINPUT_VIBRATION vib = { WORD(math::Saturate(left) * 65535.0f), WORD(math::Saturate(right) * 65535.0f) };
		XInputSetState(index, &vib);
	}
}
