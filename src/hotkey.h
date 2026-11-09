#pragma once
#include "common.h"

extern HHOOK g_hKeyboardHook;

// Low-level keyboard hook callback to intercept PrintScreen and Shift+PrintScreen
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
