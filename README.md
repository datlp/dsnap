# dsnap ⚡

[![Release](https://img.shields.io/github/v/release/datlp/dsnap?color=blue&label=version)](https://github.com/datlp/dsnap/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6.svg)](https://github.com/datlp/dsnap)

> **A featherweight (< 450 KB), zero-dependency native Windows region screenshot & screen recording utility powered by Win32 API, GDI+, WASAPI Loopback, and FFmpeg.**

[⬇️ **Download Latest Release (dsnap.exe)**](https://github.com/datlp/dsnap/releases/latest/download/dsnap.exe) • [📦 **Download ZIP Archive**](https://github.com/datlp/dsnap/releases/latest/download/dsnap-v1.0.0-windows-x64.zip)

---

## ✨ Features

- **Ultra-lightweight & Native**: Built entirely in **pure Win32 C++17**. Zero .NET Runtime, zero Electron, zero bloat. Executable size is ~430 KB with an idle memory footprint of only ~8 MB.
- **Freeze Desktop Snapshot**:
  - When capturing (`PrintScreen`), the desktop is snapshotted into memory in under 2ms before Windows dismisses active menus.
  - Reliably captures **Right-Click Context Menus, Tooltips, System Tray Popups, Flyouts, and Start Menu** (`SRCCOPY | CAPTUREBLT`).
  - Spotlight selection effect rendered at 60+ FPS using GDI double-buffering.
- **Screen Recording with Floating Control Bar & Visual Border**:
  - **Click-through recording border**: Uses `WS_EX_TRANSPARENT` and is hidden from the recorded video using `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`.
  - **Floating recording toolbar**:
    - `● 00:00`: Live elapsed recording timer.
    - `⏸ / ▶`: Instant Pause / Resume via low-level `NtSuspendProcess` / `NtResumeProcess`.
    - `⏹`: Stop recording and finalize the MP4 container cleanly.
    - `✕`: Cancel recording instantly and **automatically delete partial video**.
- **Orphan-Proof Process Management**:
  - FFmpeg process is attached to a **Windows Job Object** (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`).
  - Guarantees zero zombie `ffmpeg.exe` processes and eliminates file-in-use locking bugs even if dsnap crashes or is killed.
- **Native WASAPI Audio Loopback**:
  - Captures crystal-clear system audio output (YouTube, Spotify, games, meetings) directly from the default playback device.
  - No Microphone required, and no Stereo Mix or virtual audio cables needed.
  - Easily toggled via the system tray context menu (`🎙️ Record System Audio`).
- **Low-Level Keyboard Hook (`WH_KEYBOARD_LL`)**:
  - Intercepts `PrintScreen` and `Shift + PrintScreen` with instant responsiveness, bypassing conflicts with other hotkey tools.
- **Multi-Monitor & Per-Monitor DPI Awareness (V2)**:
  - Spans the complete Virtual Screen (`SM_XVIRTUALSCREEN`, `SM_CXVIRTUALSCREEN`).
  - Crisp rendering and pixel-perfect selection across multi-monitor setups with mixed DPI scaling factors (100%, 125%, 150%, 200%).
- **Standard Windows Known Folders**:
  - Screenshots: `Pictures\Screenshots\dsnap-screenshot-yyyyMMdd-HHmmss.png` (auto-copied to Clipboard as `CF_BITMAP`).
  - Recordings: `Videos\Captures\dsnap-recording-yyyyMMdd-HHmmss.mp4`.
- **Dual-Layer Single-Instance Protection**:
  - Enforced via a Global Named Mutex and window probe.
  - Displays a tray hint balloon if a second instance is launched.
- **High-Contrast Dual-Mode Icon**:
  - Custom camera viewfinder silhouette optimized for crisp visibility across both Windows Dark Mode and Light Mode taskbars.

---

## ⌨️ Hotkeys & Shortcuts

| Hotkey | Action |
| :--- | :--- |
| **`PrintScreen`** | Freezes the screen and opens the region selector. Saves PNG to `Pictures\Screenshots` and **copies image to Clipboard** (`Ctrl + V`). |
| **`Shift + PrintScreen`** | Selects region and starts background MP4 recording with highlight border and floating toolbar. Pressing again cleanly stops recording. |
| **`Escape`** (ESC) | Cancels active region selection. |

---

## 🖱️ System Tray Context Menu

Right-click the `dsnap` icon in the system tray to access options:

- 📸 **Capture Screen (PrintScreen)**
- 🎥 **Record Screen (Shift + PrtScn)**
- 📁 **Open Screenshots Folder**
- 📁 **Open Recordings Folder**
- 🎙️ **Record Audio** *(Check to enable / Off by default)*
- **Start with Windows** *(Enable autostart on user login)*
- **Exit** *(Terminates application and active recordings)*

---

## 🚀 Getting Started

### Prerequisites
- **Windows 10 / 11** (64-bit recommended)
- **FFmpeg**: Ensure `ffmpeg.exe` is either available in your system `PATH` or placed in the same folder as `dsnap.exe`.

### Running
Simply run `dsnap.exe`. It will minimize to the Windows System Tray (notification area) and listen for global capture hotkeys.

---

## 🛠️ Building from Source

### Option 1: Root Build Script (`build.bat`)
Automatically detects **LLVM Clang++**, **MinGW GCC**, or **MSVC**:
```cmd
build.bat
```

### Option 2: Modern CMake
```cmd
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

## 📁 Repository Structure

```text
dsnap/
├── .github/
│   └── workflows/
│       └── build.yml       # GitHub Actions CI build & release workflow
├── res/
│   ├── dsnap.ico           # Multi-resolution high-contrast application icon
│   ├── dsnap.manifest      # Per-Monitor DPI V2 & Common Controls manifest
│   ├── dsnap.rc            # Windows resource script
│   └── resource.h          # Resource identifiers
├── src/
│   ├── audio.cpp / .h      # DirectShow audio device enumerator
│   ├── capture.cpp / .h    # GDI freeze desktop snapshot & PNG encoder
│   ├── common.cpp / .h     # Global definitions, DPI context, window affinities
│   ├── config.cpp / .h     # Registry configuration & Windows known folders
│   ├── hotkey.cpp / .h     # Low-level keyboard hook (WH_KEYBOARD_LL)
│   ├── main.cpp            # WinMain entry point, single-instance mutex, event loop
│   ├── overlay.cpp / .h    # Snipping selection overlay & spotlight effect
│   ├── recorder.cpp / .h   # FFmpeg pipe controller, border, floating toolbar
│   └── tray.cpp / .h       # System tray icon & context menu management
├── .gitignore              # Ignored build outputs and media files
├── build.bat               # Automated one-click native build script
├── CMakeLists.txt          # Modern CMake configuration
├── LICENSE                 # MIT License
└── README.md               # Project documentation
```

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
