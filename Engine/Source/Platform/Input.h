#pragma once
#include "Math/Vector.h"
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

	// XInput のボタン
	enum class GamepadButton : uint16_t
	{
		DPadUp = 0x0001, DPadDown = 0x0002, DPadLeft = 0x0004, DPadRight = 0x0008,
		Start = 0x0010, Back = 0x0020, LeftThumb = 0x0040, RightThumb = 0x0080,
		LeftShoulder = 0x0100, RightShoulder = 0x0200, A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000,
	};

	// 入力管理（Win32 メッセージ + Raw Input + XInput）
	class Input
	{
	public:
		static constexpr uint32_t kMaxGamepads = 4;

		static void Initialize(HWND hwnd);
		// フレーム先頭、メッセージ処理の前に呼ぶ（前フレームの状態を保存）
		static void NewFrame();
		// メッセージ処理の後に呼ぶ（このフレームのマウス移動量を確定）
		static void LatchFrame();
		static void ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		static void ResetState();

		// エディタの UI が入力を使っているときにゲーム側の入力を止める
		static void SetBlocked(bool keyboard, bool mouse) { sBlockKeyboard = keyboard; sBlockMouse = mouse; }

		// キーボード
		static bool GetKey(Key key);
		static bool GetKeyDown(Key key);
		static bool GetKeyUp(Key key);

		// マウス
		static bool GetMouseButton(MouseButton button);
		static bool GetMouseButtonDown(MouseButton button);
		static bool GetMouseButtonUp(MouseButton button);
		static Vector2 GetMousePosition() { return sMousePos; }// クライアント座標
		static Vector2 GetMouseDelta() { return sBlockMouse ? Vector2::kZero : sMouseDelta; }
		static float GetMouseWheel() { return sBlockMouse ? 0.0f : sWheel; }

		// ゲームパッド
		static bool IsGamepadConnected(uint32_t index = 0);
		static bool GetGamepadButton(GamepadButton button, uint32_t index = 0);
		static bool GetGamepadButtonDown(GamepadButton button, uint32_t index = 0);
		static Vector2 GetLeftStick(uint32_t index = 0);
		static Vector2 GetRightStick(uint32_t index = 0);
		static float GetLeftTrigger(uint32_t index = 0);
		static float GetRightTrigger(uint32_t index = 0);
		static void SetVibration(float left, float right, uint32_t index = 0);

		// ブロックを無視した生の状態（エディタ用）
		static bool GetKeyRaw(Key key) { return sKeys[uint8_t(key)]; }
		static bool GetKeyDownRaw(Key key) { return sKeys[uint8_t(key)] && !sPrevKeys[uint8_t(key)]; }
		static bool GetMouseButtonRaw(MouseButton b) { return sMouse[uint8_t(b)]; }
		static Vector2 GetMouseDeltaRaw() { return sMouseDelta; }
		static float GetMouseWheelRaw() { return sWheel; }

	private:
		static inline bool sKeys[256] = {};
		static inline bool sPrevKeys[256] = {};
		static inline bool sMouse[uint8_t(MouseButton::Count)] = {};
		static inline bool sPrevMouse[uint8_t(MouseButton::Count)] = {};
		static inline Vector2 sMousePos;
		static inline Vector2 sMouseDelta;
		static inline Vector2 sMouseDeltaAccum;
		static inline float sWheel = 0.0f;
		static inline float sWheelAccum = 0.0f;
		static inline bool sBlockKeyboard = false;
		static inline bool sBlockMouse = false;
	};
}
