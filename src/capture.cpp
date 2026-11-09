#include "capture.h"
#include "config.h"

using namespace Gdiplus;

HBITMAP g_hFrozenBitmap = NULL;
HBITMAP g_hDimmedBitmap = NULL;
int     g_virtualX      = 0;
int     g_virtualY      = 0;
int     g_virtualW      = 0;
int     g_virtualH      = 0;

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
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

    // Capture entire virtual desktop
    BitBlt(hMemDC, 0, 0, g_virtualW, g_virtualH, hScreenDC, g_virtualX, g_virtualY, SRCCOPY);

    // Create Dimmed version via GDI+
    Bitmap* bmp = Bitmap::FromHBITMAP(g_hFrozenBitmap, NULL);
    if (bmp) {
        Bitmap* dimmedBmp = new Bitmap(g_virtualW, g_virtualH, PixelFormat32bppARGB);
        Graphics g(dimmedBmp);
        g.DrawImage(bmp, 0, 0, g_virtualW, g_virtualH);

        SolidBrush darkBrush(Color(100, 0, 0, 0)); // 39% dark
        g.FillRectangle(&darkBrush, 0, 0, g_virtualW, g_virtualH);

        dimmedBmp->GetHBITMAP(Color(0, 0, 0), &g_hDimmedBitmap);
        delete dimmedBmp;
        delete bmp;
    }

    SelectObject(hMemDC, oldBmp);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);

    ReleaseCapture();
    EndMenu();
}

void SaveScreenshotFromFrozen(int selX, int selY, int w, int h) {
    if (w <= 0 || h <= 0 || !g_hFrozenBitmap) return;

    HDC hScreenDC = GetDC(NULL);
    HDC hFrozenDC = CreateCompatibleDC(hScreenDC);
    HDC hCropDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hCropBitmap = CreateCompatibleBitmap(hScreenDC, w, h);
    HGDIOBJ oldFrozen = SelectObject(hFrozenDC, g_hFrozenBitmap);
    HGDIOBJ oldCrop = SelectObject(hCropDC, hCropBitmap);

    int relX = selX - g_virtualX;
    int relY = selY - g_virtualY;

    BitBlt(hCropDC, 0, 0, w, h, hFrozenDC, relX, relY, SRCCOPY);

    // Save PNG via GDI+
    std::wstring outDir = GetScreenshotsDir();
    std::wstring filePath = outDir + L"\\dsnap-screenshot-" + GetTimestampW() + L".png";

    Bitmap* gdiBitmap = new Bitmap(hCropBitmap, NULL);
    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) != -1) {
        gdiBitmap->Save(filePath.c_str(), &pngClsid, NULL);
    }
    delete gdiBitmap;

    // Copy to Clipboard
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
