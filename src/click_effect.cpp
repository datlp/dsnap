#include "click_effect.h"
#include <vector>
#include <mutex>

#define RIPPLE_CLASS L"DsnapClickRippleWindow"
#define TIMER_RIPPLE_ANIM 2001
#define RIPPLE_DURATION_MS 280

struct ClickRipple {
    int x;
    int y;
    bool isRightClick;
    ULONGLONG startTick;
};

static HWND s_hOverlayWnd = NULL;
static HHOOK s_hMouseHook = NULL;
static bool s_enabled = false;
static std::vector<ClickRipple> s_ripples;
static std::mutex s_mutex;

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_enabled && s_hOverlayWnd) {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN) {
            MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            bool isRight = (wParam == WM_RBUTTONDOWN);
            
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                ClickRipple r;
                r.x = pMouse->pt.x;
                r.y = pMouse->pt.y;
                r.isRightClick = isRight;
                r.startTick = GetTickCount64();
                s_ripples.push_back(r);
            }

            PostMessageW(s_hOverlayWnd, WM_USER + 101, 0, 0);
        }
    }
    return CallNextHookEx(s_hMouseHook, nCode, wParam, lParam);
}

static void RenderRipples(HWND hWnd) {
    ULONGLONG now = GetTickCount64();
    std::vector<ClickRipple> activeRipples;

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (const auto& r : s_ripples) {
            if (now - r.startTick < RIPPLE_DURATION_MS) {
                activeRipples.push_back(r);
            }
        }
        s_ripples = activeRipples;
    }

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (activeRipples.empty()) {
        KillTimer(hWnd, TIMER_RIPPLE_ANIM);
        ShowWindow(hWnd, SW_HIDE);
        return;
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = vw;
    bmi.bmiHeader.biHeight = -vh; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

    // Render using GDI+
    {
        Gdiplus::Graphics g(hdcMem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        for (const auto& r : activeRipples) {
            float progress = static_cast<float>(now - r.startTick) / RIPPLE_DURATION_MS;
            if (progress > 1.0f) progress = 1.0f;

            // Ease out quad: 1 - (1 - p)^2
            float ease = 1.0f - (1.0f - progress) * (1.0f - progress);

            float minRadius = 8.0f;
            float maxRadius = r.isRightClick ? 36.0f : 44.0f;
            float radius = minRadius + (maxRadius - minRadius) * ease;

            BYTE alpha = static_cast<BYTE>((1.0f - progress) * 220);

            // Screen to virtual window local coords
            float lx = static_cast<float>(r.x - vx);
            float ly = static_cast<float>(r.y - vy);

            if (r.isRightClick) {
                // Coral/Red accent for Right Click
                Gdiplus::Pen outerPen(Gdiplus::Color(alpha, 255, 69, 58), 3.5f);
                g.DrawEllipse(&outerPen, lx - radius, ly - radius, radius * 2.0f, radius * 2.0f);

                Gdiplus::SolidBrush dotBrush(Gdiplus::Color(static_cast<BYTE>(alpha * 0.7f), 255, 100, 90));
                float dotRadius = 4.0f * (1.0f - progress);
                g.FillEllipse(&dotBrush, lx - dotRadius, ly - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            } else {
                // Electric Cyan accent for Left Click
                Gdiplus::Pen outerPen(Gdiplus::Color(alpha, 0, 229, 255), 3.5f);
                g.DrawEllipse(&outerPen, lx - radius, ly - radius, radius * 2.0f, radius * 2.0f);

                Gdiplus::SolidBrush dotBrush(Gdiplus::Color(static_cast<BYTE>(alpha * 0.7f), 130, 240, 255));
                float dotRadius = 5.0f * (1.0f - progress);
                g.FillEllipse(&dotBrush, lx - dotRadius, ly - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            }
        }
    }

    POINT ptSrc = { 0, 0 };
    POINT ptDst = { vx, vy };
    SIZE sizeWnd = { vw, vh };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

static LRESULT CALLBACK RippleWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_USER + 101: {
        ShowWindow(hWnd, SW_SHOWNOACTIVATE);
        SetTimer(hWnd, TIMER_RIPPLE_ANIM, 16, NULL);
        RenderRipples(hWnd);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == TIMER_RIPPLE_ANIM) {
            RenderRipples(hWnd);
        }
        return 0;
    }
    case WM_DESTROY: {
        KillTimer(hWnd, TIMER_RIPPLE_ANIM);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void InitClickEffect(HINSTANCE hInstance) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = RippleWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = RIPPLE_CLASS;
    RegisterClassExW(&wc);

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    s_hOverlayWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        RIPPLE_CLASS,
        L"",
        WS_POPUP,
        vx, vy, vw, vh,
        NULL, NULL, hInstance, NULL
    );
}

void EnableClickEffect(bool enable) {
    s_enabled = enable;
    if (enable) {
        if (!s_hMouseHook) {
            s_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, g_hInstance, 0);
        }
    } else {
        if (s_hMouseHook) {
            UnhookWindowsHookEx(s_hMouseHook);
            s_hMouseHook = NULL;
        }
        if (s_hOverlayWnd) {
            KillTimer(s_hOverlayWnd, TIMER_RIPPLE_ANIM);
            ShowWindow(s_hOverlayWnd, SW_HIDE);
        }
        std::lock_guard<std::mutex> lock(s_mutex);
        s_ripples.clear();
    }
}

bool IsClickEffectEnabled() {
    return s_enabled;
}

void ShutdownClickEffect() {
    EnableClickEffect(false);
    if (s_hOverlayWnd) {
        DestroyWindow(s_hOverlayWnd);
        s_hOverlayWnd = NULL;
    }
    UnregisterClassW(RIPPLE_CLASS, g_hInstance);
}
