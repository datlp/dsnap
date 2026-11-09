#include "config.h"
#include "tray.h"
#include <urlmon.h>
#include <thread>
#include <atomic>

bool g_recordAudio = false;
bool g_highlightClicks = false;

static std::atomic<bool> g_isDownloadingFFmpeg(false);

bool IsFFmpegDownloading() {
    return g_isDownloadingFFmpeg.load();
}

bool IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        DWORD type = REG_SZ;
        LONG res = RegQueryValueExW(hKey, APP_NAME, NULL, &type, (LPBYTE)path, &size);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

void SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ, (LPBYTE)path, (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
}

bool LoadAudioSetting() {
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        return GetPrivateProfileIntW(L"Settings", L"RecordAudio", 0, ini.c_str()) != 0;
    }
    HKEY hKey;
    DWORD val = 0;
    DWORD size = sizeof(val);
    DWORD type = REG_DWORD;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"RecordAudio", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val != 0);
        }
        RegCloseKey(hKey);
    }
    return false;
}

void SaveAudioSetting(bool enable) {
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        WritePrivateProfileStringW(L"Settings", L"RecordAudio", enable ? L"1" : L"0", ini.c_str());
        return;
    }
    HKEY hKey;
    DWORD val = enable ? 1 : 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"RecordAudio", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

bool LoadClickEffectSetting() {
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        return GetPrivateProfileIntW(L"Settings", L"HighlightClicks", 0, ini.c_str()) != 0;
    }
    HKEY hKey;
    DWORD val = 0;
    DWORD size = sizeof(val);
    DWORD type = REG_DWORD;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"HighlightClicks", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val != 0);
        }
        RegCloseKey(hKey);
    }
    return false;
}

void SaveClickEffectSetting(bool enable) {
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        WritePrivateProfileStringW(L"Settings", L"HighlightClicks", enable ? L"1" : L"0", ini.c_str());
        return;
    }
    HKEY hKey;
    DWORD val = enable ? 1 : 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"HighlightClicks", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

Language LoadLanguageSetting() {
    wchar_t code[32] = { 0 };
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        GetPrivateProfileStringW(L"Settings", L"Language", L"Auto", code, 32, ini.c_str());
    } else {
        HKEY hKey;
        DWORD size = sizeof(code);
        DWORD type = REG_SZ;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExW(hKey, L"Language", NULL, &type, (LPBYTE)code, &size);
            RegCloseKey(hKey);
        }
    }
    return ParseLanguageCode(code);
}

void SaveLanguageSetting(Language lang) {
    std::wstring code = (lang == Language::Auto) ? L"Auto" : GetLanguageCode(lang);
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        WritePrivateProfileStringW(L"Settings", L"Language", code.c_str(), ini.c_str());
        return;
    }
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"Language", 0, REG_SZ, (const BYTE*)code.c_str(), (DWORD)((code.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

std::wstring GetScreenshotsDir() {
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        wchar_t customDir[MAX_PATH] = { 0 };
        GetPrivateProfileStringW(L"Paths", L"Screenshots", L"", customDir, MAX_PATH, ini.c_str());
        if (wcslen(customDir) > 0) {
            CreateDirectoryW(customDir, NULL);
            return std::wstring(customDir);
        }
        std::wstring localScreenshots = GetDsnapDirectory() + L"screenshots";
        DWORD attr = GetFileAttributesW(localScreenshots.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return localScreenshots;
        }
    }
    PWSTR pPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Screenshots, 0, NULL, &pPath))) {
        std::wstring path(pPath);
        CoTaskMemFree(pPath);
        return path;
    }
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &pPath))) {
        std::wstring path = std::wstring(pPath) + L"\\Screenshots";
        CoTaskMemFree(pPath);
        CreateDirectoryW(path.c_str(), NULL);
        return path;
    }
    return L".";
}

std::wstring GetCapturesDir() {
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        wchar_t customDir[MAX_PATH] = { 0 };
        GetPrivateProfileStringW(L"Paths", L"Captures", L"", customDir, MAX_PATH, ini.c_str());
        if (wcslen(customDir) > 0) {
            CreateDirectoryW(customDir, NULL);
            return std::wstring(customDir);
        }
        std::wstring localCaptures = GetDsnapDirectory() + L"captures";
        DWORD attr = GetFileAttributesW(localCaptures.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return localCaptures;
        }
    }
    PWSTR pPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Captures, 0, NULL, &pPath))) {
        std::wstring path(pPath);
        CoTaskMemFree(pPath);
        return path;
    }
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &pPath))) {
        std::wstring path = std::wstring(pPath) + L"\\Captures";
        CoTaskMemFree(pPath);
        CreateDirectoryW(path.c_str(), NULL);
        return path;
    }
    return L".";
}

std::wstring GetDsnapDirectory() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring path(exePath);
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(0, lastSlash + 1);
    }
    return L".\\";
}

bool IsPortableMode() {
    static int s_isPortable = -1;
    if (s_isPortable != -1) return (s_isPortable == 1);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--portable") == 0 || _wcsicmp(argv[i], L"-p") == 0) {
                LocalFree(argv);
                s_isPortable = 1;
                return true;
            }
        }
        LocalFree(argv);
    }

    std::wstring iniPath = GetDsnapDirectory() + L"dsnap.ini";
    DWORD attr = GetFileAttributesW(iniPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        s_isPortable = 1;
        return true;
    }

    s_isPortable = 0;
    return false;
}

std::wstring GetIniFilePath() {
    return GetDsnapDirectory() + L"dsnap.ini";
}

std::wstring FindFFmpegPath() {
    std::wstring localPath = GetDsnapDirectory() + L"ffmpeg.exe";
    if (GetFileAttributesW(localPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return localPath;
    }
    wchar_t sysPath[MAX_PATH];
    DWORD res = SearchPathW(NULL, L"ffmpeg.exe", NULL, MAX_PATH, sysPath, NULL);
    if (res > 0 && res < MAX_PATH) {
        return std::wstring(sysPath);
    }
    return L"";
}

std::wstring GetTimestampW() {
    time_t rawtime;
    struct tm ti;
    wchar_t buf[64];
    time(&rawtime);
    localtime_s(&ti, &rawtime);
    wcsftime(buf, sizeof(buf) / sizeof(wchar_t), L"%Y%m%d-%H%M%S", &ti);
    return std::wstring(buf);
}

static void DownloadFFmpegWorker() {
    g_isDownloadingFFmpeg = true;
    UpdateTrayTip(L"dsnap (Downloading FFmpeg core...)");

    NOTIFYICONDATAW nid = g_nid;
    nid.uFlags = NIF_INFO;
    wcscpy_s(nid.szInfoTitle, L"dsnap - Downloading FFmpeg");
    wcscpy_s(nid.szInfo, L"Downloading FFmpeg core for screen recording (~28 MB)...\nPlease wait a moment.");
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid);

    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring zipPath = std::wstring(tempPath) + L"dsnap_ffmpeg.zip";
    std::wstring dsnapDir = GetDsnapDirectory();

    const wchar_t* url = L"https://github.com/datlp/dsnap/releases/download/v1.0.0/ffmpeg.zip";
    HRESULT hr = URLDownloadToFileW(NULL, url, zipPath.c_str(), 0, NULL);

    if (SUCCEEDED(hr)) {
        // Extract using native Windows tar.exe (available in Windows 10/11)
        std::wstring extractCmd = L"tar -xf \"" + zipPath + L"\" -C \"" + dsnapDir + L"\"";
        _wsystem(extractCmd.c_str());
        DeleteFileW(zipPath.c_str());

        // Verify if ffmpeg.exe is now available
        if (!FindFFmpegPath().empty()) {
            wcscpy_s(nid.szInfoTitle, L"dsnap - FFmpeg Ready!");
            wcscpy_s(nid.szInfo, L"FFmpeg installed successfully!\nYou can now record your screen (Shift + PrtScn).");
            nid.dwInfoFlags = NIIF_INFO;
            Shell_NotifyIconW(NIM_MODIFY, &nid);
            UpdateTrayTip(Tr(StringId::TrayTipDefault));
            g_isDownloadingFFmpeg = false;
            return;
        }
    }

    DeleteFileW(zipPath.c_str());
    wcscpy_s(nid.szInfoTitle, L"dsnap - Download Failed");
    wcscpy_s(nid.szInfo, L"Could not download FFmpeg automatically. Please check your internet connection.");
    nid.dwInfoFlags = NIIF_WARNING;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    UpdateTrayTip(Tr(StringId::TrayTipDefault));
    g_isDownloadingFFmpeg = false;
}

bool EnsureFFmpegAvailable(HWND hWndParent) {
    if (!FindFFmpegPath().empty()) {
        return true;
    }

    if (IsFFmpegDownloading()) {
        MessageBoxW(hWndParent,
            L"FFmpeg is currently downloading in the background.\nPlease wait a moment for the download to complete.",
            L"dsnap - FFmpeg Setup", MB_ICONINFORMATION | MB_TOPMOST);
        return false;
    }

    int res = MessageBoxW(hWndParent,
        Tr(StringId::FFmpegPromptBody),
        Tr(StringId::FFmpegPromptTitle),
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);

    if (res == IDYES) {
        std::thread t(DownloadFFmpegWorker);
        t.detach();
    }
    return false;
}
