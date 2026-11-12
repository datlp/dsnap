#pragma once
#include "common.h"
#include "i18n.h"

extern bool g_recordAudio;
extern bool g_highlightClicks;

bool IsAutoStartEnabled();
void SetAutoStart(bool enable);

bool IsProcessElevated();
bool IsRunAsAdminConfigured();
bool IsAdminAutoStartTaskEnabled();
bool SetAdminAutoStartTask(bool enable);

bool LoadAudioSetting();
void SaveAudioSetting(bool enable);

bool LoadClickEffectSetting();
void SaveClickEffectSetting(bool enable);

Language LoadLanguageSetting();
void SaveLanguageSetting(Language lang);

std::wstring GetScreenshotsDir();
std::wstring GetCapturesDir();
std::wstring FindFFmpegPath();
std::wstring GetDsnapDirectory();
std::wstring GetTimestampW();

bool IsPortableMode();
std::wstring GetIniFilePath();

// Checks if FFmpeg is available; if not, prompts user to auto-download via GitHub CDN
bool EnsureFFmpegAvailable(HWND hWndParent);
bool IsFFmpegDownloading();
