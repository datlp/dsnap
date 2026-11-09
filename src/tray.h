#pragma once
#include "common.h"

#define TRAY_TIP_DEFAULT L"dsnap\n• Capture: PrtScn\n• Record: Shift + PrtScn"

extern NOTIFYICONDATAW g_nid;

// Display the system tray context menu
void ShowContextMenu(HWND hWnd);

// Update tooltip text on the system tray icon
void UpdateTrayTip(const wchar_t* tip);

// Window procedure for the hidden background management window
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
