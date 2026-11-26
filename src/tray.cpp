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

enum class MenuIconType {
    Camera,
    Record,
    Screenshots,
    Recordings,
    Folder,
    Trash,
    Audio,
    Language,
    Exit
};

static HBITMAP CreateMenuIconBitmap(MenuIconType type) {
    using namespace Gdiplus;
    Bitmap bmp(16, 16, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));

    switch (type) {
    case MenuIconType::Camera: {
        SolidBrush bodyBrush(Color(255, 14, 165, 233));
        g.FillRectangle(&bodyBrush, 2.0f, 5.0f, 12.0f, 8.0f);
        g.FillRectangle(&bodyBrush, 5.5f, 3.0f, 5.0f, 2.5f);
        SolidBrush lensBg(Color(255, 255, 255, 255));
        g.FillEllipse(&lensBg, 5.5f, 6.5f, 5.0f, 5.0f);
        SolidBrush lensCore(Color(255, 3, 105, 161));
        g.FillEllipse(&lensCore, 6.7f, 7.7f, 2.6f, 2.6f);
        break;
    }
    case MenuIconType::Record: {
        Pen ringPen(Color(255, 239, 68, 68), 1.6f);
        g.DrawEllipse(&ringPen, 2.0f, 2.0f, 12.0f, 12.0f);
        SolidBrush dotBrush(Color(255, 239, 68, 68));
        g.FillEllipse(&dotBrush, 4.8f, 4.8f, 6.4f, 6.4f);
        break;
    }
    case MenuIconType::Screenshots: {
        Pen framePen(Color(255, 2, 132, 199), 1.5f);
        g.DrawRectangle(&framePen, 2.0f, 3.0f, 12.0f, 10.0f);
        SolidBrush sunBrush(Color(255, 245, 158, 11));
        g.FillEllipse(&sunBrush, 9.5f, 4.8f, 2.4f, 2.4f);
        SolidBrush hillBrush(Color(255, 56, 189, 248));
        PointF pts[3] = { PointF(3.0f, 12.0f), PointF(6.8f, 7.5f), PointF(11.0f, 12.0f) };
        g.FillPolygon(&hillBrush, pts, 3);
        break;
    }
    case MenuIconType::Recordings: {
        SolidBrush boxBrush(Color(255, 139, 92, 246));
        g.FillRectangle(&boxBrush, 2.0f, 3.0f, 12.0f, 10.0f);
        SolidBrush playBrush(Color(255, 255, 255, 255));
        PointF pts[3] = { PointF(6.5f, 5.2f), PointF(6.5f, 10.8f), PointF(11.2f, 8.0f) };
        g.FillPolygon(&playBrush, pts, 3);
        break;
    }
    case MenuIconType::Folder: {
        SolidBrush tabBrush(Color(255, 217, 119, 6));
        g.FillRectangle(&tabBrush, 1.5f, 3.0f, 5.5f, 3.0f);
        SolidBrush folderBrush(Color(255, 245, 158, 11));
        g.FillRectangle(&folderBrush, 1.5f, 4.8f, 13.0f, 8.2f);
        break;
    }
    case MenuIconType::Trash: {
        Pen trashPen(Color(255, 220, 38, 38), 1.5f);
        g.DrawLine(&trashPen, 3.0f, 4.2f, 13.0f, 4.2f);
        g.DrawLine(&trashPen, 6.2f, 2.4f, 9.8f, 2.4f);
        g.DrawRectangle(&trashPen, 4.2f, 4.5f, 7.6f, 8.8f);
        g.DrawLine(&trashPen, 6.8f, 6.5f, 6.8f, 11.2f);
        g.DrawLine(&trashPen, 9.2f, 6.5f, 9.2f, 11.2f);
        break;
    }
    case MenuIconType::Audio: {
        SolidBrush spkBrush(Color(255, 16, 185, 129));
        PointF pts[4] = { PointF(2.0f, 6.0f), PointF(5.0f, 6.0f), PointF(8.5f, 3.0f), PointF(8.5f, 13.0f) };
        PointF pts2[5] = { PointF(2.0f, 6.0f), PointF(5.0f, 6.0f), PointF(8.5f, 3.0f), PointF(8.5f, 13.0f), PointF(5.0f, 10.0f) };
        (void)pts;
        g.FillPolygon(&spkBrush, pts2, 5);
        g.FillRectangle(&spkBrush, 2.0f, 6.0f, 3.5f, 4.0f);
        Pen wavePen(Color(255, 16, 185, 129), 1.4f);
        g.DrawArc(&wavePen, 7.5f, 4.5f, 4.5f, 7.0f, -55.0f, 110.0f);
        g.DrawArc(&wavePen, 9.0f, 2.8f, 5.5f, 10.4f, -55.0f, 110.0f);
        break;
    }
    case MenuIconType::Language: {
        Pen globePen(Color(255, 20, 184, 166), 1.3f);
        g.DrawEllipse(&globePen, 2.0f, 2.0f, 12.0f, 12.0f);
        g.DrawEllipse(&globePen, 5.2f, 2.0f, 5.6f, 12.0f);
        g.DrawLine(&globePen, 2.2f, 8.0f, 13.8f, 8.0f);
        break;
    }
    case MenuIconType::Exit: {
        Pen exitPen(Color(255, 239, 68, 68), 1.6f);
        g.DrawArc(&exitPen, 2.8f, 3.2f, 10.4f, 10.4f, -50.0f, 280.0f);
        g.DrawLine(&exitPen, 8.0f, 1.8f, 8.0f, 7.5f);
        break;
    }
    }

    HBITMAP hBmp = NULL;
    bmp.GetHBITMAP(Color(0, 0, 0, 0), &hBmp);
    return hBmp;
}

static void SetMenuItemIcon(HMENU hMenu, UINT item, BOOL fByPosition, MenuIconType icon, std::vector<HBITMAP>& bitmaps) {
    HBITMAP hBmp = CreateMenuIconBitmap(icon);
    if (!hBmp) return;
    bitmaps.push_back(hBmp);
    MENUITEMINFOW mii = { 0 };
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_BITMAP;
    mii.hbmpItem = hBmp;
    SetMenuItemInfoW(hMenu, item, fByPosition, &mii);
}

static void ClearFolderWithConfirm(HWND hWnd, const std::wstring& dir, const wchar_t* pattern) {
    int count = CountMatchingFiles(dir, pattern);
    if (count <= 0) {
        MessageBoxW(hWnd, Tr(StringId::ClearEmptyBody), Tr(StringId::ClearConfirmTitle), MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t msg[256] = { 0 };
    swprintf_s(msg, Tr(StringId::ClearConfirmBody), count);
    if (MessageBoxW(hWnd, msg, Tr(StringId::ClearConfirmTitle), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
        ClearMatchingFiles(dir, pattern);
    }
}

void UpdateTrayTip(const wchar_t* tip) {
    wcsncpy_s(g_nid.szTip, tip, _countof(g_nid.szTip) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void ShowContextMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    std::vector<HBITMAP> menuBitmaps;

    AppendMenuW(hMenu, MF_STRING, IDM_CAPTURE_PHOTO, Tr(StringId::MenuCapturePhoto));
    SetMenuItemIcon(hMenu, IDM_CAPTURE_PHOTO, FALSE, MenuIconType::Camera, menuBitmaps);

    AppendMenuW(hMenu, MF_STRING, IDM_RECORD_VIDEO, Tr(StringId::MenuRecordVideo));
    SetMenuItemIcon(hMenu, IDM_RECORD_VIDEO, FALSE, MenuIconType::Record, menuBitmaps);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Screenshots Submenu
    HMENU hShotsMenu = CreatePopupMenu();
    AppendMenuW(hShotsMenu, MF_STRING, IDM_OPEN_SCREENSHOTS, Tr(StringId::MenuOpenScreenshots));
    SetMenuItemIcon(hShotsMenu, IDM_OPEN_SCREENSHOTS, FALSE, MenuIconType::Folder, menuBitmaps);

    AppendMenuW(hShotsMenu, MF_STRING, IDM_CLEAR_SCREENSHOTS, Tr(StringId::MenuClearScreenshots));
    SetMenuItemIcon(hShotsMenu, IDM_CLEAR_SCREENSHOTS, FALSE, MenuIconType::Trash, menuBitmaps);

    AppendMenuW(hShotsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hShotsMenu, MF_STRING | (g_copyScreenshotImage ? MF_CHECKED : MF_UNCHECKED),
                IDM_COPY_SHOT_IMAGE, Tr(StringId::MenuAutoCopyImage));
    AppendMenuW(hShotsMenu, MF_STRING | (g_copyScreenshotPath ? MF_CHECKED : MF_UNCHECKED),
                IDM_COPY_SHOT_PATH, Tr(StringId::MenuAutoCopyPath));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hShotsMenu, Tr(StringId::MenuScreenshots));
    SetMenuItemIcon(hMenu, GetMenuItemCount(hMenu) - 1, TRUE, MenuIconType::Screenshots, menuBitmaps);

    // Recordings Submenu
    HMENU hRecsMenu = CreatePopupMenu();
    AppendMenuW(hRecsMenu, MF_STRING, IDM_OPEN_CAPTURES, Tr(StringId::MenuOpenCaptures));
    SetMenuItemIcon(hRecsMenu, IDM_OPEN_CAPTURES, FALSE, MenuIconType::Folder, menuBitmaps);

    AppendMenuW(hRecsMenu, MF_STRING, IDM_CLEAR_CAPTURES, Tr(StringId::MenuClearCaptures));
    SetMenuItemIcon(hRecsMenu, IDM_CLEAR_CAPTURES, FALSE, MenuIconType::Trash, menuBitmaps);

    AppendMenuW(hRecsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hRecsMenu, MF_STRING | (g_copyRecordingPath ? MF_CHECKED : MF_UNCHECKED),
                IDM_COPY_REC_PATH, Tr(StringId::MenuAutoCopyPath));

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hRecsMenu, Tr(StringId::MenuRecordings));
    SetMenuItemIcon(hMenu, GetMenuItemCount(hMenu) - 1, TRUE, MenuIconType::Recordings, menuBitmaps);

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
    SetMenuItemIcon(hMenu, GetMenuItemCount(hMenu) - 1, TRUE, MenuIconType::Audio, menuBitmaps);
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
    SetMenuItemIcon(hMenu, GetMenuItemCount(hMenu) - 1, TRUE, MenuIconType::Language, menuBitmaps);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, Tr(StringId::MenuExit));
    SetMenuItemIcon(hMenu, IDM_EXIT, FALSE, MenuIconType::Exit, menuBitmaps);

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
    for (HBITMAP hBmp : menuBitmaps) {
        DeleteObject(hBmp);
    }
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
        case IDM_CLEAR_SCREENSHOTS:
            ClearFolderWithConfirm(hWnd, GetScreenshotsDir(), L"dsnap-screenshot-*.png");
            break;
        case IDM_CLEAR_CAPTURES:
            ClearFolderWithConfirm(hWnd, GetCapturesDir(), L"dsnap-recording-*.mp4");
            break;
        case IDM_COPY_SHOT_IMAGE:
            g_copyScreenshotImage = !g_copyScreenshotImage;
            SaveClipboardSetting(L"CopyScreenshotImage", g_copyScreenshotImage);
            break;
        case IDM_COPY_SHOT_PATH:
            g_copyScreenshotPath = !g_copyScreenshotPath;
            SaveClipboardSetting(L"CopyScreenshotPath", g_copyScreenshotPath);
            break;
        case IDM_COPY_REC_PATH:
            g_copyRecordingPath = !g_copyRecordingPath;
            SaveClipboardSetting(L"CopyRecordingPath", g_copyRecordingPath);
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
