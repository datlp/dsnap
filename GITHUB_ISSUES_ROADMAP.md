# GitHub Issues Roadmap for datlp/dsnap 🚀

Dưới đây là nội dung chi tiết của 7 Issues được chuẩn hóa theo phong cách của các dự án lớn (như React, VS Code, ShareX), sẵn sàng để tạo trên `https://github.com/datlp/dsnap/issues`.

---

## Issue #1: [Core] Initial Win32 native implementation and FFmpeg controller

**Labels:** `enhancement`, `core`, `win32`

### Description
Xây dựng khung kiến trúc cốt lõi của `dsnap`: một công cụ chụp và quay màn hình siêu nhẹ (< 400 KB) bằng C++ thuần Win32 API, không phụ thuộc .NET Runtime hay thư viện ngoài.

### Tasks
- [x] Đăng ký bắt phím toàn hệ thống bằng Low-level Keyboard Hook (`WH_KEYBOARD_LL`) cho `PrintScreen` và `Shift + PrintScreen`.
- [x] Triển khai lớp phủ chọn vùng màn hình bán trong suốt (`WS_EX_LAYERED`).
- [x] Tích hợp GDI `BitBlt` + GDI+ PNG Encoder để lưu ảnh vào `Pictures\Screenshots` và tự copy vào Clipboard (`CF_BITMAP`).
- [x] Tự động phát hiện `ffmpeg.exe` trong thư mục hiện tại hoặc biến môi trường `PATH`.
- [x] Khởi tạo ngầm tiến trình FFmpeg CLI qua Anonymous Pipe, gửi ký tự `'q'` an toàn khi dừng để đóng gói container MP4 chuẩn xác.
- [x] Hỗ trợ Per-Monitor DPI Awareness V2 và toàn bộ không gian Virtual Screen đa màn hình.

**Commit:** `feat: initial Win32 native implementation of dsnap (#1)`

---

## Issue #2: [UI] Add high-contrast dual-mode icon for Dark & Light taskbars

**Labels:** `ui`, `enhancement`, `design`

### Description
Icon ứng dụng hiển thị trên khay hệ thống (System Tray) bị chìm và khó phân biệt khi người dùng sử dụng thanh Taskbar màu tối (Dark Mode) hoặc màu sáng (Light Mode).

### Solution
- Tạo bộ icon đa độ phân giải `dsnap.ico` gồm 7 kích thước tiêu chuẩn (`16x16`, `24x24`, `32x32`, `48x48`, `64x64`, `128x128`, `256x256`).
- Thân icon màu Electric Blue nổi bật trên Dark Mode.
- Viền trắng tương phản kép bên trong + viền bóng mờ (drop shadow) bên ngoài giúp icon sắc nét trên Light Mode.
- Tinh chỉnh riêng ở mức pixel (pixel-hinted) cho cỡ 16x16 của System Tray.
- Nhúng resource vào binary thông qua `dsnap.rc` (`IDI_APPICON 101`).

**Commit:** `feat(ui): add high-contrast dual-mode pixel-crafted icon (#2)`

---

## Issue #3: [Overlay] Fix stacking overlay transparency and nested message loops

**Labels:** `bug`, `ui`, `performance`

### Description
Khi người dùng nhấn phím `PrintScreen` nhiều lần liên tiếp, màn hình ngày càng tối đen lại và ứng dụng bị lag hoặc treo.

### Root Cause
- Thiếu cờ guard kiểm tra trạng thái Overlay đang hoạt động (`g_isOverlayActive`).
- Mỗi lần nhấn phím lại tạo thêm một cửa sổ `WS_EX_LAYERED` với alpha=60 chồng đè lên nhau, làm tăng độ tối theo cấp số nhân ($1 - (1 - 0.24)^n$).
- Mỗi Overlay chạy một vòng lặp `while (GetMessage)` lồng nhau làm hỏng luồng thông điệp chính.

### Solution
- Đăng ký Window Class `DsnapOverlay` 1 lần duy nhất lúc khởi động app.
- Quy hoạch lại toàn bộ ứng dụng về 1 vòng lặp thông điệp tập trung (Single Message Loop) tại `WinMain`.
- Bổ sung guard `g_isOverlayActive`: Bỏ qua các lần nhấn phím tiếp theo khi Overlay đã mở.

**Commit:** `fix: prevent duplicate overlay stacking and eliminate nested message loops (#3)`

---

## Issue #4: [Capture] Implement Freeze Desktop Snapshot to capture context menus

**Labels:** `bug`, `core`, `ux`

### Description
Khi người dùng chuột phải vào System Tray (hoặc bất kỳ menu ngữ cảnh nào) rồi nhấn `PrintScreen`, chuột không thể kéo chọn vùng hoặc menu chuột phải lập tức biến mất trước khi kịp chụp.

### Root Cause
- Windows Shell (`explorer.exe`) kích hoạt cơ chế chiếm quyền chuột độc quyền (`SetCapture()`) khi mở Context Menu.
- Khi Overlay thông thường mở lên, sự thay đổi focus khiến Windows tự động hủy menu chuột phải ngay trong 1ms.

### Solution
- Chuyển đổi sang cơ chế **Đóng băng màn hình (Freeze Desktop Snapshot)** giống ShareX / macOS:
  1. Ngay khi bấm phím, gọi `BitBlt` chụp toàn màn hình vào RAM trong 2ms trước khi Windows kịp đóng menu.
  2. Giải phóng quyền chuột (`ReleaseCapture` / `EndMenu`).
  3. Hiển thị ảnh đã chụp lên màn hình và tạo hiệu ứng chiếu sáng vùng chọn (Illuminate Selection) mượt mà 60 FPS với Double-Buffering.
  4. Bổ sung nút bấm chụp ảnh / quay video trực tiếp vào menu chuột phải của System Tray.

**Commit:** `fix: implement Freeze Desktop Snapshot to capture system tray and context menus (#4)`

---

## Issue #5: [Recorder] Add non-intrusive region border and floating control toolbar

**Labels:** `enhancement`, `recorder`, `ui`

### Description
Khi quay video màn hình, vùng chọn biến mất khiến người dùng không biết mép viền đang quay ở đâu, đồng thời thiếu các nút điều khiển trực quan bằng chuột.

### Solution
- **Khung viền đỏ bao quanh vùng quay:**
  - Nằm ở tọa độ viền ngoài 3px, có thuộc tính `WS_EX_TRANSPARENT` để xuyên thấu click chuột 100%.
  - Áp dụng API `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` để tàng hình trước FFmpeg (video xuất ra không bị dính viền).
- **Thanh điều khiển nổi (Floating Toolbar) góc trên bên phải:**
  - Đồng hồ đếm thời gian thực `● MM:SS`.
  - Nút **`⏸ / ▶` (Tạm dừng / Tiếp tục)** sử dụng `NtSuspendProcess` / `NtResumeProcess`.
  - Nút **`⏹` (Dừng & Lưu)**: Hoàn tất đóng gói MP4 an toàn.
  - Nút **`✕` (Hủy bỏ)**: Dừng FFmpeg và tự động xóa file tạm, không để lại rác trên ổ cứng.

**Commit:** `feat(recorder): add click-through region border and top-right floating toolbar (#5)`

---

## Issue #6: [Audio] Add optional DirectShow audio recording toggle

**Labels:** `enhancement`, `audio`, `recorder`

### Description
Người dùng có nhu cầu tùy chọn ghi lại âm thanh (từ Microphone hoặc Stereo Mix hệ thống) khi quay màn hình, với thiết lập mặc định là không ghi âm để giữ video siêu nhẹ.

### Solution
- Tự động quét và nhận diện thiết bị thu âm thanh khả dụng trên Windows thông qua DirectShow `CLSID_AudioInputDeviceCategory`.
- Bổ sung mục tick chọn trong khay hệ thống: `[ ] 🎙️ Quay kèm âm thanh (Audio)` (mặc định: **NO**).
- Khi bật, FFmpeg tự động ghép thêm luồng audio mã hóa AAC `-c:a aac -b:a 128k`.
- Lưu trữ trạng thái cấu hình vào Registry (`HKCU\Software\dsnap\RecordAudio`).

**Commit:** `feat(audio): add optional audio recording toggle in tray menu (#6)`

---

## Issue #7: [App] Enforce strict single-instance lifecycle with Global Mutex

**Labels:** `enhancement`, `stability`, `win32`

### Description
Ngăn chặn người dùng click mở file `.exe` nhiều lần dẫn đến việc chạy trùng lặp nhiều tiến trình `dsnap.exe`, gây xung đột keyboard hook và rác khay hệ thống.

### Solution
- **Bảo vệ 2 lớp:**
  - Quét cửa sổ nền bằng `FindWindowW(BG_CLASS, L"dsnap_bg")`. Nếu tìm thấy, instance mới gửi thông điệp đánh thức và lập tức thoát.
  - Sử dụng `CreateMutexW` với tên `Global\Dsnap_SingleInstance_Mutex_9999` bao quát toàn bộ các phiên Windows.
- **Phản hồi người dùng (UX):** Khi mở lại, instance đang chạy sẽ hiển thị thông báo bong bóng (Balloon Tip) ở góc màn hình: *"dsnap đã đang chạy sẵn trong khay hệ thống!"*.
- **Tự khôi phục Icon:** Bắt thông điệp `RegisterWindowMessageW(L"TaskbarCreated")` để tự gắn lại icon nếu Windows Explorer bị crash hoặc khởi động lại.

**Commit:** `fix: enforce single-instance lifecycle using Global Mutex and FindWindow (#7)`
