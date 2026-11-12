#include "hotkey.h"
#include "overlay.h"

HHOOK g_hKeyboardHook = NULL;
bool g_isRegisterHotKeyActive = false;
static ULONGLONG s_lastTriggerTick = 0;

void TriggerActionDebounced(int triggerMode) {
    if (g_isOverlayActive) {
        return;
    }
    ULONGLONG now = GetTickCount64();
    if (now - s_lastTriggerTick < 250) {
        // Ignore duplicate triggers within 250ms (debounce between RegisterHotKey and Hook)
        return;
    }
    s_lastTriggerTick = now;
    PostMessageW(g_hMainWnd, WM_DSNAP_TRIGGER, triggerMode, 0);
}

void InitHotkeys(HWND hWnd) {
    // 1. Try kernel-level RegisterHotKey (works across UIPI on elevated admin windows)
    BOOL regPhoto = RegisterHotKey(hWnd, HOTKEY_ID_PHOTO, 0, VK_SNAPSHOT);
    BOOL regVideo = RegisterHotKey(hWnd, HOTKEY_ID_VIDEO, MOD_SHIFT, VK_SNAPSHOT);

    g_isRegisterHotKeyActive = (regPhoto || regVideo);

    // 2. Install Low-Level Keyboard Hook as universal fallback
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    if (!g_hKeyboardHook) {
        g_hKeyboardHook = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            LowLevelKeyboardProc,
            hInst ? hInst : GetModuleHandleW(NULL),
            0
        );
    }
}

void CleanupHotkeys(HWND hWnd) {
    if (g_hKeyboardHook) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }
    UnregisterHotKey(hWnd, HOTKEY_ID_PHOTO);
    UnregisterHotKey(hWnd, HOTKEY_ID_VIDEO);
    g_isRegisterHotKeyActive = false;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (pKey->vkCode == VK_SNAPSHOT) {
                bool isShiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                TriggerActionDebounced(isShiftPressed ? TRIGGER_VIDEO : TRIGGER_PHOTO);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}
