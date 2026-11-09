#pragma once
#include "common.h"

extern bool g_isOverlayActive;
extern HWND g_hWndOverlay;

void TriggerOverlay(Mode mode);
LRESULT CALLBACK OverlayProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
