#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <initguid.h>
#include <knownfolders.h>
#include <gdiplus.h>
#include <dshow.h>
#include <string>
#include <ctime>
#include <vector>
#include <algorithm>

// Define Known Folder GUIDs if missing from MinGW headers
#ifndef FOLDERID_Captures
DEFINE_GUID(FOLDERID_Captures, 0xedc0fe99, 0xaa4b, 0x4770, 0x81, 0xa2, 0x05, 0x6e, 0xe3, 0xde, 0xc1, 0xab);
#endif

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "strmiids.lib")

using namespace Gdiplus;

// Identifiers
#define IDI_APPICON           101
#define WM_TRAYICON           (WM_USER + 1)
#define WM_DSNAP_TRIGGER      (WM_USER + 100)
#define IDM_CAPTURE_PHOTO     1001
#define IDM_RECORD_VIDEO      1002
#define IDM_OPEN_SCREENSHOTS  1003
#define IDM_OPEN_CAPTURES     1004
#define IDM_TOGGLE_AUDIO      1005
#define IDM_AUTOSTART         1006
#define IDM_EXIT              1007

#define TRIGGER_PHOTO         1
#define TRIGGER_VIDEO         2

#define TIMER_RECORDING       101

const wchar_t APP_NAME[]      = L"dsnap";
const wchar_t REG_KEY[]       = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t BG_CLASS[]      = L"DsnapBackground";
const wchar_t OVERLAY_CLASS[] = L"DsnapOverlay";
const wchar_t BORDER_CLASS[]  = L"DsnapRecordingBorder";
const wchar_t TOOLBAR_CLASS[] = L"DsnapRecordingToolbar";

// NTDLL Function pointers for Process Suspend/Resume (Pause recording)
typedef LONG(NTAPI* PFN_NtSuspendProcess)(HANDLE ProcessHandle);
typedef LONG(NTAPI* PFN_NtResumeProcess)(HANDLE ProcessHandle);
static PFN_NtSuspendProcess pfnNtSuspendProcess = NULL;
static PFN_NtResumeProcess  pfnNtResumeProcess  = NULL;

// Global Variables
HINSTANCE g_hInstance = NULL;
HHOOK g_hKeyboardHook = NULL;
HWND g_hMainWnd = NULL;
NOTIFYICONDATAW g_nid = { 0 };
HICON g_hAppIcon = NULL;

// Recorder state
HANDLE g_hFFmpegStdinWrite = NULL;
PROCESS_INFORMATION g_ffmpegPi = { 0 };
bool g_isRecording = false;
bool g_isPaused = false;
int g_recordingSeconds = 0;
std::wstring g_currentRecordingPath = L"";
bool g_recordAudio = false; // Mặc định KHÔNG ghi kèm âm thanh

// Đọc / Ghi cấu hình ghi âm thanh từ Registry
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
    return false; // Default: NO
}

void SaveAudioSetting(bool enable) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\dsnap", 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = enable ? 1 : 0;
        RegSetValueExW(hKey, L"RecordAudio", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

// Lấy tên thiết bị thu âm thanh DirectShow đầu tiên có sẵn
std::wstring GetDefaultDirectShowAudioDevice() {
    ICreateDevEnum* pDevEnum = NULL;
    IEnumMoniker* pEnum = NULL;
    std::wstring deviceName = L"";

    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pDevEnum);
    if (SUCCEEDED(hr)) {
        hr = pDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEnum, 0);
        if (hr == S_OK) {
            IMoniker* pMoniker = NULL;
            if (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
                IPropertyBag* pPropBag = NULL;
                hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
                if (SUCCEEDED(hr)) {
                    VARIANT var;
                    VariantInit(&var);
                    hr = pPropBag->Read(L"FriendlyName", &var, 0);
                    if (SUCCEEDED(hr) && var.bstrVal) {
                        deviceName = var.bstrVal;
                        VariantClear(&var);
                    }
                    pPropBag->Release();
                }
                pMoniker->Release();
            }
            pEnum->Release();
        }
        pDevEnum->Release();
    }
    return deviceName;
}

// Recording UI handles
HWND g_hWndRecordingBorder = NULL;
HWND g_hWndRecordingToolbar = NULL;
int g_recX = 0, g_recY = 0, g_recW = 0, g_recH = 0;

// Overlay state
enum Mode { MODE_NONE, MODE_PHOTO, MODE_VIDEO };
Mode g_currentMode = MODE_NONE;
bool g_isOverlayActive = false;
HWND g_hWndOverlay = NULL;
bool g_isDragging = false;
POINT g_ptStartScreen = { 0, 0 };
RECT g_selectedRectScreen = { 0, 0, 0, 0 };

// Freeze Snapshot Bitmaps
HBITMAP g_hFrozenBitmap = NULL;
HBITMAP g_hDimmedBitmap = NULL;
int g_virtualX = 0;
int g_virtualY = 0;
int g_virtualW = 0;
int g_virtualH = 0;

// Exclude window from screen capture (invisible in recording)
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

// Enable DPI Awareness
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

// Update Tray Tooltip
void UpdateTrayTip(const wchar_t* tip) {
    wcsncpy_s(g_nid.szTip, tip, _countof(g_nid.szTip) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// Registry Autostart Check & Set
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

// GDI+ PNG Encoder CLSID
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    std::vector<BYTE> buffer(size);
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)buffer.data();
    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            return j;
        }
    }
    return -1;
}

// Timestamp generator: yyyyMMdd-HHmmss
std::wstring GetTimestampW() {
    time_t rawtime;
    struct tm ti;
    wchar_t buf[64];
    time(&rawtime);
    localtime_s(&ti, &rawtime);
    wcsftime(buf, sizeof(buf) / sizeof(wchar_t), L"%Y%m%d-%H%M%S", &ti);
    return std::wstring(buf);
}

// Resolve Pictures\Screenshots directory
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

// Resolve Videos\Captures directory
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

// Resolve ffmpeg.exe path
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

// Forward declarations
void StopRecording(bool cancel);
void TogglePauseRecording();

// Save Screenshot from Frozen Desktop Bitmap
void SaveScreenshotFromFrozen(int selX, int selY, int w, int h) {
    if (w <= 0 || h <= 0 || !g_hFrozenBitmap) return;

    HDC hScreenDC = GetDC(NULL);
    HDC hFrozenDC = CreateCompatibleDC(hScreenDC);
    HDC hCropDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hCropBitmap = CreateCompatibleBitmap(hScreenDC, w, h);
    HGDIOBJ oldFrozen = SelectObject(hFrozenDC, g_hFrozenBitmap);
    HGDIOBJ oldCrop = SelectObject(hCropDC, hCropBitmap);

    // Map screen coordinate to virtual screen relative coordinates
    int relX = selX - g_virtualX;
    int relY = selY - g_virtualY;

    BitBlt(hCropDC, 0, 0, w, h, hFrozenDC, relX, relY, SRCCOPY);

    // Save PNG using GDI+
    std::wstring outDir = GetScreenshotsDir();
    std::wstring filePath = outDir + L"\\dsnap-screenshot-" + GetTimestampW() + L".png";

    Bitmap* gdiBitmap = new Bitmap(hCropBitmap, NULL);
    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) != -1) {
        gdiBitmap->Save(filePath.c_str(), &pngClsid, NULL);
    }
    delete gdiBitmap;

    // Copy to Clipboard (System takes ownership of hCropBitmap)
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hCropBitmap);
        CloseClipboard();
    } else {
        DeleteObject(hCropBitmap);
    }

    SelectObject(hCropDC, oldCrop);
    SelectObject(hFrozenDC, oldFrozen);
    DeleteDC(hCropDC);
    DeleteDC(hFrozenDC);
    ReleaseDC(NULL, hScreenDC);
}

// Clean up frozen snapshot bitmaps
void CleanupFrozenBitmaps() {
    if (g_hFrozenBitmap) {
        DeleteObject(g_hFrozenBitmap);
        g_hFrozenBitmap = NULL;
    }
    if (g_hDimmedBitmap) {
        DeleteObject(g_hDimmedBitmap);
        g_hDimmedBitmap = NULL;
    }
}

// Capture and Freeze Desktop into RAM
void CreateFrozenDesktopSnapshot() {
    CleanupFrozenBitmaps();

    g_virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_virtualW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_virtualH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    g_hFrozenBitmap = CreateCompatibleBitmap(hScreenDC, g_virtualW, g_virtualH);
    HGDIOBJ oldBmp = SelectObject(hMemDC, g_hFrozenBitmap);

    // Capture entire virtual desktop (all monitors, context menus, tooltips, flyouts)
    BitBlt(hMemDC, 0, 0, g_virtualW, g_virtualH, hScreenDC, g_virtualX, g_virtualY, SRCCOPY);

    // Create Dimmed Version using GDI+ for smooth dark backdrop
    Bitmap* bmp = Bitmap::FromHBITMAP(g_hFrozenBitmap, NULL);
    if (bmp) {
        Bitmap* dimmedBmp = new Bitmap(g_virtualW, g_virtualH, PixelFormat32bppARGB);
        Graphics g(dimmedBmp);
        g.DrawImage(bmp, 0, 0, g_virtualW, g_virtualH);

        SolidBrush darkBrush(Color(100, 0, 0, 0)); // 39% dark overlay
        g.FillRectangle(&darkBrush, 0, 0, g_virtualW, g_virtualH);

        dimmedBmp->GetHBITMAP(Color(0, 0, 0), &g_hDimmedBitmap);
        delete dimmedBmp;
        delete bmp;
    }

    SelectObject(hMemDC, oldBmp);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);

    // Release mouse capture and close any blocking context menus from other apps
    ReleaseCapture();
    EndMenu();
}

// Recording Border Window Procedure (Transparent interior, click-through)
LRESULT CALLBACK RecordingBorderProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HBRUSH hTransBrush = CreateSolidBrush(RGB(255, 0, 255));
        FillRect(hdc, &rc, hTransBrush);
        DeleteObject(hTransBrush);

        COLORREF borderColor = g_isPaused ? RGB(234, 179, 8) : RGB(239, 68, 68);
        HPEN hPen = CreatePen(PS_SOLID, 3, borderColor);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

        Rectangle(hdc, 1, 1, rc.right - 1, rc.bottom - 1);

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hPen);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Recording Floating Toolbar Procedure
static int g_hoverButton = 0;

LRESULT CALLBACK RecordingToolbarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        SetTimer(hWnd, TIMER_RECORDING, 1000, NULL);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == TIMER_RECORDING && g_isRecording && !g_isPaused) {
            g_recordingSeconds++;
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam);
        int oldHover = g_hoverButton;
        if (x >= 75 && x < 118) g_hoverButton = 1;
        else if (x >= 122 && x < 165) g_hoverButton = 2;
        else if (x >= 169 && x < 205) g_hoverButton = 3;
        else g_hoverButton = 0;

        if (oldHover != g_hoverButton) {
            InvalidateRect(hWnd, NULL, TRUE);
        }

        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: {
        g_hoverButton = 0;
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }
    case WM_LBUTTONUP: {
        int x = LOWORD(lParam);
        if (x >= 75 && x < 118) {
            TogglePauseRecording();
        } else if (x >= 122 && x < 165) {
            StopRecording(false);
        } else if (x >= 169 && x < 205) {
            StopRecording(true);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HBRUSH bgBrush = CreateSolidBrush(RGB(15, 23, 42));
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(51, 65, 85));
        HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
        HGDIOBJ oldPen = SelectObject(hdc, borderPen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(bgBrush);
        DeleteObject(borderPen);

        SetBkMode(hdc, TRANSPARENT);

        // Dot & Time
        COLORREF dotCol = g_isPaused ? RGB(234, 179, 8) : RGB(239, 68, 68);
        HBRUSH dotBrush = CreateSolidBrush(dotCol);
        HGDIOBJ oB = SelectObject(hdc, dotBrush);
        Ellipse(hdc, 12, 11, 22, 21);
        SelectObject(hdc, oB);
        DeleteObject(dotBrush);

        int mins = g_recordingSeconds / 60;
        int secs = g_recordingSeconds % 60;
        wchar_t timeBuf[16];
        swprintf_s(timeBuf, L"%02d:%02d", mins, secs);

        SetTextColor(hdc, RGB(241, 245, 249));
        HFONT hFont = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            VARIABLE_PITCH, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(hdc, hFont);
        RECT rcTime = { 28, 6, 72, 26 };
        DrawTextW(hdc, timeBuf, -1, &rcTime, DT_SINGLELINE | DT_VCENTER);

        // Pause/Resume Button
        RECT rcBtn1 = { 75, 4, 118, 28 };
        if (g_hoverButton == 1) {
            HBRUSH hHov = CreateSolidBrush(RGB(51, 65, 85));
            FillRect(hdc, &rcBtn1, hHov);
            DeleteObject(hHov);
        }
        SetTextColor(hdc, g_isPaused ? RGB(234, 179, 8) : RGB(226, 232, 240));
        DrawTextW(hdc, g_isPaused ? L"▶" : L"⏸", -1, &rcBtn1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Stop & Save Button
        RECT rcBtn2 = { 122, 4, 165, 28 };
        if (g_hoverButton == 2) {
            HBRUSH hHov = CreateSolidBrush(RGB(51, 65, 85));
            FillRect(hdc, &rcBtn2, hHov);
            DeleteObject(hHov);
        }
        SetTextColor(hdc, RGB(56, 189, 248));
        DrawTextW(hdc, L"⏹", -1, &rcBtn2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Cancel Button
        RECT rcBtn3 = { 169, 4, 205, 28 };
        if (g_hoverButton == 3) {
            HBRUSH hHov = CreateSolidBrush(RGB(153, 27, 27));
            FillRect(hdc, &rcBtn3, hHov);
            DeleteObject(hHov);
        }
        SetTextColor(hdc, RGB(248, 113, 113));
        DrawTextW(hdc, L"✕", -1, &rcBtn3, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DESTROY: {
        KillTimer(hWnd, TIMER_RECORDING);
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Start Recording via FFmpeg with Visual Region & Top-Right Toolbar
void StartRecording(int x, int y, int w, int h) {
    w -= (w % 2);
    h -= (h % 2);
    if (w <= 0 || h <= 0) return;

    std::wstring ffmpegPath = FindFFmpegPath();
    if (ffmpegPath.empty()) {
        MessageBoxW(NULL,
            L"Không tìm thấy ffmpeg.exe trong thư mục dsnap hoặc trong PATH hệ thống!\n\nVui lòng cài FFmpeg hoặc đặt ffmpeg.exe cùng thư mục.",
            L"dsnap - Thiếu FFmpeg", MB_ICONWARNING | MB_TOPMOST);
        return;
    }

    std::wstring outDir = GetCapturesDir();
    g_currentRecordingPath = outDir + L"\\dsnap-recording-" + GetTimestampW() + L".mp4";

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hStdinRead = NULL;
    if (!CreatePipe(&hStdinRead, &g_hFFmpegStdinWrite, &sa, 0)) return;
    SetHandleInformation(g_hFFmpegStdinWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);
    si.hStdInput = hStdinRead;
    si.dwFlags |= STARTF_USESTDHANDLES;

    std::wstring cmd = L"\"" + ffmpegPath + L"\" -y -f gdigrab -framerate 30 -offset_x " +
        std::to_wstring(x) + L" -offset_y " + std::to_wstring(y) +
        L" -video_size " + std::to_wstring(w) + L"x" + std::to_wstring(h) +
        L" -i desktop";

    if (g_recordAudio) {
        std::wstring audioDev = GetDefaultDirectShowAudioDevice();
        if (!audioDev.empty()) {
            cmd += L" -f dshow -i audio=\"" + audioDev + L"\" -c:a aac -b:a 128k";
        }
    }

    cmd += L" -c:v libx264 -preset ultrafast -crf 23 -pix_fmt yuv420p \"" +
        g_currentRecordingPath + L"\"";

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL success = CreateProcessW(
        NULL,
        cmdBuf.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &g_ffmpegPi
    );

    CloseHandle(hStdinRead);

    if (success) {
        g_isRecording = true;
        g_isPaused = false;
        g_recordingSeconds = 0;
        g_recX = x; g_recY = y; g_recW = w; g_recH = h;

        UpdateTrayTip(L"dsnap (Đang quay video - Nhấn Shift+PrtScn để lưu)");

        // 1. Tạo Khung viền vùng quay
        g_hWndRecordingBorder = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
            BORDER_CLASS, L"", WS_POPUP,
            x - 3, y - 3, w + 6, h + 6,
            NULL, NULL, g_hInstance, NULL
        );
        if (g_hWndRecordingBorder) {
            SetLayeredWindowAttributes(g_hWndRecordingBorder, RGB(255, 0, 255), 0, LWA_COLORKEY);
            ExcludeFromCapture(g_hWndRecordingBorder);
            ShowWindow(g_hWndRecordingBorder, SW_SHOWNOACTIVATE);
        }

        // 2. Tạo Thanh điều khiển nổi góc trên bên phải
        int tbW = 210, tbH = 32;
        int tbX = x + w - tbW;
        int tbY = y - tbH - 6;

        int virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int virtualW = GetSystemMetrics(SM_CXVIRTUALSCREEN);

        if (tbY < virtualY) tbY = y + 6;
        if (tbX + tbW > virtualX + virtualW) tbX = virtualX + virtualW - tbW - 4;
        if (tbX < virtualX) tbX = virtualX + 4;

        g_hWndRecordingToolbar = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            TOOLBAR_CLASS, L"", WS_POPUP,
            tbX, tbY, tbW, tbH,
            NULL, NULL, g_hInstance, NULL
        );
        if (g_hWndRecordingToolbar) {
            ExcludeFromCapture(g_hWndRecordingToolbar);
            ShowWindow(g_hWndRecordingToolbar, SW_SHOWNOACTIVATE);
        }
    } else {
        CloseHandle(g_hFFmpegStdinWrite);
        g_hFFmpegStdinWrite = NULL;
        MessageBoxW(NULL, L"Không thể khởi chạy tiến trình FFmpeg!", L"dsnap - Lỗi", MB_ICONERROR);
    }
}

// Toggle Pause / Resume
void TogglePauseRecording() {
    if (!g_isRecording || !g_ffmpegPi.hProcess) return;

    if (!g_isPaused) {
        if (pfnNtSuspendProcess) pfnNtSuspendProcess(g_ffmpegPi.hProcess);
        g_isPaused = true;
        UpdateTrayTip(L"dsnap (Tạm dừng quay video)");
    } else {
        if (pfnNtResumeProcess) pfnNtResumeProcess(g_ffmpegPi.hProcess);
        g_isPaused = false;
        UpdateTrayTip(L"dsnap (Đang quay video - Nhấn Shift+PrtScn để lưu)");
    }

    if (g_hWndRecordingBorder) InvalidateRect(g_hWndRecordingBorder, NULL, TRUE);
    if (g_hWndRecordingToolbar) InvalidateRect(g_hWndRecordingToolbar, NULL, TRUE);
}

// Stop Recording
void StopRecording(bool cancel) {
    if (g_isRecording) {
        if (g_isPaused && pfnNtResumeProcess && g_ffmpegPi.hProcess) {
            pfnNtResumeProcess(g_ffmpegPi.hProcess);
            g_isPaused = false;
        }

        if (cancel) {
            if (g_ffmpegPi.hProcess) {
                TerminateProcess(g_ffmpegPi.hProcess, 0);
                WaitForSingleObject(g_ffmpegPi.hProcess, 1000);
                CloseHandle(g_ffmpegPi.hProcess);
                CloseHandle(g_ffmpegPi.hThread);
            }
            if (g_hFFmpegStdinWrite) {
                CloseHandle(g_hFFmpegStdinWrite);
                g_hFFmpegStdinWrite = NULL;
            }
            if (!g_currentRecordingPath.empty()) {
                DeleteFileW(g_currentRecordingPath.c_str());
            }
        } else {
            if (g_hFFmpegStdinWrite != NULL) {
                DWORD bytesWritten;
                const char* q = "q\n";
                WriteFile(g_hFFmpegStdinWrite, q, (DWORD)strlen(q), &bytesWritten, NULL);

                if (g_ffmpegPi.hProcess) {
                    WaitForSingleObject(g_ffmpegPi.hProcess, 4000);
                    CloseHandle(g_ffmpegPi.hProcess);
                    CloseHandle(g_ffmpegPi.hThread);
                }

                CloseHandle(g_hFFmpegStdinWrite);
                g_hFFmpegStdinWrite = NULL;
            }
        }

        ZeroMemory(&g_ffmpegPi, sizeof(g_ffmpegPi));
        g_isRecording = false;
        g_isPaused = false;
        g_currentRecordingPath = L"";

        if (g_hWndRecordingToolbar) {
            DestroyWindow(g_hWndRecordingToolbar);
            g_hWndRecordingToolbar = NULL;
        }
        if (g_hWndRecordingBorder) {
            DestroyWindow(g_hWndRecordingBorder);
            g_hWndRecordingBorder = NULL;
        }

        UpdateTrayTip(L"dsnap (Sẵn sàng)");
    }
}

// Overlay Window Proc (Supports both Freeze Snapshot for Photo and Live Layer for Video)
LRESULT CALLBACK OverlayProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_LBUTTONDOWN: {
        g_isDragging = true;
        SetCapture(hWnd);
        GetCursorPos(&g_ptStartScreen);
        g_selectedRectScreen = { g_ptStartScreen.x, g_ptStartScreen.y, g_ptStartScreen.x, g_ptStartScreen.y };
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (g_isDragging) {
            POINT ptCur;
            GetCursorPos(&ptCur);
            g_selectedRectScreen.left   = (std::min)(g_ptStartScreen.x, ptCur.x);
            g_selectedRectScreen.top    = (std::min)(g_ptStartScreen.y, ptCur.y);
            g_selectedRectScreen.right  = (std::max)(g_ptStartScreen.x, ptCur.x);
            g_selectedRectScreen.bottom = (std::max)(g_ptStartScreen.y, ptCur.y);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_isDragging) {
            g_isDragging = false;
            ReleaseCapture();

            int x = g_selectedRectScreen.left;
            int y = g_selectedRectScreen.top;
            int w = g_selectedRectScreen.right - g_selectedRectScreen.left;
            int h = g_selectedRectScreen.bottom - g_selectedRectScreen.top;
            Mode m = g_currentMode;

            // Đóng overlay ngay lập tức
            DestroyWindow(hWnd);

            if (w > 10 && h > 10) {
                if (m == MODE_PHOTO) {
                    SaveScreenshotFromFrozen(x, y, w, h);
                } else if (m == MODE_VIDEO) {
                    StartRecording(x, y, w, h);
                }
            }
            CleanupFrozenBitmaps();
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            g_isDragging = false;
            ReleaseCapture();
            DestroyWindow(hWnd);
            CleanupFrozenBitmaps();
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        int clientW = rcClient.right - rcClient.left;
        int clientH = rcClient.bottom - rcClient.top;

        // Double-buffering to eliminate any dragging flicker
        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
        HGDIOBJ oldMemBmp = SelectObject(hMemDC, hMemBmp);

        if (g_currentMode == MODE_PHOTO && g_hDimmedBitmap && g_hFrozenBitmap) {
            HDC hDimmedDC = CreateCompatibleDC(hdc);
            HDC hFrozenDC = CreateCompatibleDC(hdc);
            HGDIOBJ oldDimmed = SelectObject(hDimmedDC, g_hDimmedBitmap);
            HGDIOBJ oldFrozen = SelectObject(hFrozenDC, g_hFrozenBitmap);

            // 1. Draw dimmed backdrop
            BitBlt(hMemDC, 0, 0, clientW, clientH, hDimmedDC, 0, 0, SRCCOPY);

            // 2. If dragging, illuminate the selected box with the bright original pixels
            if (g_isDragging) {
                POINT p1 = { g_selectedRectScreen.left, g_selectedRectScreen.top };
                POINT p2 = { g_selectedRectScreen.right, g_selectedRectScreen.bottom };
                ScreenToClient(hWnd, &p1);
                ScreenToClient(hWnd, &p2);

                int selW = p2.x - p1.x;
                int selH = p2.y - p1.y;

                if (selW > 0 && selH > 0) {
                    BitBlt(hMemDC, p1.x, p1.y, selW, selH, hFrozenDC, p1.x, p1.y, SRCCOPY);

                    // 2px Cyan outline
                    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 162, 232));
                    HGDIOBJ oldPen = SelectObject(hMemDC, hPen);
                    HGDIOBJ oldBrush = SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
                    Rectangle(hMemDC, p1.x, p1.y, p2.x, p2.y);
                    SelectObject(hMemDC, oldPen);
                    SelectObject(hMemDC, oldBrush);
                    DeleteObject(hPen);

                    // Dimension badge [W x H]
                    wchar_t dimBuf[32];
                    swprintf_s(dimBuf, L"%d × %d", selW, selH);

                    SetBkMode(hMemDC, TRANSPARENT);
                    SetTextColor(hMemDC, RGB(255, 255, 255));
                    HFONT hBadgeFont = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        VARIABLE_PITCH, L"Segoe UI");
                    HGDIOBJ oldFont = SelectObject(hMemDC, hBadgeFont);

                    SIZE textSize;
                    GetTextExtentPoint32W(hMemDC, dimBuf, (int)wcslen(dimBuf), &textSize);

                    int badgeX = p2.x + 8;
                    int badgeY = p2.y + 8;
                    if (badgeX + textSize.cx + 10 > clientW) badgeX = p1.x - textSize.cx - 8;
                    if (badgeY + textSize.cy + 10 > clientH) badgeY = p1.y - textSize.cy - 8;

                    RECT rcBadgeBg = { badgeX - 4, badgeY - 2, badgeX + textSize.cx + 4, badgeY + textSize.cy + 2 };
                    HBRUSH hBadgeBg = CreateSolidBrush(RGB(15, 23, 42));
                    FillRect(hMemDC, &rcBadgeBg, hBadgeBg);
                    DeleteObject(hBadgeBg);

                    TextOutW(hMemDC, badgeX, badgeY, dimBuf, (int)wcslen(dimBuf));

                    SelectObject(hMemDC, oldFont);
                    DeleteObject(hBadgeFont);
                }
            }

            SelectObject(hDimmedDC, oldDimmed);
            SelectObject(hFrozenDC, oldFrozen);
            DeleteDC(hDimmedDC);
            DeleteDC(hFrozenDC);
        } else {
            // Video Mode: semi-transparent live layer
            HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hMemDC, &rcClient, hBlackBrush);
            DeleteObject(hBlackBrush);

            if (g_isDragging) {
                POINT p1 = { g_selectedRectScreen.left, g_selectedRectScreen.top };
                POINT p2 = { g_selectedRectScreen.right, g_selectedRectScreen.bottom };
                ScreenToClient(hWnd, &p1);
                ScreenToClient(hWnd, &p2);

                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(239, 68, 68));
                HGDIOBJ oldPen = SelectObject(hMemDC, hPen);
                HGDIOBJ oldBrush = SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
                Rectangle(hMemDC, p1.x, p1.y, p2.x, p2.y);
                SelectObject(hMemDC, oldPen);
                SelectObject(hMemDC, oldBrush);
                DeleteObject(hPen);
            }
        }

        // Blit backbuffer to screen
        BitBlt(hdc, 0, 0, clientW, clientH, hMemDC, 0, 0, SRCCOPY);

        SelectObject(hMemDC, oldMemBmp);
        DeleteObject(hMemBmp);
        DeleteDC(hMemDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY: {
        g_hWndOverlay = NULL;
        g_isOverlayActive = false;
        g_currentMode = MODE_NONE;
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Trigger Overlay safely
void TriggerOverlay(Mode mode) {
    if (g_isOverlayActive && g_hWndOverlay != NULL) {
        return;
    }

    g_currentMode = mode;
    g_isOverlayActive = true;
    g_isDragging = false;

    if (mode == MODE_PHOTO) {
        // 1. Chụp ngay lập tức toàn bộ màn hình vào RAM (đóng băng menu chuột phải, tooltips)
        CreateFrozenDesktopSnapshot();

        // 2. Tạo cửa sổ fullscreen hiển thị ảnh đã đóng băng
        g_hWndOverlay = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            OVERLAY_CLASS, L"", WS_POPUP,
            g_virtualX, g_virtualY, g_virtualW, g_virtualH,
            NULL, NULL, g_hInstance, NULL
        );
    } else {
        // Chế độ video: cửa sổ live bán trong suốt
        EndMenu();
        int virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int virtualW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int virtualH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        g_hWndOverlay = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            OVERLAY_CLASS, L"", WS_POPUP,
            virtualX, virtualY, virtualW, virtualH,
            NULL, NULL, g_hInstance, NULL
        );
        if (g_hWndOverlay) {
            SetLayeredWindowAttributes(g_hWndOverlay, 0, 60, LWA_ALPHA);
        }
    }

    if (g_hWndOverlay) {
        ShowWindow(g_hWndOverlay, SW_SHOW);
        SetForegroundWindow(g_hWndOverlay);
        UpdateWindow(g_hWndOverlay);
    } else {
        g_isOverlayActive = false;
        CleanupFrozenBitmaps();
    }
}

// Show System Tray Context Menu
void ShowContextMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, IDM_CAPTURE_PHOTO, L"📸 Chụp ảnh màn hình (PrintScreen)");
    AppendMenuW(hMenu, MF_STRING, IDM_RECORD_VIDEO, L"🎥 Quay video màn hình (Shift + PrtScn)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_SCREENSHOTS, L"📁 Mở thư mục Ảnh chụp");
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_CAPTURES, L"📁 Mở thư mục Video quay");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    UINT audioFlag = g_recordAudio ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | audioFlag, IDM_TOGGLE_AUDIO, L"🎙️ Quay kèm âm thanh (Audio)");

    UINT flag = IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | flag, IDM_AUTOSTART, L"Start with Windows");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

// Background Window Proc
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = hWnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_hAppIcon ? g_hAppIcon : LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(g_nid.szTip, L"dsnap (Sẵn sàng)");
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

// Low-Level Keyboard Hook
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (pKey->vkCode == VK_SNAPSHOT) {
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

// WinMain Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // 0. Single Instance Protection (Tránh chạy trùng lặp nhiều app dsnap)
    HANDLE hSingleInstanceMutex = CreateMutexW(NULL, TRUE, L"Local\\Dsnap_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hSingleInstanceMutex) CloseHandle(hSingleInstanceMutex);
        return 0; // Đã có phiên bản dsnap đang chạy
    }

    g_hInstance = hInstance;

    // 1. Khởi tạo COM & Per-Monitor DPI Awareness
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    EnableDpiAwareness();
    g_recordAudio = LoadAudioSetting();

    // 2. Load NTDLL suspend/resume function pointers for Pause/Continue
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        pfnNtSuspendProcess = (PFN_NtSuspendProcess)GetProcAddress(hNtdll, "NtSuspendProcess");
        pfnNtResumeProcess  = (PFN_NtResumeProcess)GetProcAddress(hNtdll, "NtResumeProcess");
    }

    // 3. Load App Icon
    g_hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));

    // 4. Khởi tạo GDI+
    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 5. Đăng ký Window Classes
    WNDCLASSW overlayWc = { 0 };
    overlayWc.lpfnWndProc = OverlayProc;
    overlayWc.hInstance = hInstance;
    overlayWc.lpszClassName = OVERLAY_CLASS;
    overlayWc.hCursor = LoadCursor(NULL, IDC_CROSS);
    overlayWc.hbrBackground = NULL;
    RegisterClassW(&overlayWc);

    WNDCLASSW borderWc = { 0 };
    borderWc.lpfnWndProc = RecordingBorderProc;
    borderWc.hInstance = hInstance;
    borderWc.lpszClassName = BORDER_CLASS;
    borderWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    borderWc.hbrBackground = NULL;
    RegisterClassW(&borderWc);

    WNDCLASSW toolbarWc = { 0 };
    toolbarWc.lpfnWndProc = RecordingToolbarProc;
    toolbarWc.hInstance = hInstance;
    toolbarWc.lpszClassName = TOOLBAR_CLASS;
    toolbarWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    toolbarWc.hbrBackground = NULL;
    RegisterClassW(&toolbarWc);

    WNDCLASSW bgWc = { 0 };
    bgWc.lpfnWndProc = MainWndProc;
    bgWc.hInstance = hInstance;
    bgWc.lpszClassName = BG_CLASS;
    bgWc.hIcon = g_hAppIcon;
    RegisterClassW(&bgWc);

    HWND hWnd = CreateWindowExW(0, BG_CLASS, L"dsnap_bg", WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    g_hMainWnd = hWnd;

    // 6. Cài đặt Low-Level Keyboard Hook
    g_hKeyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        hInstance,
        0
    );

    // 7. Message Loop tập trung duy nhất
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 8. Cleanup
    if (g_hKeyboardHook) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }
    CleanupFrozenBitmaps();
    UnregisterClassW(OVERLAY_CLASS, hInstance);
    UnregisterClassW(BORDER_CLASS, hInstance);
    UnregisterClassW(TOOLBAR_CLASS, hInstance);
    UnregisterClassW(BG_CLASS, hInstance);
    GdiplusShutdown(gdiplusToken);
    CoUninitialize();

    if (hSingleInstanceMutex) {
        ReleaseMutex(hSingleInstanceMutex);
        CloseHandle(hSingleInstanceMutex);
    }
    return 0;
}
