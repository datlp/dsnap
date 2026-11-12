#include "i18n.h"
#include <windows.h>

static Language s_currentLanguage = Language::English;

Language DetectSystemLanguage() {
    LANGID langId = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langId);

    switch (primaryLang) {
    case LANG_VIETNAMESE:
        return Language::Vietnamese;
    case LANG_CHINESE:
        return Language::Chinese;
    case LANG_KOREAN:
        return Language::Korean;
    case LANG_JAPANESE:
        return Language::Japanese;
    default:
        return Language::English;
    }
}

std::wstring GetLanguageCode(Language lang) {
    switch (lang) {
    case Language::Vietnamese: return L"vi-VN";
    case Language::Chinese:    return L"zh-CN";
    case Language::Korean:     return L"ko-KR";
    case Language::Japanese:   return L"ja-JP";
    case Language::English:
    default:                   return L"en-US";
    }
}

Language ParseLanguageCode(const std::wstring& code) {
    if (code == L"vi-VN" || code == L"vi") return Language::Vietnamese;
    if (code == L"zh-CN" || code == L"zh") return Language::Chinese;
    if (code == L"ko-KR" || code == L"ko") return Language::Korean;
    if (code == L"ja-JP" || code == L"ja") return Language::Japanese;
    if (code == L"en-US" || code == L"en") return Language::English;
    return Language::Auto;
}

void SetCurrentLanguage(Language lang) {
    if (lang == Language::Auto) {
        s_currentLanguage = DetectSystemLanguage();
    } else {
        s_currentLanguage = lang;
    }
}

Language GetCurrentLanguage() {
    return s_currentLanguage;
}

void InitLanguage() {
    s_currentLanguage = DetectSystemLanguage();
}

const wchar_t* Tr(StringId id) {
    switch (s_currentLanguage) {
    case Language::Vietnamese:
        switch (id) {
        case StringId::MenuCapturePhoto:         return L"📸 Chụp màn hình (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"🎥 Quay màn hình (Shift + PrtScn)";
        case StringId::MenuOpenScreenshots:      return L"📁 Mở thư mục ảnh chụp";
        case StringId::MenuOpenCaptures:         return L"📁 Mở thư mục video quay";
        case StringId::MenuToggleAudio:          return L"🎙️ Ghi âm thanh hệ thống";
        case StringId::MenuHighlightClicks:      return L"✨ Hiệu ứng click chuột";
        case StringId::MenuStartWithWindows:     return L"Khởi động cùng Windows";
        case StringId::MenuLanguage:             return L"🌐 Ngôn ngữ (Language)";
        case StringId::MenuExit:                 return L"Thoát";
        case StringId::TrayTipDefault:           return L"dsnap\n• Chụp: PrtScn\n• Quay: Shift + PrtScn";
        case StringId::OverlayGuidePhoto:        return L"Kéo: Chọn vùng chụp | Nháy đúp: Toàn màn hình | ESC: Hủy";
        case StringId::OverlayGuideVideo:        return L"Kéo: Chọn vùng quay | Enter: Bắt đầu quay | ESC: Hủy";
        case StringId::NotificationRunningTitle: return L"dsnap đang hoạt động!";
        case StringId::NotificationRunningBody:  return L"dsnap đã sẵn sàng trong khay hệ thống!\nNhấn PrtScn để chụp, Shift+PrtScn để quay.";
        case StringId::FFmpegPromptTitle:        return L"dsnap - Yêu cầu FFmpeg";
        case StringId::FFmpegPromptBody:         return L"Không tìm thấy FFmpeg trên máy tính của bạn.\n\nTính năng quay màn hình cần bộ giải mã FFmpeg (~28 MB).\nBạn có muốn dsnap tự động tải về ngay bây giờ không?";
        }
        break;

    case Language::Chinese:
        switch (id) {
        case StringId::MenuCapturePhoto:         return L"📸 截取屏幕 (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"🎥 录制屏幕 (Shift + PrtScn)";
        case StringId::MenuOpenScreenshots:      return L"📁 打开截图文件夹";
        case StringId::MenuOpenCaptures:         return L"📁 打开录制文件夹";
        case StringId::MenuToggleAudio:          return L"🎙️ 录制系统声音";
        case StringId::MenuHighlightClicks:      return L"✨ 鼠标点击高亮效果";
        case StringId::MenuStartWithWindows:     return L"随 Windows 启动";
        case StringId::MenuLanguage:             return L"🌐 语言 (Language)";
        case StringId::MenuExit:                 return L"退出";
        case StringId::TrayTipDefault:           return L"dsnap\n• 截图: PrtScn\n• 录屏: Shift + PrtScn";
        case StringId::OverlayGuidePhoto:        return L"拖拽: 选择截图区域 | 双击: 全屏截图 | ESC: 取消";
        case StringId::OverlayGuideVideo:        return L"拖拽: 选择录屏区域 | Enter: 开始录制 | ESC: 取消";
        case StringId::NotificationRunningTitle: return L"dsnap 正在运行!";
        case StringId::NotificationRunningBody:  return L"dsnap 已在系统托盘就绪!\n按 PrtScn 截图，Shift+PrtScn 录屏。";
        case StringId::FFmpegPromptTitle:        return L"dsnap - 需要 FFmpeg";
        case StringId::FFmpegPromptBody:         return L"在您的系统中未找到 FFmpeg。\n\n录屏功能需要轻量级 FFmpeg 二进制文件 (~28 MB)。\n是否希望 dsnap 现在自动下载?";
        }
        break;

    case Language::Korean:
        switch (id) {
        case StringId::MenuCapturePhoto:         return L"📸 화면 캡처 (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"🎥 화면 녹화 (Shift + PrtScn)";
        case StringId::MenuOpenScreenshots:      return L"📁 스크린샷 폴더 열기";
        case StringId::MenuOpenCaptures:         return L"📁 녹화 폴더 열기";
        case StringId::MenuToggleAudio:          return L"🎙️ 시스템 오디오 녹음";
        case StringId::MenuHighlightClicks:      return L"✨ 마우스 클릭 효과 표시";
        case StringId::MenuStartWithWindows:     return L"Windows 시작 시 실행";
        case StringId::MenuLanguage:             return L"🌐 언어 (Language)";
        case StringId::MenuExit:                 return L"종료";
        case StringId::TrayTipDefault:           return L"dsnap\n• 캡처: PrtScn\n• 녹화: Shift + PrtScn";
        case StringId::OverlayGuidePhoto:        return L"드래그: 영역 선택 | 더블클릭: 전체 화면 | ESC: 취소";
        case StringId::OverlayGuideVideo:        return L"드래그: 영역 선택 | Enter: 녹화 시작 | ESC: 취소";
        case StringId::NotificationRunningTitle: return L"dsnap 실행 중!";
        case StringId::NotificationRunningBody:  return L"dsnap이 시스템 트레이에 준비되었습니다!\nPrtScn을 눌러 캡처하고, Shift+PrtScn을 눌러 녹화하세요.";
        case StringId::FFmpegPromptTitle:        return L"dsnap - FFmpeg 필요";
        case StringId::FFmpegPromptBody:         return L"시스템에서 FFmpeg를 찾을 수 없습니다.\n\n화면 녹화 기능을 사용하려면 FFmpeg 파일(~28 MB)이 필요합니다.\n지금 dsnap이 자동으로 다운로드하도록 하시겠습니까?";
        }
        break;

    case Language::Japanese:
        switch (id) {
        case StringId::MenuCapturePhoto:         return L"📸 画面キャプチャ (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"🎥 画面録画 (Shift + PrtScn)";
        case StringId::MenuOpenScreenshots:      return L"📁 スクリーンショットフォルダを開く";
        case StringId::MenuOpenCaptures:         return L"📁 録画フォルダを開く";
        case StringId::MenuToggleAudio:          return L"🎙️ システムオーディオ録音";
        case StringId::MenuHighlightClicks:      return L"✨ マウスクリック効果を表示";
        case StringId::MenuStartWithWindows:     return L"Windows起動時に開始";
        case StringId::MenuLanguage:             return L"🌐 言語 (Language)";
        case StringId::MenuExit:                 return L"終了";
        case StringId::TrayTipDefault:           return L"dsnap\n• キャプチャ: PrtScn\n• 録画: Shift + PrtScn";
        case StringId::OverlayGuidePhoto:        return L"ドラッグ: 領域選択 | ダブルクリック: 全画面 | ESC: キャンセル";
        case StringId::OverlayGuideVideo:        return L"ドラッグ: 領域選択 | Enter: 録画開始 | ESC: キャンセル";
        case StringId::NotificationRunningTitle: return L"dsnap は実行中です!";
        case StringId::NotificationRunningBody:  return L"dsnap の準備が完了しました!\nPrtScnでキャプチャ、Shift+PrtScnで録画できます。";
        case StringId::FFmpegPromptTitle:        return L"dsnap - FFmpegが必要です";
        case StringId::FFmpegPromptBody:         return L"システム上に FFmpeg が見つかりませんでした。\n\n画面録画には軽量な FFmpeg バイナリ (~28 MB) が必要です。\n今すぐ dsnap に自動ダウンロードさせますか?";
        }
        break;

    case Language::English:
    default:
        switch (id) {
        case StringId::MenuCapturePhoto:         return L"📸 Capture Screen (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"🎥 Record Screen (Shift + PrtScn)";
        case StringId::MenuOpenScreenshots:      return L"📁 Open Screenshots Folder";
        case StringId::MenuOpenCaptures:         return L"📁 Open Recordings Folder";
        case StringId::MenuToggleAudio:          return L"🎙️ Record System Audio";
        case StringId::MenuHighlightClicks:      return L"✨ Highlight Mouse Clicks";
        case StringId::MenuStartWithWindows:     return L"Start with Windows";
        case StringId::MenuLanguage:             return L"🌐 Language";
        case StringId::MenuExit:                 return L"Exit";
        case StringId::TrayTipDefault:           return L"dsnap\n• Capture: PrtScn\n• Record: Shift + PrtScn";
        case StringId::OverlayGuidePhoto:        return L"Drag: Select Region | Double-click: Fullscreen | ESC: Cancel";
        case StringId::OverlayGuideVideo:        return L"Drag: Select Region | Enter: Start Recording | ESC: Cancel";
        case StringId::NotificationRunningTitle: return L"dsnap is running!";
        case StringId::NotificationRunningBody:  return L"dsnap is ready in system tray!\nPress PrtScn to capture, Shift+PrtScn to record.";
        case StringId::FFmpegPromptTitle:        return L"dsnap - FFmpeg Required";
        case StringId::FFmpegPromptBody:         return L"FFmpeg was not found on your system.\n\nScreen recording requires a lightweight FFmpeg binary (~28 MB).\nWould you like dsnap to download it automatically now?";
        }
        break;
    }
    return L"";
}

