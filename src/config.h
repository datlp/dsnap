#pragma once
#include "common.h"
#include "i18n.h"

extern bool g_recordAudio;
extern bool g_recordMic;
extern bool g_highlightClicks;
extern std::vector<std::wstring> g_selectedOutputDevices;
extern std::vector<std::wstring> g_selectedInputDevices;

bool IsAutoStartEnabled();
void SetAutoStart(bool enable);

bool IsProcessElevated();
bool IsRunAsAdminConfigured();
bool IsAdminAutoStartTaskEnabled();
bool SetAdminAutoStartTask(bool enable);

bool LoadAudioSetting();
void SaveAudioSetting(bool enable);

bool LoadMicSetting();
void SaveMicSetting(bool enable);

void LoadAudioDevicesSetting();
void SaveAudioDevicesSetting();
bool IsOutputDeviceSelected(const std::wstring& id);
void ToggleOutputDeviceSelection(const std::wstring& id);
void ClearOutputDeviceSelection();
bool IsInputDeviceSelected(const std::wstring& id);
void ToggleInputDeviceSelection(const std::wstring& id);
void ClearInputDeviceSelection();

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
