#include "hotkey.h"
#include "overlay.h"

HHOOK g_hKeyboardHook = NULL;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (pKey->vkCode == VK_SNAPSHOT) {
                // Dismiss open popup menus so they freeze into the snapshot without blocking input
                EndMenu();

                bool isShiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

                if (g_isOverlayActive) {
                    return 1;
                }

                if (isShiftPressed) {
                    PostMessageW(g_hMainWnd, WM_DSNAP_TRIGGER, TRIGGER_VIDEO, 0);
                } else {
                    PostMessageW(g_hMainWnd, WM_DSNAP_TRIGGER, TRIGGER_PHOTO, 0);
                }
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}
