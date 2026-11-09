#include "common.h"

HINSTANCE g_hInstance   = NULL;
HWND      g_hMainWnd    = NULL;
HICON     g_hAppIcon    = NULL;
Mode      g_currentMode = MODE_NONE;

void EnableDpiAwareness() {
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetProcessDpiAwarenessContextProc)(HANDLE);
        SetProcessDpiAwarenessContextProc setCtx = 
            (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (setCtx) {
            setCtx((HANDLE)-4); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            return;
        }
    }
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        typedef HRESULT(WINAPI* SetProcessDpiAwarenessProc)(int);
        SetProcessDpiAwarenessProc setDpi = 
            (SetProcessDpiAwarenessProc)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (setDpi) {
            setDpi(2); // PROCESS_PER_MONITOR_DPI_AWARE
        }
        FreeLibrary(hShcore);
    }
}

void ExcludeFromCapture(HWND hWnd) {
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetWindowDisplayAffinityProc)(HWND, DWORD);
        SetWindowDisplayAffinityProc pfn = 
            (SetWindowDisplayAffinityProc)GetProcAddress(hUser32, "SetWindowDisplayAffinity");
        if (pfn) {
            pfn(hWnd, 0x00000011); // WDA_EXCLUDEFROMCAPTURE
        }
    }
}
