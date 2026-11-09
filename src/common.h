#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <initguid.h>
#include <knownfolders.h>
#include <gdiplus.h>
#include <dshow.h>
#include <string>
#include <ctime>
#include <vector>
#include <algorithm>

#include "../res/resource.h"

// Define Known Folder GUIDs if missing from MinGW headers
#ifndef FOLDERID_Captures
DEFINE_GUID(FOLDERID_Captures, 0xedc0fe99, 0xaa4b, 0x4770, 0x81, 0xa2, 0x05, 0x6e, 0xe3, 0xde, 0xc1, 0xab);
#endif

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "strmiids.lib")

#define WM_TRAYICON           (WM_USER + 1)
#define WM_DSNAP_TRIGGER      (WM_USER + 100)

#define TRIGGER_PHOTO         1
#define TRIGGER_VIDEO         2

#define TIMER_RECORDING       101

constexpr wchar_t APP_NAME[]      = L"dsnap";
constexpr wchar_t REG_KEY[]       = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t BG_CLASS[]      = L"DsnapBackground";
constexpr wchar_t OVERLAY_CLASS[] = L"DsnapOverlay";
constexpr wchar_t BORDER_CLASS[]  = L"DsnapRecordingBorder";
constexpr wchar_t TOOLBAR_CLASS[] = L"DsnapRecordingToolbar";

// Global Instance & Window handles
extern HINSTANCE g_hInstance;
extern HWND      g_hMainWnd;
extern HICON     g_hAppIcon;

// Mode
enum Mode { MODE_NONE, MODE_PHOTO, MODE_VIDEO };
extern Mode g_currentMode;

// DPI Awareness & Window capture exclusion helpers
void EnableDpiAwareness();
void ExcludeFromCapture(HWND hWnd);
