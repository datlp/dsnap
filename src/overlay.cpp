#include "overlay.h"
#include "capture.h"
#include "recorder.h"
#include "i18n.h"

bool g_isOverlayActive = false;
HWND g_hWndOverlay = NULL;

static bool  g_isDragging = false;
static POINT g_ptStartScreen = { 0, 0 };
static RECT  g_selectedRectScreen = { 0, 0, 0, 0 };

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

        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
        HGDIOBJ oldMemBmp = SelectObject(hMemDC, hMemBmp);

        if (g_currentMode == MODE_PHOTO && g_hDimmedBitmap && g_hFrozenBitmap) {
            HDC hDimmedDC = CreateCompatibleDC(hdc);
            HDC hFrozenDC = CreateCompatibleDC(hdc);
            HGDIOBJ oldDimmed = SelectObject(hDimmedDC, g_hDimmedBitmap);
            HGDIOBJ oldFrozen = SelectObject(hFrozenDC, g_hFrozenBitmap);

            BitBlt(hMemDC, 0, 0, clientW, clientH, hDimmedDC, 0, 0, SRCCOPY);

            if (g_isDragging) {
                POINT p1 = { g_selectedRectScreen.left, g_selectedRectScreen.top };
                POINT p2 = { g_selectedRectScreen.right, g_selectedRectScreen.bottom };
                ScreenToClient(hWnd, &p1);
                ScreenToClient(hWnd, &p2);

                int selW = p2.x - p1.x;
                int selH = p2.y - p1.y;

                if (selW > 0 && selH > 0) {
                    BitBlt(hMemDC, p1.x, p1.y, selW, selH, hFrozenDC, p1.x, p1.y, SRCCOPY);

                    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 162, 232));
                    HGDIOBJ oldPen = SelectObject(hMemDC, hPen);
                    HGDIOBJ oldBrush = SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
                    Rectangle(hMemDC, p1.x, p1.y, p2.x, p2.y);
                    SelectObject(hMemDC, oldPen);
                    SelectObject(hMemDC, oldBrush);
                    DeleteObject(hPen);

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

        // Draw HUD hint pill at top-center when idle (not dragging)
        if (!g_isDragging) {
            const wchar_t* hint = (g_currentMode == MODE_PHOTO)
                ? Tr(StringId::OverlayGuidePhoto)
                : Tr(StringId::OverlayGuideVideo);

            HFONT hHintFont = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(hMemDC, hHintFont);

            SIZE sz;
            GetTextExtentPoint32W(hMemDC, hint, (int)wcslen(hint), &sz);

            int pillW = sz.cx + 40;
            int pillH = sz.cy + 18;
            int pillX = (clientW - pillW) / 2;
            int pillY = 32;

            HBRUSH hPillBrush = CreateSolidBrush(RGB(15, 23, 42));
            HPEN hPillPen = CreatePen(PS_SOLID, 1, RGB(71, 85, 105));
            HGDIOBJ oldPillBrush = SelectObject(hMemDC, hPillBrush);
            HGDIOBJ oldPillPen = SelectObject(hMemDC, hPillPen);

            RoundRect(hMemDC, pillX, pillY, pillX + pillW, pillY + pillH, 18, 18);

            SelectObject(hMemDC, oldPillBrush);
            SelectObject(hMemDC, oldPillPen);
            DeleteObject(hPillBrush);
            DeleteObject(hPillPen);

            SetBkMode(hMemDC, TRANSPARENT);
            SetTextColor(hMemDC, RGB(248, 250, 252));
            RECT rcText = { pillX, pillY, pillX + pillW, pillY + pillH };
            DrawTextW(hMemDC, hint, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hMemDC, oldFont);
            DeleteObject(hHintFont);
        }

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

void TriggerOverlay(Mode mode) {
    if (g_isOverlayActive && g_hWndOverlay != NULL) {
        return;
    }

    g_currentMode = mode;
    g_isOverlayActive = true;
    g_isDragging = false;

    if (mode == MODE_PHOTO) {
        CreateFrozenDesktopSnapshot();

        g_hWndOverlay = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            OVERLAY_CLASS, L"", WS_POPUP,
            g_virtualX, g_virtualY, g_virtualW, g_virtualH,
            NULL, NULL, g_hInstance, NULL
        );
    } else {
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
