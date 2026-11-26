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
        case StringId::MenuCapturePhoto:         return L"Chụp màn hình (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"Quay màn hình (Shift + PrtScn)";
        case StringId::MenuScreenshots:          return L"Ảnh chụp màn hình";
        case StringId::MenuRecordings:           return L"Video quay màn hình";
        case StringId::MenuOpenScreenshots:      return L"Mở thư mục ảnh chụp";
        case StringId::MenuOpenCaptures:         return L"Mở thư mục video quay";
        case StringId::MenuClearScreenshots:     return L"Dọn sạch thư mục ảnh chụp";
        case StringId::MenuClearCaptures:        return L"Dọn sạch thư mục video quay";
        case StringId::MenuAutoCopyImage:        return L"Tự động copy ảnh vào Clipboard";
        case StringId::MenuAutoCopyPath:         return L"Tự động copy đường dẫn vào Clipboard";
        case StringId::ClearConfirmTitle:        return L"dsnap - Xác nhận xóa";
        case StringId::ClearConfirmBody:         return L"Bạn có chắc chắn muốn xóa %d tệp trong thư mục này không?";
        case StringId::ClearEmptyBody:           return L"Thư mục hiện đang trống (không có tệp dsnap nào).";
        case StringId::MenuToggleAudio:          return L"Ghi âm thanh hệ thống";
        case StringId::MenuToggleMic:            return L"Ghi âm Microphone";
        case StringId::MenuAudioDevices:         return L"Thiết bị âm thanh";
        case StringId::MenuAudioOutHeader:       return L"Đầu ra (Loa / Tai nghe)";
        case StringId::MenuAudioInHeader:        return L"Đầu vào (Microphone)";
        case StringId::MenuAudioDefaultOut:      return L"Thiết bị phát mặc định";
        case StringId::MenuAudioDefaultIn:       return L"Microphone mặc định";
        case StringId::MenuAudioNone:            return L"(Không tìm thấy thiết bị)";
        case StringId::MenuHighlightClicks:      return L"Hiệu ứng click chuột";
        case StringId::MenuStartWithWindows:     return L"Khởi động cùng Windows";
        case StringId::MenuLanguage:             return L"Ngôn ngữ (Language)";
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
        case StringId::MenuCapturePhoto:         return L"截取屏幕 (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"录制屏幕 (Shift + PrtScn)";
        case StringId::MenuScreenshots:          return L"截图选项";
        case StringId::MenuRecordings:           return L"录屏选项";
        case StringId::MenuOpenScreenshots:      return L"打开截图文件夹";
        case StringId::MenuOpenCaptures:         return L"打开录制文件夹";
        case StringId::MenuClearScreenshots:     return L"清空截图文件夹";
        case StringId::MenuClearCaptures:        return L"清空录制文件夹";
        case StringId::MenuAutoCopyImage:        return L"自动复制图片到剪贴板";
        case StringId::MenuAutoCopyPath:         return L"自动复制文件路径到剪贴板";
        case StringId::ClearConfirmTitle:        return L"dsnap - 确认清理";
        case StringId::ClearConfirmBody:         return L"确定要删除此文件夹中的 %d 个文件吗？";
        case StringId::ClearEmptyBody:           return L"文件夹中暂无 dsnap 文件。";
        case StringId::MenuToggleAudio:          return L"录制系统声音";
        case StringId::MenuToggleMic:            return L"录制麦克风";
        case StringId::MenuAudioDevices:         return L"音频设备";
        case StringId::MenuAudioOutHeader:       return L"输出设备 (扬声器 / 耳机)";
        case StringId::MenuAudioInHeader:        return L"输入设备 (麦克风)";
        case StringId::MenuAudioDefaultOut:      return L"默认系统输出";
        case StringId::MenuAudioDefaultIn:       return L"默认麦克风";
        case StringId::MenuAudioNone:            return L"(未找到活动设备)";
        case StringId::MenuHighlightClicks:      return L"鼠标点击高亮效果";
        case StringId::MenuStartWithWindows:     return L"随 Windows 启动";
        case StringId::MenuLanguage:             return L"语言 (Language)";
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
        case StringId::MenuCapturePhoto:         return L"화면 캡처 (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"화면 녹화 (Shift + PrtScn)";
        case StringId::MenuScreenshots:          return L"스크린샷";
        case StringId::MenuRecordings:           return L"화면 녹화";
        case StringId::MenuOpenScreenshots:      return L"스크린샷 폴더 열기";
        case StringId::MenuOpenCaptures:         return L"녹화 폴더 열기";
        case StringId::MenuClearScreenshots:     return L"스크린샷 폴더 비우기";
        case StringId::MenuClearCaptures:        return L"녹화 폴더 비우기";
        case StringId::MenuAutoCopyImage:        return L"클립보드에 이미지 자동 복사";
        case StringId::MenuAutoCopyPath:         return L"클립보드에 파일 경로 자동 복사";
        case StringId::ClearConfirmTitle:        return L"dsnap - 삭제 확인";
        case StringId::ClearConfirmBody:         return L"이 폴더에서 %d개의 파일을 삭제하시겠습니까?";
        case StringId::ClearEmptyBody:           return L"폴더에 dsnap 파일이 없습니다.";
        case StringId::MenuToggleAudio:          return L"시스템 오디오 녹음";
        case StringId::MenuToggleMic:            return L"마이크 녹음";
        case StringId::MenuAudioDevices:         return L"오디오 장치";
        case StringId::MenuAudioOutHeader:       return L"출력 장치 (스피커 / 헤드폰)";
        case StringId::MenuAudioInHeader:        return L"입력 장치 (마이크)";
        case StringId::MenuAudioDefaultOut:      return L"기본 시스템 출력";
        case StringId::MenuAudioDefaultIn:       return L"기본 마이크";
        case StringId::MenuAudioNone:            return L"(활성 장치 없음)";
        case StringId::MenuHighlightClicks:      return L"마우스 클릭 효과 표시";
        case StringId::MenuStartWithWindows:     return L"Windows 시작 시 실행";
        case StringId::MenuLanguage:             return L"언어 (Language)";
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
        case StringId::MenuCapturePhoto:         return L"画面キャプチャ (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"画面録画 (Shift + PrtScn)";
        case StringId::MenuScreenshots:          return L"スクリーンショット";
        case StringId::MenuRecordings:           return L"画面録画";
        case StringId::MenuOpenScreenshots:      return L"スクリーンショットフォルダを開く";
        case StringId::MenuOpenCaptures:         return L"録画フォルダを開く";
        case StringId::MenuClearScreenshots:     return L"スクリーンショットフォルダを空にする";
        case StringId::MenuClearCaptures:        return L"録画フォルダを空にする";
        case StringId::MenuAutoCopyImage:        return L"画像をクリップボードに自動コピー";
        case StringId::MenuAutoCopyPath:         return L"ファイルパスをクリップボードに自動コピー";
        case StringId::ClearConfirmTitle:        return L"dsnap - 削除の確認";
        case StringId::ClearConfirmBody:         return L"このフォルダ内の %d 個のファイルを削除しますか？";
        case StringId::ClearEmptyBody:           return L"フォルダ内に dsnap ファイルはありません。";
        case StringId::MenuToggleAudio:          return L"システム音声を録音";
        case StringId::MenuToggleMic:            return L"マイクを録音";
        case StringId::MenuAudioDevices:         return L"オーディオデバイス";
        case StringId::MenuAudioOutHeader:       return L"出力デバイス (スピーカー / ヘッドホン)";
        case StringId::MenuAudioInHeader:        return L"入力デバイス (マイク)";
        case StringId::MenuAudioDefaultOut:      return L"既定のシステム出力";
        case StringId::MenuAudioDefaultIn:       return L"既定のマイク";
        case StringId::MenuAudioNone:            return L"(有効なデバイスがありません)";
        case StringId::MenuHighlightClicks:      return L"マウスクリック効果を表示";
        case StringId::MenuStartWithWindows:     return L"Windows 起動時に実行";
        case StringId::MenuLanguage:             return L"言語 (Language)";
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
        case StringId::MenuCapturePhoto:         return L"Capture Screen (PrintScreen)";
        case StringId::MenuRecordVideo:          return L"Record Screen (Shift + PrtScn)";
        case StringId::MenuScreenshots:          return L"Screenshots";
        case StringId::MenuRecordings:           return L"Recordings";
        case StringId::MenuOpenScreenshots:      return L"Open Screenshots Folder";
        case StringId::MenuOpenCaptures:         return L"Open Recordings Folder";
        case StringId::MenuClearScreenshots:     return L"Clear Screenshots Folder";
        case StringId::MenuClearCaptures:        return L"Clear Recordings Folder";
        case StringId::MenuAutoCopyImage:        return L"Auto-Copy Image to Clipboard";
        case StringId::MenuAutoCopyPath:         return L"Auto-Copy Path to Clipboard";
        case StringId::ClearConfirmTitle:        return L"dsnap - Confirm Clear";
        case StringId::ClearConfirmBody:         return L"Are you sure you want to delete %d file(s) from this folder?";
        case StringId::ClearEmptyBody:           return L"No dsnap files found in this folder.";
        case StringId::MenuToggleAudio:          return L"Record System Audio";
        case StringId::MenuToggleMic:            return L"Record Microphone";
        case StringId::MenuAudioDevices:         return L"Audio Devices";
        case StringId::MenuAudioOutHeader:       return L"Output (Speakers / Headphones)";
        case StringId::MenuAudioInHeader:        return L"Input (Microphones)";
        case StringId::MenuAudioDefaultOut:      return L"Default System Output";
        case StringId::MenuAudioDefaultIn:       return L"Default Microphone";
        case StringId::MenuAudioNone:            return L"(No active devices found)";
        case StringId::MenuHighlightClicks:      return L"Highlight Mouse Clicks";
        case StringId::MenuStartWithWindows:     return L"Start with Windows";
        case StringId::MenuLanguage:             return L"Language";
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
