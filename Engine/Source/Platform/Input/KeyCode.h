#pragma once
#include <Windows.h>
#include <cstdint>

namespace rd
{
	// キーコード（値は Win32 の仮想キーコード）
	enum class Key : uint8_t
	{
		Backspace = VK_BACK, Tab = VK_TAB, Enter = VK_RETURN, Shift = VK_SHIFT, Control = VK_CONTROL,
		Alt = VK_MENU, Escape = VK_ESCAPE, Space = VK_SPACE, PageUp = VK_PRIOR, PageDown = VK_NEXT,
		End = VK_END, Home = VK_HOME, Left = VK_LEFT, Up = VK_UP, Right = VK_RIGHT, Down = VK_DOWN,
		Insert = VK_INSERT, Delete = VK_DELETE,
		D0 = '0', D1, D2, D3, D4, D5, D6, D7, D8, D9,
		A = 'A', B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		F1 = VK_F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		LeftShift = VK_LSHIFT, RightShift = VK_RSHIFT, LeftControl = VK_LCONTROL, RightControl = VK_RCONTROL,
	};

	enum class MouseButton : uint8_t { Left, Right, Middle, X1, X2, Count };

	// XInput のボタン（値は XINPUT_GAMEPAD_* と同じ）
	enum class GamepadButton : uint16_t
	{
		DPadUp = 0x0001, DPadDown = 0x0002, DPadLeft = 0x0004, DPadRight = 0x0008,
		Start = 0x0010, Back = 0x0020, LeftThumb = 0x0040, RightThumb = 0x0080,
		LeftShoulder = 0x0100, RightShoulder = 0x0200, A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000,
	};
}
