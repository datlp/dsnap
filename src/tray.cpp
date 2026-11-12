#include "tray.h"
#include "config.h"
#include "capture.h"
#include "overlay.h"
#include "recorder.h"
#include "click_effect.h"
#include "i18n.h"
#include "hotkey.h"
#include "audio.h"

NOTIFYICONDATAW g_nid = { 0 };
static std::vector<AudioDeviceInfo> s_cachedOutputDevices;
static std::vector<AudioDeviceInfo> s_cachedInputDevices;

void UpdateTrayTip(const wchar_t* tip) {
    wcsncpy_s(g_nid.szTip, tip, _countof(g_nid.szTip) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void ShowContextMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, IDM_CAPTURE_PHOTO, Tr(StringId::MenuCapturePhoto));
    AppendMenuW(hMenu, MF_STRING, IDM_RECORD_VIDEO, Tr(StringId::MenuRecordVideo));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_SCREENSHOTS, Tr(StringId::MenuOpenScreenshots));
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_CAPTURES, Tr(StringId::MenuOpenCaptures));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    UINT audioFlag = g_recordAudio ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | audioFlag, IDM_TOGGLE_AUDIO, Tr(StringId::MenuToggleAudio));

    UINT micFlag = g_recordMic ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | micFlag, IDM_TOGGLE_MIC, Tr(StringId::MenuToggleMic));

    // Audio Devices Submenu (Multi-Output Loopback + Multi-Microphone Selection)
    HMENU hAudioMenu = CreatePopupMenu();
    s_cachedOutputDevices = EnumAudioOutputDevices();
    s_cachedInputDevices = EnumAudioInputDevices();

    AppendMenuW(hAudioMenu, MF_STRING | MF_GRAYED, 0, Tr(StringId::MenuAudioOutHeader));
    UINT defOutCheck = g_selectedOutputDevices.empty() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hAudioMenu, MF_STRING | defOutCheck, IDM_AUDIO_OUT_DEFAULT, Tr(StringId::MenuAudioDefaultOut));
    if (s_cachedOutputDevices.empty()) {
        AppendMenuW(hAudioMenu, MF_STRING | MF_GRAYED, 0, Tr(StringId::MenuAudioNone));
    } else {
        size_t maxOut = (std::min)(s_cachedOutputDevices.size(), (size_t)(IDM_AUDIO_OUT_MAX - IDM_AUDIO_OUT_BASE + 1));
        for (size_t i = 0; i < maxOut; ++i) {
            UINT check = IsOutputDeviceSelected(s_cachedOutputDevices[i].id) ? MF_CHECKED : MF_UNCHECKED;
            std::wstring label = s_cachedOutputDevices[i].name;
            if (s_cachedOutputDevices[i].isDefault) label += L" *";
            AppendMenuW(hAudioMenu, MF_STRING | check, IDM_AUDIO_OUT_BASE + (UINT)i, label.c_str());
        }
    }

    AppendMenuW(hAudioMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAudioMenu, MF_STRING | MF_GRAYED, 0, Tr(StringId::MenuAudioInHeader));
    UINT defInCheck = g_selectedInputDevices.empty() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hAudioMenu, MF_STRING | defInCheck, IDM_AUDIO_IN_DEFAULT, Tr(StringId::MenuAudioDefaultIn));
    if (s_cachedInputDevices.empty()) {
        AppendMenuW(hAudioMenu, MF_STRING | MF_GRAYED, 0, Tr(StringId::MenuAudioNone));
    } else {
        size_t maxIn = (std::min)(s_cachedInputDevices.size(), (size_t)(IDM_AUDIO_IN_MAX - IDM_AUDIO_IN_BASE + 1));
        for (size_t i = 0; i < maxIn; ++i) {
            UINT check = IsInputDeviceSelected(s_cachedInputDevices[i].id) ? MF_CHECKED : MF_UNCHECKED;
            std::wstring label = s_cachedInputDevices[i].name;
            if (s_cachedInputDevices[i].isDefault) label += L" *";
            AppendMenuW(hAudioMenu, MF_STRING | check, IDM_AUDIO_IN_BASE + (UINT)i, label.c_str());
        }
    }

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAudioMenu, Tr(StringId::MenuAudioDevices));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    UINT clickFlag = g_highlightClicks ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | clickFlag, IDM_HIGHLIGHT_CLICKS, Tr(StringId::MenuHighlightClicks));

    UINT autostartFlag = IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | autostartFlag, IDM_AUTOSTART, Tr(StringId::MenuStartWithWindows));

    // Language Submenu
    HMENU hLangMenu = CreatePopupMenu();
    Language currentLang = GetCurrentLanguage();
    Language savedLang = LoadLanguageSetting();

    UINT autoCheck = (savedLang == Language::Auto) ? MF_CHECKED : MF_UNCHECKED;
    UINT enCheck = (savedLang == Language::English) ? MF_CHECKED : MF_UNCHECKED;
    UINT viCheck = (savedLang == Language::Vietnamese) ? MF_CHECKED : MF_UNCHECKED;
    UINT zhCheck = (savedLang == Language::Chinese) ? MF_CHECKED : MF_UNCHECKED;
    UINT koCheck = (savedLang == Language::Korean) ? MF_CHECKED : MF_UNCHECKED;
    UINT jaCheck = (savedLang == Language::Japanese) ? MF_CHECKED : MF_UNCHECKED;

    AppendMenuW(hLangMenu, MF_STRING | autoCheck, IDM_LANG_AUTO, L"Auto (System Default)");
    AppendMenuW(hLangMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hLangMenu, MF_STRING | enCheck, IDM_LANG_EN, L"English");
    AppendMenuW(hLangMenu, MF_STRING | viCheck, IDM_LANG_VI, L"Tiếng Việt");
    AppendMenuW(hLangMenu, MF_STRING | zhCheck, IDM_LANG_ZH, L"简体中文");
    AppendMenuW(hLangMenu, MF_STRING | koCheck, IDM_LANG_KO, L"한국어");
    AppendMenuW(hLangMenu, MF_STRING | jaCheck, IDM_LANG_JA, L"日本語");

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hLangMenu, Tr(StringId::MenuLanguage));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, Tr(StringId::MenuExit));

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
        Language savedLang = LoadLanguageSetting();
        SetCurrentLanguage(savedLang);

        g_highlightClicks = LoadClickEffectSetting();
        EnableClickEffect(g_highlightClicks);

        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = hWnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_hAppIcon ? g_hAppIcon : LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(g_nid.szTip, Tr(StringId::TrayTipDefault));
        wcscpy_s(g_nid.szInfoTitle, Tr(StringId::NotificationRunningTitle));
        wcscpy_s(g_nid.szInfo, Tr(StringId::NotificationRunningBody));
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
        UINT cmd = LOWORD(wParam);
        if (cmd >= IDM_AUDIO_OUT_BASE && cmd <= IDM_AUDIO_OUT_MAX) {
            size_t idx = (size_t)(cmd - IDM_AUDIO_OUT_BASE);
            if (idx < s_cachedOutputDevices.size()) {
                ToggleOutputDeviceSelection(s_cachedOutputDevices[idx].id);
                if (!g_selectedOutputDevices.empty() && !g_recordAudio) {
                    g_recordAudio = true;
                    SaveAudioSetting(true);
                }
            }
            return 0;
        }
        if (cmd >= IDM_AUDIO_IN_BASE && cmd <= IDM_AUDIO_IN_MAX) {
            size_t idx = (size_t)(cmd - IDM_AUDIO_IN_BASE);
            if (idx < s_cachedInputDevices.size()) {
                ToggleInputDeviceSelection(s_cachedInputDevices[idx].id);
                if (!g_selectedInputDevices.empty() && !g_recordMic) {
                    g_recordMic = true;
                    SaveMicSetting(true);
                }
            }
            return 0;
        }

        switch (cmd) {
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
        case IDM_TOGGLE_MIC:
            g_recordMic = !g_recordMic;
            SaveMicSetting(g_recordMic);
            break;
        case IDM_AUDIO_OUT_DEFAULT:
            ClearOutputDeviceSelection();
            if (!g_recordAudio) {
                g_recordAudio = true;
                SaveAudioSetting(true);
            }
            break;
        case IDM_AUDIO_IN_DEFAULT:
            ClearInputDeviceSelection();
            if (!g_recordMic) {
                g_recordMic = true;
                SaveMicSetting(true);
            }
            break;
        case IDM_HIGHLIGHT_CLICKS:
            g_highlightClicks = !g_highlightClicks;
            SaveClickEffectSetting(g_highlightClicks);
            EnableClickEffect(g_highlightClicks);
            break;
        case IDM_LANG_AUTO:
            SaveLanguageSetting(Language::Auto);
            SetCurrentLanguage(Language::Auto);
            UpdateTrayTip(Tr(StringId::TrayTipDefault));
            break;
        case IDM_LANG_EN:
            SaveLanguageSetting(Language::English);
            SetCurrentLanguage(Language::English);
            UpdateTrayTip(Tr(StringId::TrayTipDefault));
            break;
        case IDM_LANG_VI:
            SaveLanguageSetting(Language::Vietnamese);
            SetCurrentLanguage(Language::Vietnamese);
            UpdateTrayTip(Tr(StringId::TrayTipDefault));
            break;
        case IDM_LANG_ZH:
            SaveLanguageSetting(Language::Chinese);
            SetCurrentLanguage(Language::Chinese);
            UpdateTrayTip(Tr(StringId::TrayTipDefault));
            break;
        case IDM_LANG_KO:
            SaveLanguageSetting(Language::Korean);
            SetCurrentLanguage(Language::Korean);
            UpdateTrayTip(Tr(StringId::TrayTipDefault));
            break;
        case IDM_LANG_JA:
            SaveLanguageSetting(Language::Japanese);
            SetCurrentLanguage(Language::Japanese);
            UpdateTrayTip(Tr(StringId::TrayTipDefault));
            break;
        case IDM_SHOW_RUNNING_TIP: {
            NOTIFYICONDATAW nidTip = g_nid;
            nidTip.uFlags = NIF_INFO;
            wcscpy_s(nidTip.szInfoTitle, Tr(StringId::NotificationRunningTitle));
            wcscpy_s(nidTip.szInfo, Tr(StringId::NotificationRunningBody));
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
    case WM_HOTKEY: {
        if (wParam == HOTKEY_ID_PHOTO) {
            TriggerActionDebounced(TRIGGER_PHOTO);
        } else if (wParam == HOTKEY_ID_VIDEO) {
            TriggerActionDebounced(TRIGGER_VIDEO);
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
        ShutdownClickEffect();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
