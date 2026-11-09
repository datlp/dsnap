#pragma once
#include "common.h"

extern bool g_isRecording;
extern bool g_isPaused;
extern int  g_recordingSeconds;

void InitRecorderNTDLL();
void StartRecording(int x, int y, int w, int h);
void StopRecording(bool cancel);
void TogglePauseRecording();

LRESULT CALLBACK RecordingBorderProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK RecordingToolbarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
