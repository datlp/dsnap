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
std::wstring GetTimestampW();
