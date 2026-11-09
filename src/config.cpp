#include "config.h"
#include "tray.h"
#include <urlmon.h>
#include <thread>
#include <atomic>

bool g_recordAudio = false;

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
    HKEY hKey;
    DWORD val = enable ? 1 : 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"RecordAudio", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

std::wstring GetScreenshotsDir() {
    PWSTR pPath = NULL;
    std::wstring outDir = L"";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Screenshots, 0, NULL, &pPath))) {
        outDir = pPath;
        CoTaskMemFree(pPath);
    } else if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &pPath))) {
        outDir = std::wstring(pPath) + L"\\Screenshots";
        CoTaskMemFree(pPath);
    } else {
        wchar_t userProfile[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            outDir = std::wstring(userProfile) + L"\\Pictures\\Screenshots";
        } else {
            outDir = L".";
        }
    }
    CreateDirectoryW(outDir.c_str(), NULL);
    return outDir;
}

std::wstring GetCapturesDir() {
    PWSTR pPath = NULL;
    std::wstring outDir = L"";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Captures, 0, NULL, &pPath))) {
        outDir = pPath;
        CoTaskMemFree(pPath);
    } else if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &pPath))) {
        outDir = std::wstring(pPath) + L"\\Captures";
        CoTaskMemFree(pPath);
    } else {
        wchar_t userProfile[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            outDir = std::wstring(userProfile) + L"\\Videos\\Captures";
        } else {
            outDir = L".";
        }
    }
    CreateDirectoryW(outDir.c_str(), NULL);
    return outDir;
}

std::wstring GetDsnapDirectory() {
    wchar_t appDir[MAX_PATH];
    GetModuleFileNameW(NULL, appDir, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(appDir, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    return std::wstring(appDir);
}

std::wstring FindFFmpegPath() {
    std::wstring appDir = GetDsnapDirectory();
    std::wstring localFFmpeg = appDir + L"ffmpeg.exe";
    if (GetFileAttributesW(localFFmpeg.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return localFFmpeg;
    }

    wchar_t pathBuf[MAX_PATH];
    DWORD res = SearchPathW(NULL, L"ffmpeg.exe", NULL, MAX_PATH, pathBuf, NULL);
    if (res > 0 && res < MAX_PATH) {
        return std::wstring(pathBuf);
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
    wcscpy_s(nid.szInfo, L"Downloading FFmpeg core for screen recording (~84 MB)...\nPlease wait a moment.");
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
            UpdateTrayTip(L"dsnap (Ready)");
            g_isDownloadingFFmpeg = false;
            return;
        }
    }

    DeleteFileW(zipPath.c_str());
    wcscpy_s(nid.szInfoTitle, L"dsnap - Download Failed");
    wcscpy_s(nid.szInfo, L"Could not download FFmpeg automatically. Please check your internet connection.");
    nid.dwInfoFlags = NIIF_WARNING;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    UpdateTrayTip(L"dsnap (Ready)");
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
        L"Screen recording requires FFmpeg, which was not found on your system.\n\n"
        L"Would you like dsnap to download and install it automatically?\n"
        L"(One-time setup from GitHub Release CDN ~84 MB)",
        L"dsnap - FFmpeg Setup",
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);

    if (res == IDYES) {
        std::thread t(DownloadFFmpegWorker);
        t.detach();
    }
    return false;
}
