#include "audio.h"
#include "config.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cstdint>

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

// PKEY_Device_FriendlyName: {a45c254e-df1c-4efd-8020-67d146a850e0}, pid 14
static const PROPERTYKEY kPKEY_Device_FriendlyName = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } },
    14
};

static std::thread g_audioThread;
static std::atomic<bool> g_stopAudio(false);
static HANDLE g_hAudioPipe = INVALID_HANDLE_VALUE;
static constexpr DWORD kMixSampleRate = 48000;
static constexpr WORD  kMixChannels   = 2;

static std::vector<AudioDeviceInfo> EnumEndpointsInternal(EDataFlow dataFlow) {
    std::vector<AudioDeviceInfo> result;

    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );
    if (FAILED(hr) || !pEnumerator) return result;

    std::wstring defaultId;
    IMMDevice* pDefaultDevice = NULL;
    if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(dataFlow, eConsole, &pDefaultDevice)) && pDefaultDevice) {
        LPWSTR pwszDefId = NULL;
        if (SUCCEEDED(pDefaultDevice->GetId(&pwszDefId)) && pwszDefId) {
            defaultId = pwszDefId;
            CoTaskMemFree(pwszDefId);
        }
        pDefaultDevice->Release();
    }

    IMMDeviceCollection* pCollection = NULL;
    if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &pCollection)) && pCollection) {
        UINT count = 0;
        pCollection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* pDevice = NULL;
            if (SUCCEEDED(pCollection->Item(i, &pDevice)) && pDevice) {
                AudioDeviceInfo info;
                info.isDefault = false;

                LPWSTR pwszId = NULL;
                if (SUCCEEDED(pDevice->GetId(&pwszId)) && pwszId) {
                    info.id = pwszId;
                    info.isDefault = (!defaultId.empty() && info.id == defaultId);
                    CoTaskMemFree(pwszId);
                }

                IPropertyStore* pProps = NULL;
                if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps)) && pProps) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    if (SUCCEEDED(pProps->GetValue(kPKEY_Device_FriendlyName, &varName))) {
                        if (varName.vt == VT_LPWSTR && varName.pwszVal) {
                            info.name = varName.pwszVal;
                        }
                    }
                    PropVariantClear(&varName);
                    pProps->Release();
                }

                if (!info.id.empty()) {
                    if (info.name.empty()) {
                        info.name = (dataFlow == eRender) ? L"Audio Output Device" : L"Microphone";
                    }
                    result.push_back(info);
                }
                pDevice->Release();
            }
        }
        pCollection->Release();
    }

    pEnumerator->Release();
    return result;
}

std::vector<AudioDeviceInfo> EnumAudioOutputDevices() {
    return EnumEndpointsInternal(eRender);
}

std::vector<AudioDeviceInfo> EnumAudioInputDevices() {
    return EnumEndpointsInternal(eCapture);
}

struct ActiveStream {
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioCaptureClient* pCaptureClient = NULL;
    WORD channels = 2;
    WORD bitsPerSample = 32;
    bool isFloat = true;
    bool isLoopback = false;
    std::vector<float> fifo; // interleaved L, R stereo float32 samples
};

static bool InitStreamForDevice(IMMDevice* pDevice, bool isLoopback, ActiveStream& outStream) {
    if (!pDevice) return false;

    IAudioClient* pAudioClient = NULL;
    HRESULT hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr) || !pAudioClient) {
        return false;
    }

    // Request unified 48000 Hz, 2-channel, 32-bit IEEE Float format with WASAPI auto-resampler
    WAVEFORMATEX wfx = { 0 };
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = kMixChannels;
    wfx.nSamplesPerSec = kMixSampleRate;
    wfx.wBitsPerSample = 32;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    DWORD streamFlags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    if (isLoopback) {
        streamFlags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    }

    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        streamFlags,
        10000000, // 1 second buffer
        0,
        &wfx,
        NULL
    );

    if (FAILED(hr)) {
        // Fallback: try 1-channel (mono) 48000 Hz float32 and upmix to stereo in our mixer
        wfx.nChannels = 1;
        wfx.nBlockAlign = 4;
        wfx.nAvgBytesPerSec = kMixSampleRate * 4;
        hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            streamFlags,
            10000000,
            0,
            &wfx,
            NULL
        );
    }

    if (FAILED(hr)) {
        pAudioClient->Release();
        return false;
    }

    IAudioCaptureClient* pCaptureClient = NULL;
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr) || !pCaptureClient) {
        pAudioClient->Release();
        return false;
    }

    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        pCaptureClient->Release();
        pAudioClient->Release();
        return false;
    }

    pDevice->AddRef();
    outStream.pDevice = pDevice;
    outStream.pAudioClient = pAudioClient;
    outStream.pCaptureClient = pCaptureClient;
    outStream.channels = wfx.nChannels;
    outStream.bitsPerSample = wfx.wBitsPerSample;
    outStream.isFloat = true;
    outStream.isLoopback = isLoopback;
    outStream.fifo.reserve(kMixSampleRate * 2);
    return true;
}

static void CleanupStream(ActiveStream& s) {
    if (s.pAudioClient) {
        s.pAudioClient->Stop();
    }
    if (s.pCaptureClient) {
        s.pCaptureClient->Release();
        s.pCaptureClient = NULL;
    }
    if (s.pAudioClient) {
        s.pAudioClient->Release();
        s.pAudioClient = NULL;
    }
    if (s.pDevice) {
        s.pDevice->Release();
        s.pDevice = NULL;
    }
}

static void DrainStreamPackets(ActiveStream& s) {
    if (!s.pCaptureClient) return;

    UINT32 packetLength = 0;
    HRESULT hr = s.pCaptureClient->GetNextPacketSize(&packetLength);
    if (FAILED(hr)) return;

    while (packetLength > 0 && !g_stopAudio.load()) {
        BYTE* pData = NULL;
        UINT32 numFrames = 0;
        DWORD flags = 0;
        hr = s.pCaptureClient->GetBuffer(&pData, &numFrames, &flags, NULL, NULL);
        if (FAILED(hr)) break;

        if (numFrames > 0) {
            bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || (pData == NULL);
            if (silent) {
                s.fifo.insert(s.fifo.end(), (size_t)numFrames * 2, 0.0f);
            } else if (s.channels == 2) {
                const float* pFloat = reinterpret_cast<const float*>(pData);
                s.fifo.insert(s.fifo.end(), pFloat, pFloat + ((size_t)numFrames * 2));
            } else if (s.channels == 1) {
                const float* pFloat = reinterpret_cast<const float*>(pData);
                for (UINT32 i = 0; i < numFrames; ++i) {
                    float sample = pFloat[i];
                    s.fifo.push_back(sample);
                    s.fifo.push_back(sample);
                }
            }
        }

        s.pCaptureClient->ReleaseBuffer(numFrames);

        // Cap FIFO at 250ms (12000 stereo frames = 24000 floats) to avoid latency buildup
        const size_t maxFloats = (kMixSampleRate / 4) * 2;
        if (s.fifo.size() > maxFloats) {
            size_t excess = s.fifo.size() - maxFloats;
            s.fifo.erase(s.fifo.begin(), s.fifo.begin() + excess);
        }

        if (FAILED(s.pCaptureClient->GetNextPacketSize(&packetLength))) {
            break;
        }
    }
}

static void AudioLoopbackWorker(std::wstring pipeName) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    g_hAudioPipe = CreateNamedPipeW(
        pipeName.c_str(),
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1, 65536, 65536, 0, NULL
    );

    if (g_hAudioPipe == INVALID_HANDLE_VALUE) {
        CoUninitialize();
        return;
    }

    // Wait for FFmpeg to connect to the named pipe
    BOOL connected = ConnectNamedPipe(g_hAudioPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!connected) {
        CloseHandle(g_hAudioPipe);
        g_hAudioPipe = INVALID_HANDLE_VALUE;
        CoUninitialize();
        return;
    }

    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );

    std::vector<ActiveStream> streams;

    if (SUCCEEDED(hr) && pEnumerator) {
        // 1. Initialize selected Output (Loopback) devices if g_recordAudio is enabled
        if (g_recordAudio) {
            bool addedOutput = false;
            for (const auto& id : g_selectedOutputDevices) {
                IMMDevice* pDev = NULL;
                if (SUCCEEDED(pEnumerator->GetDevice(id.c_str(), &pDev)) && pDev) {
                    DWORD state = 0;
                    if (SUCCEEDED(pDev->GetState(&state)) && (state & DEVICE_STATE_ACTIVE)) {
                        ActiveStream st;
                        if (InitStreamForDevice(pDev, true, st)) {
                            streams.push_back(std::move(st));
                            addedOutput = true;
                        }
                    }
                    pDev->Release();
                }
            }
            if (!addedOutput) {
                IMMDevice* pDefOut = NULL;
                if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefOut)) && pDefOut) {
                    ActiveStream st;
                    if (InitStreamForDevice(pDefOut, true, st)) {
                        streams.push_back(std::move(st));
                    }
                    pDefOut->Release();
                }
            }
        }

        // 2. Initialize selected Input (Microphone) devices if g_recordMic is enabled
        if (g_recordMic) {
            bool addedInput = false;
            for (const auto& id : g_selectedInputDevices) {
                IMMDevice* pDev = NULL;
                if (SUCCEEDED(pEnumerator->GetDevice(id.c_str(), &pDev)) && pDev) {
                    DWORD state = 0;
                    if (SUCCEEDED(pDev->GetState(&state)) && (state & DEVICE_STATE_ACTIVE)) {
                        ActiveStream st;
                        if (InitStreamForDevice(pDev, false, st)) {
                            streams.push_back(std::move(st));
                            addedInput = true;
                        }
                    }
                    pDev->Release();
                }
            }
            if (!addedInput) {
                IMMDevice* pDefIn = NULL;
                if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDefIn)) && pDefIn) {
                    ActiveStream st;
                    if (InitStreamForDevice(pDefIn, false, st)) {
                        streams.push_back(std::move(st));
                    }
                    pDefIn->Release();
                }
            }
        }
    }

    LARGE_INTEGER qpcFreq, qpcStart;
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcStart);
    uint64_t emittedFrames = 0;
    const uint64_t chunkFrames = kMixSampleRate / 100; // 10ms = 480 frames

    std::vector<float> mixBuf;
    mixBuf.reserve(chunkFrames * 4 * kMixChannels);

    while (!g_stopAudio.load()) {
        for (auto& st : streams) {
            DrainStreamPackets(st);
        }

        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);
        uint64_t elapsedTicks = (uint64_t)(qpcNow.QuadPart - qpcStart.QuadPart);
        uint64_t targetFrames = (elapsedTicks * (uint64_t)kMixSampleRate) / (uint64_t)qpcFreq.QuadPart;

        if (targetFrames >= emittedFrames + chunkFrames) {
            uint64_t framesToEmit = targetFrames - emittedFrames;
            // Cap single write burst to 100ms in case process was paused
            if (framesToEmit > kMixSampleRate / 10) {
                emittedFrames = targetFrames - chunkFrames;
                framesToEmit = chunkFrames;
            }

            size_t totalFloats = (size_t)(framesToEmit * kMixChannels);
            mixBuf.assign(totalFloats, 0.0f);

            for (auto& st : streams) {
                size_t availFloats = (std::min)(totalFloats, st.fifo.size());
                for (size_t i = 0; i < availFloats; ++i) {
                    mixBuf[i] += st.fifo[i];
                }
                if (availFloats > 0) {
                    st.fifo.erase(st.fifo.begin(), st.fifo.begin() + availFloats);
                }
            }

            // Soft-clip mixed output to [-1.0f, 1.0f]
            for (size_t i = 0; i < totalFloats; ++i) {
                if (mixBuf[i] > 1.0f) mixBuf[i] = 1.0f;
                else if (mixBuf[i] < -1.0f) mixBuf[i] = -1.0f;
            }

            DWORD bytesToWrite = (DWORD)(totalFloats * sizeof(float));
            DWORD written = 0;
            if (!WriteFile(g_hAudioPipe, mixBuf.data(), bytesToWrite, &written, NULL)) {
                break;
            }

            emittedFrames += framesToEmit;
        }

        Sleep(4);
    }

    for (auto& st : streams) {
        CleanupStream(st);
    }

    if (pEnumerator) {
        pEnumerator->Release();
    }

    if (g_hAudioPipe != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_hAudioPipe);
        DisconnectNamedPipe(g_hAudioPipe);
        CloseHandle(g_hAudioPipe);
        g_hAudioPipe = INVALID_HANDLE_VALUE;
    }

    CoUninitialize();
}

bool StartSystemAudioLoopback(std::wstring& outPipeName, DWORD& outSampleRate, WORD& outChannels) {
    StopSystemAudioLoopback();

    g_stopAudio = false;
    outPipeName = L"\\\\.\\pipe\\dsnap_audio_" + std::to_wstring(GetCurrentProcessId());
    outSampleRate = kMixSampleRate;
    outChannels = kMixChannels;

    g_audioThread = std::thread(AudioLoopbackWorker, outPipeName);
    return true;
}

void StopSystemAudioLoopback() {
    g_stopAudio = true;
    if (g_audioThread.joinable()) {
        g_audioThread.join();
    }
}
