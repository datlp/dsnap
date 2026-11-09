#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <initguid.h>
#include <knownfolders.h>
#include <gdiplus.h>
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

using namespace Gdiplus;

// Identifiers
#define IDI_APPICON       101
#define WM_TRAYICON       (WM_USER + 1)
#define WM_DSNAP_TRIGGER  (WM_USER + 100)
#define IDM_AUTOSTART     1001
#define IDM_EXIT          1002
#define TRIGGER_PHOTO     1
#define TRIGGER_VIDEO     2

const wchar_t APP_NAME[]      = L"dsnap";
const wchar_t REG_KEY[]       = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t BG_CLASS[]      = L"DsnapBackground";
const wchar_t OVERLAY_CLASS[] = L"DsnapOverlay";

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

// Overlay state
enum Mode { MODE_NONE, MODE_PHOTO, MODE_VIDEO };
Mode g_currentMode = MODE_NONE;
bool g_isOverlayActive = false;
HWND g_hWndOverlay = NULL;
bool g_isDragging = false;
POINT g_ptStartScreen = { 0, 0 };
RECT g_selectedRectScreen = { 0, 0, 0, 0 };

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

// Save Screenshot
void SaveScreenshot(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, w, h);
    HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);
    BitBlt(hMemoryDC, 0, 0, w, h, hScreenDC, x, y, SRCCOPY);

    // Save PNG using GDI+
    std::wstring outDir = GetScreenshotsDir();
    std::wstring filePath = outDir + L"\\dsnap-screenshot-" + GetTimestampW() + L".png";

    Bitmap* gdiBitmap = new Bitmap(hBitmap, NULL);
    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) != -1) {
        gdiBitmap->Save(filePath.c_str(), &pngClsid, NULL);
    }
    delete gdiBitmap;

    // Copy to Clipboard (System takes ownership of hBitmap upon success)
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hBitmap);
        CloseClipboard();
    } else {
        DeleteObject(hBitmap);
    }

    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    MessageBeep(MB_OK);
}

// Start Recording via FFmpeg
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
    std::wstring outputPath = outDir + L"\\dsnap-recording-" + GetTimestampW() + L".mp4";

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
        L" -i desktop -c:v libx264 -preset ultrafast -crf 23 -pix_fmt yuv420p \"" +
        outputPath + L"\"";

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
        UpdateTrayTip(L"dsnap (Đang quay video - Nhấn Shift+PrtScn để lưu)");
        MessageBeep(MB_ICONASTERISK);
    } else {
        CloseHandle(g_hFFmpegStdinWrite);
        g_hFFmpegStdinWrite = NULL;
        MessageBoxW(NULL, L"Không thể khởi chạy tiến trình FFmpeg!", L"dsnap - Lỗi", MB_ICONERROR);
    }
}

// Stop Recording Gracefully
void StopRecording() {
    if (g_isRecording && g_hFFmpegStdinWrite != NULL) {
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
        ZeroMemory(&g_ffmpegPi, sizeof(g_ffmpegPi));
        g_isRecording = false;

        UpdateTrayTip(L"dsnap (Sẵn sàng)");
        MessageBeep(MB_OK);
    }
}

// Overlay Window Proc (Được xử lý bởi luồng chính, KHÔNG có vòng lặp GetMessage riêng)
LRESULT CALLBACK OverlayProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_LBUTTONDOWN: {
        g_isDragging = true;
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
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_isDragging) {
            g_isDragging = false;
            int x = g_selectedRectScreen.left;
            int y = g_selectedRectScreen.top;
            int w = g_selectedRectScreen.right - g_selectedRectScreen.left;
            int h = g_selectedRectScreen.bottom - g_selectedRectScreen.top;
            Mode m = g_currentMode;

            // Đóng overlay ngay lập tức trước khi chụp/quay
            DestroyWindow(hWnd);

            if (w > 10 && h > 10) {
                if (m == MODE_PHOTO) {
                    SaveScreenshot(x, y, w, h);
                } else if (m == MODE_VIDEO) {
                    StartRecording(x, y, w, h);
                }
            }
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            g_isDragging = false;
            DestroyWindow(hWnd);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        if (g_isDragging) {
            COLORREF borderColor = (g_currentMode == MODE_PHOTO) ? RGB(0, 162, 232) : RGB(255, 50, 50);
            HPEN hPen = CreatePen(PS_SOLID, 2, borderColor);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

            // Chuyển tọa độ màn hình sang tọa độ client của Overlay
            POINT p1 = { g_selectedRectScreen.left, g_selectedRectScreen.top };
            POINT p2 = { g_selectedRectScreen.right, g_selectedRectScreen.bottom };
            ScreenToClient(hWnd, &p1);
            ScreenToClient(hWnd, &p2);

            Rectangle(hdc, p1.x, p1.y, p2.x, p2.y);

            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(hPen);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DESTROY: {
        g_hWndOverlay = NULL;
        g_isOverlayActive = false;
        g_currentMode = MODE_NONE;
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Kích hoạt Overlay an toàn (Chống chồng lấn nhiều cửa sổ làm tối màn hình)
void TriggerOverlay(Mode mode) {
    // Nếu đang có một Overlay mở sẵn, tuyệt đối KHÔNG tạo thêm cửa sổ thứ 2!
    if (g_isOverlayActive && g_hWndOverlay != NULL) {
        return;
    }

    // Đóng bất kỳ context menu nào đang hiển thị
    EndMenu();

    g_currentMode = mode;
    g_isOverlayActive = true;
    g_isDragging = false;

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
        ShowWindow(g_hWndOverlay, SW_SHOW);
        SetForegroundWindow(g_hWndOverlay);
        UpdateWindow(g_hWndOverlay);
    } else {
        g_isOverlayActive = false;
    }
}

// Show System Tray Context Menu
void ShowContextMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    UINT flag = IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(hMenu, MF_STRING | flag, IDM_AUTOSTART, L"Start with Windows");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    // Quy chuẩn Microsoft cho System Tray Context Menu:
    // Phải SetForegroundWindow trước và PostMessage(WM_NULL) sau TrackPopupMenu
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
        if (LOWORD(wParam) == IDM_AUTOSTART) {
            SetAutoStart(!IsAutoStartEnabled());
        } else if (LOWORD(wParam) == IDM_EXIT) {
            DestroyWindow(hWnd);
        }
        return 0;
    }
    case WM_DSNAP_TRIGGER: {
        if (wParam == TRIGGER_PHOTO) {
            TriggerOverlay(MODE_PHOTO);
        } else if (wParam == TRIGGER_VIDEO) {
            if (g_isRecording) {
                StopRecording();
            } else {
                TriggerOverlay(MODE_VIDEO);
            }
        }
        return 0;
    }
    case WM_DESTROY: {
        if (g_isRecording) StopRecording();
        if (g_isOverlayActive && g_hWndOverlay) {
            DestroyWindow(g_hWndOverlay);
        }
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
                // Đóng menu nếu đang mở
                EndMenu();

                bool isShiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

                // Nếu đang mở overlay, ấn lại PrintScreen sẽ không làm gì (tránh chồng mờ)
                if (g_isOverlayActive) {
                    return 1;
                }

                if (isShiftPressed) {
                    PostMessageW(g_hMainWnd, WM_DSNAP_TRIGGER, TRIGGER_VIDEO, 0);
                } else {
                    PostMessageW(g_hMainWnd, WM_DSNAP_TRIGGER, TRIGGER_PHOTO, 0);
                }
                return 1; // Nuốt phím để chiếm quyền hoàn toàn
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

// WinMain Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInstance = hInstance;

    // 1. Khởi tạo COM & Per-Monitor DPI Awareness
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    EnableDpiAwareness();

    // 2. Load Icon
    g_hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));

    // 3. Khởi tạo GDI+
    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 4. Đăng ký Window Class cho Overlay (Đăng ký 1 lần duy nhất tại khởi động)
    WNDCLASSW overlayWc = { 0 };
    overlayWc.lpfnWndProc = OverlayProc;
    overlayWc.hInstance = hInstance;
    overlayWc.lpszClassName = OVERLAY_CLASS;
    overlayWc.hCursor = LoadCursor(NULL, IDC_CROSS);
    overlayWc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&overlayWc);

    // 5. Đăng ký & Tạo Background Window (Cửa sổ thường không hiển thị, cho phép SetForegroundWindow)
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
    UnregisterClassW(OVERLAY_CLASS, hInstance);
    UnregisterClassW(BG_CLASS, hInstance);
    GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return 0;
}
