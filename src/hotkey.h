#pragma once
#include "common.h"

constexpr int HOTKEY_ID_PHOTO = 1001;
constexpr int HOTKEY_ID_VIDEO = 1002;

extern HHOOK g_hKeyboardHook;
extern bool g_isRegisterHotKeyActive;

// Initialize Hybrid Hotkey Engine (RegisterHotKey + Low-Level Hook fallback)
void InitHotkeys(HWND hWnd);
void CleanupHotkeys(HWND hWnd);

// Low-level keyboard hook callback to intercept PrintScreen and Shift+PrintScreen
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

// Helper to trigger actions with de-duplication guard
void TriggerActionDebounced(int triggerMode);
