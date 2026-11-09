#include "config.h"

bool g_recordAudio = false;

bool IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        LONG res = RegQueryValueExW(hKey, APP_NAME, NULL, &type, NULL, NULL);
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
            std::wstring cmd = L"\"" + std::wstring(path) + L"\"";
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ,
                (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
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
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"RecordAudio", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return val != 0;
        }
        RegCloseKey(hKey);
    }
    return false;
}

void SaveAudioSetting(bool enable) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = enable ? 1 : 0;
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

std::wstring FindFFmpegPath() {
    wchar_t appDir[MAX_PATH];
    GetModuleFileNameW(NULL, appDir, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(appDir, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';

    std::wstring localFFmpeg = std::wstring(appDir) + L"ffmpeg.exe";
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
