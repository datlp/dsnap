#pragma once
#include "common.h"

// Start WASAPI Loopback capture for system audio output (Youtube, games, apps)
// Returns true if audio loopback thread and named pipe were initialized successfully.
bool StartSystemAudioLoopback(std::wstring& outPipeName, DWORD& outSampleRate, WORD& outChannels);

// Stop WASAPI Loopback capture, finalize stream, and close audio pipe
void StopSystemAudioLoopback();
