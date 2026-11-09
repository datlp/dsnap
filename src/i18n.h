#pragma once
#include <string>

enum class Language {
    Auto = 0,
    English = 1,
    Vietnamese = 2,
    Chinese = 3,
    Korean = 4,
    Japanese = 5
};

enum class StringId {
    MenuCapturePhoto,
    MenuRecordVideo,
    MenuOpenScreenshots,
    MenuOpenCaptures,
    MenuToggleAudio,
    MenuHighlightClicks,
    MenuStartWithWindows,
    MenuLanguage,
    MenuExit,
    TrayTipDefault,
    OverlayGuidePhoto,
    OverlayGuideVideo,
    NotificationRunningTitle,
    NotificationRunningBody,
    FFmpegPromptTitle,
    FFmpegPromptBody
};

void InitLanguage();
Language GetCurrentLanguage();
void SetCurrentLanguage(Language lang);
const wchar_t* Tr(StringId id);
std::wstring GetLanguageCode(Language lang);
Language ParseLanguageCode(const std::wstring& code);
Language DetectSystemLanguage();
