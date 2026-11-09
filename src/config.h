#pragma once
#include "common.h"

extern bool g_recordAudio;

bool IsAutoStartEnabled();
void SetAutoStart(bool enable);

bool LoadAudioSetting();
void SaveAudioSetting(bool enable);

std::wstring GetScreenshotsDir();
std::wstring GetCapturesDir();
std::wstring FindFFmpegPath();
std::wstring GetDsnapDirectory();
std::wstring GetTimestampW();

// Checks if FFmpeg is available; if not, prompts user to auto-download via GitHub CDN
bool EnsureFFmpegAvailable(HWND hWndParent);
bool IsFFmpegDownloading();
