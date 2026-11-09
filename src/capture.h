#pragma once
#include "common.h"

extern HBITMAP g_hFrozenBitmap;
extern HBITMAP g_hDimmedBitmap;
extern int     g_virtualX;
extern int     g_virtualY;
extern int     g_virtualW;
extern int     g_virtualH;

void CreateFrozenDesktopSnapshot();
void SaveScreenshotFromFrozen(int selX, int selY, int w, int h);
void CleanupFrozenBitmaps();
