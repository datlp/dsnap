# dsnap ⚡

> **A featherweight (< 350 KB), zero-bloat native Windows region screenshot & screen recording utility powered by Win32 API, GDI+, and FFmpeg.**

---

## ✨ Features

- **Siêu nhẹ & Native**: Viết bằng **C++ Win32 API thuần**, không phụ thuộc .NET Runtime, Java hay Python. Kích thước file `.exe` chỉ ~340 KB, chiếm < 5 MB RAM khi chạy nền.
- **Chiếm quyền phím tắt bằng Hook cấp thấp (`WH_KEYBOARD_LL`)**:
  - Không bị chặn bởi các app đã đăng ký trước (`RegisterHotKey`) như Windows Snipping Tool hay OneDrive.
  - Tự động "nuốt phím" (consume key) để phản hồi tức thì.
- **Hỗ trợ đa màn hình (Multi-Monitor)**: Nhận diện toàn bộ không gian Virtual Screen (`SM_XVIRTUALSCREEN`, `SM_CXVIRTUALSCREEN`,...).
- **Chuẩn Per-Monitor DPI (V2)**: Không bị mờ giao diện hay lệch tọa độ trên màn hình 2K/4K có tỉ lệ DPI scaling (125%, 150%, 200%).
- **Lưu trữ chuẩn Windows Known Folders**:
  - Ảnh: `Pictures\Screenshots\dsnap-screenshot-yyyyMMdd-HHmmss.png`
  - Video: `Videos\Captures\dsnap-recording-yyyyMMdd-HHmmss.mp4`
- **Ghi video MP4 an toàn**: Điều khiển `ffmpeg.exe` ngầm qua Anonymous Pipe, gửi lệnh `'q'` khi dừng để đóng gói container MP4 hoàn chỉnh (ghi metadata `moov atom`), tránh tuyệt đối lỗi corrupt video.
- **Âm thanh phản hồi (Audio Feedback)**: Phát âm báo nhẹ khi bắt đầu quay và sau khi lưu file thành công.
- **Khay hệ thống (System Tray)**:
  - Icon khay hệ thống hiển thị trạng thái (Sẵn sàng / Đang quay).
  - Menu chuột phải: Bật/Tắt tự khởi động cùng Windows (`Start with Windows`), Thoát (`Exit`).

---

## ⌨️ Phím tắt (Hotkeys)

| Phím tắt | Thao tác & Hành vi |
| :--- | :--- |
| **`PrintScreen`** | Mở lớp phủ mờ (viền xanh), kéo chuột chọn vùng $\rightarrow$ Lưu ảnh PNG vào `Pictures\Screenshots` và **tự động Copy ảnh vào Clipboard** để dán ngay (`Ctrl + V`). |
| **`Shift + PrintScreen`** (Lần 1) | Mở lớp phủ mờ (viền đỏ), kéo chuột chọn vùng $\rightarrow$ Bắt đầu quay video màn hình ngầm với FFmpeg (30 FPS, H.264 ultrafast). |
| **`Shift + PrintScreen`** (Lần 2) | Dừng quay ngay lập tức, hoàn thiện file MP4 và lưu vào `Videos\Captures`. |
| **`Escape`** (ESC) | Hủy thao tác chọn vùng chụp/quay. |

---

## 🚀 Cài đặt & Sử dụng

### 1. Chuẩn bị FFmpeg
`dsnap` tự động tìm kiếm `ffmpeg.exe` theo thứ tự ưu tiên:
1. Nằm cùng thư mục với `dsnap.exe`.
2. Có sẵn trong biến môi trường `PATH` của Windows.

*(Nếu máy bạn đã cài FFmpeg qua Winget hoặc Chocolatey, `dsnap` sẽ tự nhận diện ngay lập tức).*

### 2. Chạy ứng dụng
- Click đúp `dsnap.exe`. Ứng dụng sẽ chạy ngầm dưới góc phải thanh Taskbar (System Tray).
- Chuột phải vào biểu tượng `dsnap` ở khay hệ thống để bật **Start with Windows** nếu muốn khởi động cùng máy tính.

---

## 🛠️ Biên dịch từ mã nguồn (Build)

Bạn có thể biên dịch `dsnap.exe` bằng bất kỳ trình biên dịch C++ nào:

### Cách 1: Sử dụng script tự động
Chỉ cần chạy file `build.bat`:
```cmd
build.bat
```

### Cách 2: Biên dịch thủ công qua dòng lệnh

* **Dùng MinGW / Clang++ (LLVM-MinGW)**:
```bash
windres dsnap.rc -O coff -o dsnap.res
clang++ -O3 -std=c++17 -mwindows dsnap.cpp dsnap.res -o dsnap.exe -lgdi32 -luser32 -lshell32 -ladvapi32 -lgdiplus -lole32 -static
strip --strip-all dsnap.exe
```

* **Dùng MSVC (`cl.exe`)**:
```cmd
rc /fo dsnap.res dsnap.rc
cl /O2 /std:c++17 /EHsc dsnap.cpp dsnap.res /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib ole32.lib /OUT:dsnap.exe
```

---

## 📄 Cấu trúc dự án

```text
dsnap/
├── dsnap.cpp           # Toàn bộ mã nguồn Win32 C++ (Single File)
├── dsnap.manifest      # Khai báo Per-Monitor DPI Awareness V2 & Common Controls
├── dsnap.rc            # File tài nguyên nhúng Manifest vào binary
├── build.bat           # Script build tự động
├── dsnap.exe           # Binary thực thi (< 350 KB)
├── dsnap.py            # Bản script Python tương đương (dành cho thử nghiệm nhanh)
└── README.md           # Hướng dẫn sử dụng & tài liệu
```

