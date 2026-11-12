#pragma once
#include "common.h"

struct AudioDeviceInfo {
    std::wstring id;
    std::wstring name;
    bool isDefault;
};

// Enumerate active output (speakers/headphones) or input (microphone) endpoints
std::vector<AudioDeviceInfo> EnumAudioOutputDevices();
std::vector<AudioDeviceInfo> EnumAudioInputDevices();

// Start multi-stream WASAPI capture & mixer (System Output Loopback + Microphones)
// Returns true if audio thread and named pipe were initialized successfully.
bool StartSystemAudioLoopback(std::wstring& outPipeName, DWORD& outSampleRate, WORD& outChannels);

// Stop WASAPI capture, finalize stream, and close audio pipe
void StopSystemAudioLoopback();
