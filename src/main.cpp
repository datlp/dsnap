#include "common.h"
#include "config.h"
#include "tray.h"
#include "hotkey.h"
#include "overlay.h"
#include "recorder.h"
#include "capture.h"
#include "click_effect.h"
#include "i18n.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // 0. Strict Single-Instance Lifecycle Check
    HWND hExistingWnd = FindWindowW(BG_CLASS, L"dsnap_bg");
    if (hExistingWnd) {
        PostMessageW(hExistingWnd, WM_COMMAND, MAKEWPARAM(IDM_SHOW_RUNNING_TIP, 0), 0);
        return 0;
    }

    SetLastError(0);
    HANDLE hSingleInstanceMutex = CreateMutexW(NULL, TRUE, L"Local\\Dsnap_SingleInstance_Mutex_9999");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hSingleInstanceMutex) CloseHandle(hSingleInstanceMutex);
        return 0;
    }

    g_hInstance = hInstance;

    // 1. Initialize COM & Per-Monitor DPI Awareness
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    EnableDpiAwareness();
    g_recordAudio = LoadAudioSetting();

    // 2. Load NTDLL function pointers for process suspension (Pause / Resume recording)
    InitRecorderNTDLL();

    // 3. Load Embedded Application Icon
    g_hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));

    // 4. Initialize GDI+
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Initialize Localization and Click Effect overlay
    InitLanguage();
    InitClickEffect(hInstance);

    // 5. Register Window Classes
    WNDCLASSW overlayWc = { 0 };
    overlayWc.lpfnWndProc = OverlayProc;
    overlayWc.hInstance = hInstance;
    overlayWc.lpszClassName = OVERLAY_CLASS;
    overlayWc.hCursor = LoadCursor(NULL, IDC_CROSS);
    overlayWc.hbrBackground = NULL;
    RegisterClassW(&overlayWc);

    WNDCLASSW borderWc = { 0 };
    borderWc.lpfnWndProc = RecordingBorderProc;
    borderWc.hInstance = hInstance;
    borderWc.lpszClassName = BORDER_CLASS;
    borderWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    borderWc.hbrBackground = NULL;
    RegisterClassW(&borderWc);

    WNDCLASSW toolbarWc = { 0 };
    toolbarWc.lpfnWndProc = RecordingToolbarProc;
    toolbarWc.hInstance = hInstance;
    toolbarWc.lpszClassName = TOOLBAR_CLASS;
    toolbarWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    toolbarWc.hbrBackground = NULL;
    RegisterClassW(&toolbarWc);

    WNDCLASSW bgWc = { 0 };
    bgWc.lpfnWndProc = MainWndProc;
    bgWc.hInstance = hInstance;
    bgWc.lpszClassName = BG_CLASS;
    bgWc.hIcon = g_hAppIcon;
    RegisterClassW(&bgWc);

    // Create hidden background host window
    HWND hWnd = CreateWindowExW(0, BG_CLASS, L"dsnap_bg", WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    g_hMainWnd = hWnd;

    // 6. Initialize Hybrid Hotkey Engine (RegisterHotKey + Low-Level Hook fallback)
    InitHotkeys(hWnd);

    // 7. Centralized Win32 Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 8. Clean up system resources
    ShutdownClickEffect();
    CleanupHotkeys(hWnd);
    CleanupFrozenBitmaps();
    UnregisterClassW(OVERLAY_CLASS, hInstance);
    UnregisterClassW(BORDER_CLASS, hInstance);
    UnregisterClassW(TOOLBAR_CLASS, hInstance);
    UnregisterClassW(BG_CLASS, hInstance);
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();

    if (hSingleInstanceMutex) {
        ReleaseMutex(hSingleInstanceMutex);
        CloseHandle(hSingleInstanceMutex);
    }
    return 0;
}
