#include "tray.h"
#include "config.h"
#include "capture.h"
#include "overlay.h"
#include "recorder.h"

NOTIFYICONDATAW g_nid = { 0 };

void UpdateTrayTip(const wchar_t* tip) {
    wcsncpy_s(g_nid.szTip, tip, _countof(g_nid.szTip) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void ShowContextMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, IDM_CAPTURE_PHOTO, L"📸 Capture Screen (PrintScreen)");
    AppendMenuW(hMenu, MF_STRING, IDM_RECORD_VIDEO, L"🎥 Record Screen (Shift + PrtScn)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_SCREENSHOTS, L"📁 Open Screenshots Folder");
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_CAPTURES, L"📁 Open Recordings Folder");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    UINT audioFlag = g_recordAudio ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | audioFlag, IDM_TOGGLE_AUDIO, L"🎙️ Record System Audio");

    UINT autostartFlag = IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | autostartFlag, IDM_AUTOSTART, L"Start with Windows");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static UINT s_uTaskbarRestartMsg = RegisterWindowMessageW(L"TaskbarCreated");
    if (uMsg == s_uTaskbarRestartMsg) {
        Shell_NotifyIconW(NIM_ADD, &g_nid);
        return 0;
    }

    switch (uMsg) {
    case WM_CREATE: {
        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = hWnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_hAppIcon ? g_hAppIcon : LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(g_nid.szTip, TRAY_TIP_DEFAULT);
        wcscpy_s(g_nid.szInfoTitle, L"dsnap is running!");
        wcscpy_s(g_nid.szInfo, L"dsnap is ready in system tray!\nPress PrtScn to capture, Shift+PrtScn to record.");
        g_nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_ADD, &g_nid);
        return 0;
    }
    case WM_TRAYICON: {
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            ShowContextMenu(hWnd);
        }
        return 0;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDM_CAPTURE_PHOTO:
            PostMessageW(hWnd, WM_DSNAP_TRIGGER, TRIGGER_PHOTO, 0);
            break;
        case IDM_RECORD_VIDEO:
            PostMessageW(hWnd, WM_DSNAP_TRIGGER, TRIGGER_VIDEO, 0);
            break;
        case IDM_OPEN_SCREENSHOTS:
            ShellExecuteW(NULL, L"open", GetScreenshotsDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case IDM_OPEN_CAPTURES:
            ShellExecuteW(NULL, L"open", GetCapturesDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case IDM_TOGGLE_AUDIO:
            g_recordAudio = !g_recordAudio;
            SaveAudioSetting(g_recordAudio);
            break;
        case IDM_SHOW_RUNNING_TIP: {
            NOTIFYICONDATAW nidTip = g_nid;
            nidTip.uFlags = NIF_INFO;
            wcscpy_s(nidTip.szInfoTitle, L"dsnap");
            wcscpy_s(nidTip.szInfo, L"dsnap is already running in the system tray!\nPress PrtScn to capture, Shift+PrtScn to record.");
            nidTip.dwInfoFlags = NIIF_INFO;
            Shell_NotifyIconW(NIM_MODIFY, &nidTip);
            break;
        }
        case IDM_AUTOSTART:
            SetAutoStart(!IsAutoStartEnabled());
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        return 0;
    }
    case WM_DSNAP_TRIGGER: {
        if (wParam == TRIGGER_PHOTO) {
            TriggerOverlay(MODE_PHOTO);
        } else if (wParam == TRIGGER_VIDEO) {
            if (g_isRecording) {
                StopRecording(false);
            } else {
                if (!EnsureFFmpegAvailable(hWnd)) {
                    break;
                }
                TriggerOverlay(MODE_VIDEO);
            }
        }
        return 0;
    }
    case WM_DESTROY: {
        if (g_isRecording) StopRecording(false);
        if (g_isOverlayActive && g_hWndOverlay) {
            DestroyWindow(g_hWndOverlay);
        }
        CleanupFrozenBitmaps();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
