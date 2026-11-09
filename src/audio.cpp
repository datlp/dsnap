#include "audio.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>
#include <atomic>
#include <vector>

static std::thread g_audioThread;
static std::atomic<bool> g_stopAudio(false);
static HANDLE g_hAudioPipe = INVALID_HANDLE_VALUE;
static DWORD g_audioSampleRate = 48000;
static WORD  g_audioChannels = 2;

static void AudioLoopbackWorker(std::wstring pipeName) {
    CoInitialize(NULL);

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

    IMMDeviceEnumerator *pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );
    if (FAILED(hr)) {
        CloseHandle(g_hAudioPipe);
        g_hAudioPipe = INVALID_HANDLE_VALUE;
        CoUninitialize();
        return;
    }

    IMMDevice *pDevice = NULL;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        pEnumerator->Release();
        CloseHandle(g_hAudioPipe);
        g_hAudioPipe = INVALID_HANDLE_VALUE;
        CoUninitialize();
        return;
    }

    IAudioClient *pAudioClient = NULL;
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) {
        pDevice->Release();
        pEnumerator->Release();
        CloseHandle(g_hAudioPipe);
        g_hAudioPipe = INVALID_HANDLE_VALUE;
        CoUninitialize();
        return;
    }

    WAVEFORMATEX *pwfx = NULL;
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        CloseHandle(g_hAudioPipe);
        g_hAudioPipe = INVALID_HANDLE_VALUE;
        CoUninitialize();
        return;
    }

    g_audioSampleRate = pwfx->nSamplesPerSec;
    g_audioChannels = pwfx->nChannels;

    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, pwfx, NULL);
    if (FAILED(hr)) {
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        CloseHandle(g_hAudioPipe);
        g_hAudioPipe = INVALID_HANDLE_VALUE;
        CoUninitialize();
        return;
    }

    IAudioCaptureClient *pCaptureClient = NULL;
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) {
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        CloseHandle(g_hAudioPipe);
        g_hAudioPipe = INVALID_HANDLE_VALUE;
        CoUninitialize();
        return;
    }

    hr = pAudioClient->Start();

    int blockAlign = pwfx->nBlockAlign;
    int silentChunkFrames = pwfx->nSamplesPerSec / 100; // 10ms chunk
    std::vector<BYTE> silentBuf(blockAlign * silentChunkFrames, 0);

    while (!g_stopAudio.load()) {
        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;

        if (packetLength == 0) {
            // Stream silence when nothing is currently playing to maintain A/V synchronization
            DWORD written = 0;
            WriteFile(g_hAudioPipe, silentBuf.data(), (DWORD)silentBuf.size(), &written, NULL);
            Sleep(10);
            continue;
        }

        while (packetLength > 0 && !g_stopAudio.load()) {
            BYTE *pData = NULL;
            UINT32 numFrames = 0;
            DWORD flags = 0;
            hr = pCaptureClient->GetBuffer(&pData, &numFrames, &flags, NULL, NULL);
            if (SUCCEEDED(hr)) {
                DWORD bytesToWrite = numFrames * blockAlign;
                DWORD written = 0;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::vector<BYTE> zeros(bytesToWrite, 0);
                    WriteFile(g_hAudioPipe, zeros.data(), bytesToWrite, &written, NULL);
                } else {
                    WriteFile(g_hAudioPipe, pData, bytesToWrite, &written, NULL);
                }
                pCaptureClient->ReleaseBuffer(numFrames);
            }
            pCaptureClient->GetNextPacketSize(&packetLength);
        }
        Sleep(5);
    }

    pAudioClient->Stop();
    pCaptureClient->Release();
    CoTaskMemFree(pwfx);
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();

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

    // Pre-probe device mix format so FFmpeg command receives exact sample rate & channels
    IMMDeviceEnumerator *pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );
    if (SUCCEEDED(hr)) {
        IMMDevice *pDevice = NULL;
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (SUCCEEDED(hr)) {
            IAudioClient *pAudioClient = NULL;
            hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
            if (SUCCEEDED(hr)) {
                WAVEFORMATEX *pwfx = NULL;
                if (SUCCEEDED(pAudioClient->GetMixFormat(&pwfx))) {
                    g_audioSampleRate = pwfx->nSamplesPerSec;
                    g_audioChannels = pwfx->nChannels;
                    CoTaskMemFree(pwfx);
                }
                pAudioClient->Release();
            }
            pDevice->Release();
        }
        pEnumerator->Release();
    }

    outSampleRate = g_audioSampleRate;
    outChannels = g_audioChannels;

    g_audioThread = std::thread(AudioLoopbackWorker, outPipeName);
    return true;
}

void StopSystemAudioLoopback() {
    g_stopAudio = true;
    if (g_audioThread.joinable()) {
        g_audioThread.join();
    }
}
