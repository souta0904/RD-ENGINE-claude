#pragma once
#include "Math/Vector2.h"
#include "Platform/Input/KeyCode.h"

namespace rd
{
	// 入力（Win32 のメッセージ・Raw Input のマウス移動量・XInput のゲームパッド）
	//  毎フレーム NewFrame → メッセージ処理 → LatchFrame の順に呼ぶ
	//  エディタの UI が使っている間は SetBlocked でゲーム側の入力を止める（Raw の関数は止めない）
	class Input
	{
	public:
		static constexpr uint32_t kMaxGamepads = 4;

		static void Initialize(HWND hwnd);
		static void NewFrame();
		static void LatchFrame();
		static void ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		static void ResetState();

		static void SetBlocked(bool keyboard, bool mouse) { sBlockKeyboard = keyboard; sBlockMouse = mouse; }

		static bool GetKey(Key key);
		static bool GetKeyDown(Key key);
		static bool GetKeyUp(Key key);

		static bool GetMouseButton(MouseButton button);
		static bool GetMouseButtonDown(MouseButton button);
		static bool GetMouseButtonUp(MouseButton button);
		static Vector2 GetMousePosition() { return sMousePos; }
		static Vector2 GetMouseDelta() { return sBlockMouse ? Vector2::kZero : sMouseDelta; }
		static float GetMouseWheel() { return sBlockMouse ? 0.0f : sWheel; }

		// カーソルを隠してウィンドウ内に閉じ込める（FPS・TPS のカメラ操作用）
		//  マウス入力が止められている間やウィンドウが前面でない間は外れ、戻ると自動でかかり直す
		static void SetCursorLocked(bool locked) { sCursorLocked = locked; }
		static bool IsCursorLocked() { return sCursorLocked && sCursorApplied; }

		static bool IsGamepadConnected(uint32_t index = 0);
		static bool GetGamepadButton(GamepadButton button, uint32_t index = 0);
		static bool GetGamepadButtonDown(GamepadButton button, uint32_t index = 0);
		static Vector2 GetLeftStick(uint32_t index = 0);
		static Vector2 GetRightStick(uint32_t index = 0);
		static float GetLeftTrigger(uint32_t index = 0);
		static float GetRightTrigger(uint32_t index = 0);
		static void SetVibration(float left, float right, uint32_t index = 0);

		static bool GetKeyRaw(Key key) { return sKeys[uint8_t(key)]; }
		static bool GetKeyDownRaw(Key key) { return sKeys[uint8_t(key)] && !sPrevKeys[uint8_t(key)]; }
		static bool GetMouseButtonRaw(MouseButton b) { return sMouse[uint8_t(b)]; }
		static Vector2 GetMouseDeltaRaw() { return sMouseDelta; }
		static float GetMouseWheelRaw() { return sWheel; }

	private:
		static void UpdateCursorLock();
		static void ApplyCursorLock(bool lock);

		static inline HWND sHwnd = nullptr;
		static inline bool sCursorLocked = false;
		static inline bool sCursorApplied = false;
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
