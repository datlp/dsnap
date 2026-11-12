#include "config.h"
#include "tray.h"
#include <urlmon.h>
#include <thread>
#include <atomic>

bool g_recordAudio = false;
bool g_recordMic = false;
bool g_highlightClicks = false;
std::vector<std::wstring> g_selectedOutputDevices;
std::vector<std::wstring> g_selectedInputDevices;

static std::atomic<bool> g_isDownloadingFFmpeg(false);

bool IsFFmpegDownloading() {
    return g_isDownloadingFFmpeg.load();
}

bool IsProcessElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated != FALSE;
}

bool IsRunAsAdminConfigured() {
    if (IsProcessElevated()) return true;

    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    for (HKEY root : roots) {
        HKEY hKey;
        if (RegOpenKeyExW(root, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[128] = { 0 };
            DWORD size = sizeof(val);
            DWORD type = REG_SZ;
            if (RegQueryValueExW(hKey, exePath, NULL, &type, (LPBYTE)val, &size) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                if (wcsstr(val, L"RUNASADMIN") != NULL) {
                    return true;
                }
            } else {
                RegCloseKey(hKey);
            }
        }
    }
    return false;
}

bool IsAdminAutoStartTaskEnabled() {
    HKEY hKey;
    DWORD val = 0;
    DWORD size = sizeof(val);
    DWORD type = REG_DWORD;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"AutoStartTask", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val != 0);
        }
        RegCloseKey(hKey);
    }
    return false;
}

static bool RunProcessHidden(const std::wstring& cmdLine) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };

    std::wstring mutableCmd = cmdLine;
    if (CreateProcessW(NULL, &mutableCmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

bool SetAdminAutoStartTask(bool enable) {
    if (enable) {
        wchar_t exePath[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        wchar_t tempDir[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, tempDir);
        std::wstring xmlPath = std::wstring(tempDir) + L"dsnap_autostart_task.xml";

        std::wstring xml =
            L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
            L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\r\n"
            L"  <Triggers>\r\n"
            L"    <LogonTrigger>\r\n"
            L"      <Enabled>true</Enabled>\r\n"
            L"    </LogonTrigger>\r\n"
            L"  </Triggers>\r\n"
            L"  <Principals>\r\n"
            L"    <Principal id=\"Author\">\r\n"
            L"      <LogonType>InteractiveToken</LogonType>\r\n"
            L"      <RunLevel>HighestAvailable</RunLevel>\r\n"
            L"    </Principal>\r\n"
            L"  </Principals>\r\n"
            L"  <Settings>\r\n"
            L"    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\r\n"
            L"    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\r\n"
            L"    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\r\n"
            L"    <AllowHardTerminate>true</AllowHardTerminate>\r\n"
            L"    <StartWhenAvailable>true</StartWhenAvailable>\r\n"
            L"    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>\r\n"
            L"    <AllowStartOnDemand>true</AllowStartOnDemand>\r\n"
            L"    <Enabled>true</Enabled>\r\n"
            L"    <Hidden>false</Hidden>\r\n"
            L"    <RunOnlyIfIdle>false</RunOnlyIfIdle>\r\n"
            L"    <WakeToRun>false</WakeToRun>\r\n"
            L"    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\r\n"
            L"    <Priority>7</Priority>\r\n"
            L"  </Settings>\r\n"
            L"  <Actions Context=\"Author\">\r\n"
            L"    <Exec>\r\n"
            L"      <Command>\"" + std::wstring(exePath) + L"\"</Command>\r\n"
            L"    </Exec>\r\n"
            L"  </Actions>\r\n"
            L"</Task>\r\n";

        HANDLE hFile = CreateFileW(xmlPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            const WORD bom = 0xFEFF;
            WriteFile(hFile, &bom, sizeof(bom), &written, NULL);
            WriteFile(hFile, xml.c_str(), (DWORD)(xml.length() * sizeof(wchar_t)), &written, NULL);
            CloseHandle(hFile);
        }

        std::wstring cmd = L"schtasks.exe /Create /TN \"dsnap_Autostart\" /XML \"" + xmlPath + L"\" /F";
        bool ok = RunProcessHidden(cmd);
        DeleteFileW(xmlPath.c_str());

        if (ok) {
            HKEY hKey;
            DWORD val = 1;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, L"AutoStartTask", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
                RegCloseKey(hKey);
            }
            // Remove standard HKCU Run value to prevent Windows blocking duplicate entry on logon
            if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueW(hKey, APP_NAME);
                RegCloseKey(hKey);
            }
        }
        return ok;
    } else {
        std::wstring cmd = L"schtasks.exe /Delete /TN \"dsnap_Autostart\" /F";
        RunProcessHidden(cmd);

        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"AutoStartTask");
            RegCloseKey(hKey);
        }
        return true;
    }
}

bool IsAutoStartEnabled() {
    if (IsAdminAutoStartTaskEnabled()) {
        return true;
    }
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
    if (enable && IsRunAsAdminConfigured()) {
        if (IsProcessElevated()) {
            SetAdminAutoStartTask(true);
        } else {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = path;
            sei.lpParameters = L"--register-admin-task";
            sei.nShow = SW_HIDE;
            ShellExecuteExW(&sei);
        }
        return;
    }

    if (!enable && IsAdminAutoStartTaskEnabled()) {
        if (IsProcessElevated()) {
            SetAdminAutoStartTask(false);
        } else {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = path;
            sei.lpParameters = L"--unregister-admin-task";
            sei.nShow = SW_HIDE;
            ShellExecuteExW(&sei);
        }
    }

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

bool LoadMicSetting() {
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        return GetPrivateProfileIntW(L"Settings", L"RecordMic", 0, ini.c_str()) != 0;
    }
    HKEY hKey;
    DWORD val = 0;
    DWORD size = sizeof(val);
    DWORD type = REG_DWORD;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"RecordMic", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val != 0);
        }
        RegCloseKey(hKey);
    }
    return false;
}

void SaveMicSetting(bool enable) {
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        WritePrivateProfileStringW(L"Settings", L"RecordMic", enable ? L"1" : L"0", ini.c_str());
        return;
    }
    HKEY hKey;
    DWORD val = enable ? 1 : 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"RecordMic", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

static std::vector<std::wstring> SplitDeviceIds(const std::wstring& str) {
    std::vector<std::wstring> result;
    size_t start = 0;
    while (start < str.length()) {
        size_t pos = str.find(L'|', start);
        std::wstring token = (pos == std::wstring::npos) ? str.substr(start) : str.substr(start, pos - start);
        if (!token.empty()) {
            result.push_back(token);
        }
        if (pos == std::wstring::npos) break;
        start = pos + 1;
    }
    return result;
}

static std::wstring JoinDeviceIds(const std::vector<std::wstring>& ids) {
    std::wstring res;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) res += L"|";
        res += ids[i];
    }
    return res;
}

void LoadAudioDevicesSetting() {
    wchar_t outBuf[2048] = { 0 };
    wchar_t inBuf[2048] = { 0 };
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        GetPrivateProfileStringW(L"Settings", L"OutputDevices", L"", outBuf, 2048, ini.c_str());
        GetPrivateProfileStringW(L"Settings", L"InputDevices", L"", inBuf, 2048, ini.c_str());
    } else {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD outSize = sizeof(outBuf);
            DWORD inSize = sizeof(inBuf);
            DWORD type = REG_SZ;
            RegQueryValueExW(hKey, L"OutputDevices", NULL, &type, (LPBYTE)outBuf, &outSize);
            RegQueryValueExW(hKey, L"InputDevices", NULL, &type, (LPBYTE)inBuf, &inSize);
            RegCloseKey(hKey);
        }
    }
    g_selectedOutputDevices = SplitDeviceIds(outBuf);
    g_selectedInputDevices = SplitDeviceIds(inBuf);
}

void SaveAudioDevicesSetting() {
    std::wstring outStr = JoinDeviceIds(g_selectedOutputDevices);
    std::wstring inStr = JoinDeviceIds(g_selectedInputDevices);
    if (IsPortableMode()) {
        std::wstring ini = GetIniFilePath();
        WritePrivateProfileStringW(L"Settings", L"OutputDevices", outStr.c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Settings", L"InputDevices", inStr.c_str(), ini.c_str());
        return;
    }
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"OutputDevices", 0, REG_SZ, (const BYTE*)outStr.c_str(), (DWORD)((outStr.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"InputDevices", 0, REG_SZ, (const BYTE*)inStr.c_str(), (DWORD)((inStr.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

bool IsOutputDeviceSelected(const std::wstring& id) {
    return std::find(g_selectedOutputDevices.begin(), g_selectedOutputDevices.end(), id) != g_selectedOutputDevices.end();
}

void ToggleOutputDeviceSelection(const std::wstring& id) {
    auto it = std::find(g_selectedOutputDevices.begin(), g_selectedOutputDevices.end(), id);
    if (it != g_selectedOutputDevices.end()) {
        g_selectedOutputDevices.erase(it);
    } else {
        g_selectedOutputDevices.push_back(id);
    }
    SaveAudioDevicesSetting();
}

void ClearOutputDeviceSelection() {
    g_selectedOutputDevices.clear();
    SaveAudioDevicesSetting();
}

bool IsInputDeviceSelected(const std::wstring& id) {
    return std::find(g_selectedInputDevices.begin(), g_selectedInputDevices.end(), id) != g_selectedInputDevices.end();
}

void ToggleInputDeviceSelection(const std::wstring& id) {
    auto it = std::find(g_selectedInputDevices.begin(), g_selectedInputDevices.end(), id);
    if (it != g_selectedInputDevices.end()) {
        g_selectedInputDevices.erase(it);
    } else {
        g_selectedInputDevices.push_back(id);
    }
    SaveAudioDevicesSetting();
}

void ClearInputDeviceSelection() {
    g_selectedInputDevices.clear();
    SaveAudioDevicesSetting();
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
