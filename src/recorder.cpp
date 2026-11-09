#include "recorder.h"
#include "config.h"
#include "audio.h"
#include "tray.h"

typedef LONG(NTAPI* PFN_NtSuspendProcess)(HANDLE ProcessHandle);
typedef LONG(NTAPI* PFN_NtResumeProcess)(HANDLE ProcessHandle);
static PFN_NtSuspendProcess pfnNtSuspendProcess = NULL;
static PFN_NtResumeProcess  pfnNtResumeProcess  = NULL;

static HANDLE g_hJobObject = NULL;

static void EnsureJobObject() {
    if (!g_hJobObject) {
        g_hJobObject = CreateJobObjectW(NULL, NULL);
        if (g_hJobObject) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(g_hJobObject, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }
    }
}

HANDLE g_hFFmpegStdinWrite = NULL;
PROCESS_INFORMATION g_ffmpegPi = { 0 };
bool g_isRecording = false;
bool g_isPaused = false;
int  g_recordingSeconds = 0;
std::wstring g_currentRecordingPath = L"";

HWND g_hWndRecordingBorder = NULL;
HWND g_hWndRecordingToolbar = NULL;
int  g_recX = 0, g_recY = 0, g_recW = 0, g_recH = 0;
static int g_hoverButton = 0;

void InitRecorderNTDLL() {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        pfnNtSuspendProcess = (PFN_NtSuspendProcess)GetProcAddress(hNtdll, "NtSuspendProcess");
        pfnNtResumeProcess  = (PFN_NtResumeProcess)GetProcAddress(hNtdll, "NtResumeProcess");
    }
}

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

void StartRecording(int x, int y, int w, int h) {
    w -= (w % 2);
    h -= (h % 2);
    if (w <= 0 || h <= 0) return;

    if (!EnsureFFmpegAvailable(g_hMainWnd)) {
        return;
    }
    std::wstring ffmpegPath = FindFFmpegPath();
    if (ffmpegPath.empty()) {
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
        std::wstring audioPipe;
        DWORD sampleRate = 48000;
        WORD channels = 2;
        if (StartSystemAudioLoopback(audioPipe, sampleRate, channels)) {
            cmd += L" -f f32le -ar " + std::to_wstring(sampleRate) +
                   L" -ac " + std::to_wstring(channels) +
                   L" -i \"" + audioPipe + L"\" -c:a aac -b:a 192k";
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
        EnsureJobObject();
        if (g_hJobObject) {
            AssignProcessToJobObject(g_hJobObject, g_ffmpegPi.hProcess);
        }

        g_isRecording = true;
        g_isPaused = false;
        g_recordingSeconds = 0;
        g_recX = x; g_recY = y; g_recW = w; g_recH = h;

        UpdateTrayTip(L"dsnap (Recording screen - Press Shift+PrtScn to stop)");

        // 1. Create Recording Highlight Border
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

        // 2. Create Floating Recording Toolbar in top-right corner
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
        if (g_recordAudio) {
            StopSystemAudioLoopback();
        }
        CloseHandle(g_hFFmpegStdinWrite);
        g_hFFmpegStdinWrite = NULL;
        MessageBoxW(NULL, L"Failed to start FFmpeg process!", L"dsnap - Error", MB_ICONERROR);
    }
}

void TogglePauseRecording() {
    if (!g_isRecording || !g_ffmpegPi.hProcess) return;

    if (!g_isPaused) {
        if (pfnNtSuspendProcess) pfnNtSuspendProcess(g_ffmpegPi.hProcess);
        g_isPaused = true;
        UpdateTrayTip(L"dsnap (Recording Paused)");
    } else {
        if (pfnNtResumeProcess) pfnNtResumeProcess(g_ffmpegPi.hProcess);
        g_isPaused = false;
        UpdateTrayTip(L"dsnap (Recording screen - Press Shift+PrtScn to stop)");
    }

    if (g_hWndRecordingBorder) InvalidateRect(g_hWndRecordingBorder, NULL, TRUE);
    if (g_hWndRecordingToolbar) InvalidateRect(g_hWndRecordingToolbar, NULL, TRUE);
}

void StopRecording(bool cancel) {
    if (g_isRecording) {
        if (g_recordAudio) {
            StopSystemAudioLoopback();
        }

        if (g_isPaused && pfnNtResumeProcess && g_ffmpegPi.hProcess) {
            pfnNtResumeProcess(g_ffmpegPi.hProcess);
            g_isPaused = false;
            Sleep(50); // Ensure threads resume execution before sending signals
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
                Sleep(100);
                DeleteFileW(g_currentRecordingPath.c_str());
            }
        } else {
            if (g_hFFmpegStdinWrite != NULL) {
                DWORD bytesWritten;
                const char* q = "q\n";
                WriteFile(g_hFFmpegStdinWrite, q, (DWORD)strlen(q), &bytesWritten, NULL);

                // Immediately close write handle to send EOF down the pipe
                CloseHandle(g_hFFmpegStdinWrite);
                g_hFFmpegStdinWrite = NULL;
            }

            if (g_ffmpegPi.hProcess) {
                DWORD waitRes = WaitForSingleObject(g_ffmpegPi.hProcess, 4000);
                if (waitRes == WAIT_TIMEOUT) {
                    // Force terminate if FFmpeg hung or failed to finalize within 4 seconds
                    TerminateProcess(g_ffmpegPi.hProcess, 0);
                    WaitForSingleObject(g_ffmpegPi.hProcess, 500);
                }
                CloseHandle(g_ffmpegPi.hProcess);
                CloseHandle(g_ffmpegPi.hThread);
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

        UpdateTrayTip(L"dsnap (Ready)");
    }
}
