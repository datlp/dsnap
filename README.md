# dsnap ⚡

> **A featherweight (< 400 KB), zero-bloat native Windows region screenshot & screen recording utility powered by Win32 API, GDI+, DirectShow, and FFmpeg.**

---

## ✨ Features

- **Siêu nhẹ & Native**: Viết bằng **C++ Win32 API thuần**, không phụ thuộc .NET Runtime hay bất kỳ thư viện cồng kềnh nào. File `.exe` chỉ ~390 KB, chiếm ~7 MB RAM khi chạy ngầm.
- **Đóng băng màn hình (Freeze Desktop Snapshot)**:
  - Khi chụp ảnh (`PrintScreen`), hệ thống chụp ngay lập tức toàn bộ desktop vào RAM trong 2ms trước khi Windows kịp đóng các menu đang mở.
  - Cho phép chụp trọn vẹn **menu chuột phải (Context Menu), Start Menu, System Tray, Tooltip, Flyout**.
  - Hiệu ứng chiếu sáng vùng chọn (Illuminate / Spotlight) mượt mà 60+ FPS với Double-Buffering.
- **Quay video kèm thanh điều khiển nổi & khung viền nhấp nháy**:
  - **Khung viền đỏ bao quanh vùng quay**: Xuyên thấu click chuột 100% (`WS_EX_TRANSPARENT`), tàng hình trước video bằng `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`.
  - **Thanh điều khiển góc trên bên phải**:
    - `● 00:00`: Đồng hồ đếm thời gian quay thời gian thực.
    - `⏸ / ▶`: Tạm dừng / Tiếp tục quay tức thì (sử dụng `NtSuspendProcess`).
    - `⏹`: Dừng quay và lưu file video MP4 hoàn chỉnh.
    - `✕`: Hủy quay ngay lập tức và **tự động xóa file video tạm**.
- **Tùy chọn quay kèm âm thanh (Audio Recording)**:
  - Tích hợp DirectShow Audio Capture (thu từ Microphone hoặc Stereo Mix).
  - Tùy chọn bật/tắt trong khay hệ thống (mặc định: **KHÔNG** để giữ sự tối giản và nhẹ nhàng).
- **Chiếm quyền phím tắt bằng Hook cấp thấp (`WH_KEYBOARD_LL`)**:
  - Không bị chặn bởi các app đã đăng ký trước (`RegisterHotKey`) như Windows Snipping Tool hay OneDrive.
  - Tự động "nuốt phím" (consume key) để phản hồi tức thì.
- **Hỗ trợ đa màn hình (Multi-Monitor)**: Phủ toàn bộ không gian Virtual Screen (`SM_XVIRTUALSCREEN`, `SM_CXVIRTUALSCREEN`,...).
- **Chuẩn Per-Monitor DPI (V2)**: Không bị mờ hay lệch tọa độ trên màn hình 2K/4K có tỉ lệ scaling (125%, 150%, 200%).
- **Lưu trữ chuẩn Windows Known Folders**:
  - Ảnh: `Pictures\Screenshots\dsnap-screenshot-yyyyMMdd-HHmmss.png`
  - Video: `Videos\Captures\dsnap-recording-yyyyMMdd-HHmmss.mp4`
- **Bảo vệ Single-Instance 2 lớp**:
  - Ngăn chặn hoàn toàn việc mở nhiều tiến trình `dsnap.exe`.
  - Tự động hiện thông báo khay hệ thống nhắc nhở khi người dùng mở lại file `.exe`.
- **Icon đa chế độ (Dual-Mode)**: Rõ nét, nổi bật trên cả Dark Mode và Light Mode của Windows.

---

## ⌨️ Phím tắt & Thao tác (Hotkeys)

| Thao tác | Hành vi |
| :--- | :--- |
| **`PrintScreen`** | Đóng băng màn hình, kéo chuột chọn vùng $\rightarrow$ Lưu ảnh PNG vào `Pictures\Screenshots` và **tự động Copy ảnh vào Clipboard** (`Ctrl + V`). |
| **`Shift + PrintScreen`** (Lần 1) | Kéo chọn vùng $\rightarrow$ Bắt đầu quay video MP4 ngầm (kèm khung viền và thanh điều khiển). |
| **`Shift + PrintScreen`** (Lần 2) | Dừng quay và lưu video an toàn vào `Videos\Captures`. |
| **`Escape`** (ESC) | Hủy thao tác chọn vùng chụp/quay. |

---

## 🖱️ Menu Khay Hệ Thống (System Tray)

Chuột phải vào icon `dsnap` ở góc phải Taskbar để truy cập các tính năng:
* 📸 **Chụp ảnh màn hình (PrintScreen)**
* 🎥 **Quay video màn hình (Shift + PrtScn)**
* 📁 **Mở thư mục Ảnh chụp**
* 📁 **Mở thư mục Video quay**
* 🎙️ **Quay kèm âm thanh (Audio)** *(Tick để bật / Mặc định tắt)*
* **Start with Windows** *(Tự khởi động cùng hệ thống)*
* **Exit** *(Thoát hoàn toàn)*

---

## 🚀 Cài đặt & Sử dụng

1. **Chuẩn bị FFmpeg**: Đặt `ffmpeg.exe` cùng thư mục với `dsnap.exe` hoặc thêm vào biến môi trường `PATH`.
2. **Chạy ứng dụng**: Click đúp `dsnap.exe`. Ứng dụng sẽ chạy ngầm dưới góc phải thanh Taskbar (System Tray).

---

## 🛠️ Biên dịch từ mã nguồn (Build)

Chỉ cần chạy file `build.bat` (tự động phát hiện Clang++ / MinGW / MSVC):
```cmd
build.bat
```

---

## 📄 Cấu trúc dự án

```text
dsnap/
├── dsnap.cpp           # Toàn bộ mã nguồn Win32 C++ (Single File)
├── dsnap.manifest      # Khai báo Per-Monitor DPI Awareness V2 & Common Controls
├── dsnap.rc            # File tài nguyên nhúng Icon và Manifest vào binary
├── dsnap.ico           # Bộ icon đa độ phân giải tối ưu Dark/Light Mode
├── build.bat           # Script build tự động
├── dsnap.exe           # Binary thực thi (< 400 KB)
├── dsnap.py            # Bản script Python tương đương (dành cho thử nghiệm nhanh)
└── README.md           # Hướng dẫn sử dụng & tài liệu
```
