<div align="center">
  <img src="docs/images/logo.png" width="96" height="96" alt="dsnap logo" />
  <h1>dsnap</h1>
  <p><strong>A featherweight (&lt; 450 KB), zero-dependency native Windows screen capture & region recorder.</strong></p>

  <p>
    <a href="https://github.com/datlp/dsnap/releases/latest"><img src="https://img.shields.io/github/v/release/datlp/dsnap?color=0284c7&label=version" alt="Latest Release" /></a>
    <a href="https://github.com/datlp/dsnap/actions/workflows/build.yml"><img src="https://github.com/datlp/dsnap/actions/workflows/build.yml/badge.svg" alt="CI Build Status" /></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green.svg" alt="License: MIT" /></a>
    <a href="https://datlp.github.io/dsnap/"><img src="https://img.shields.io/badge/website-live-0284c7.svg" alt="Official Website" /></a>
    <img src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6.svg" alt="Platform: Windows 10 | 11" />
  </p>

  <p>
    <a href="https://datlp.github.io/dsnap/">🌐 Website</a> &bull;
    <a href="https://github.com/datlp/dsnap/releases/latest">⬇️ Download</a> &bull;
    <a href="#-quick-start">🚀 Quick Start</a> &bull;
    <a href="#-keyboard-shortcuts">⌨️ Shortcuts</a> &bull;
    <a href="#-features">✨ Features</a> &bull;
    <a href="#-building-from-source">🛠️ Build</a> &bull;
    <a href="https://github.com/datlp/dsnap/issues">💬 Report Issue</a>
  </p>
</div>

---

**dsnap** is an ultra-fast, minimalist screen capture and region recording utility engineered from the ground up in pure **Win32 C++17**.

* **⚡ Ultra-lightweight & Native:** Zero .NET Runtime, zero Electron bloat, zero external DLL dependencies. Single standalone executable under 450 KB with ~8 MB idle RAM footprint.
* **📸 Freeze Desktop Snapshot:** Snapshots the desktop into memory in under 2ms via `SRCCOPY | CAPTUREBLT`. Reliably captures right-click context menus, tooltips, flyouts, and start menu without dismissing them.
* **🎙️ Multi-Device WASAPI Audio & Microphone Mixer:** Direct playback loopback and microphone recording from Windows Core Audio. Simultaneously capture and mix multiple Speakers, Headphones, and Microphones at 48kHz Stereo Float32 with zero stereo mix or virtual cable setup.
* **🎬 Click-Through Border, Ripple FX & Controls:** Recording border is click-transparent (`WS_EX_TRANSPARENT`) and hidden from the video via `WDA_EXCLUDEFROMCAPTURE`. Optional 60 FPS mouse click ripple animations and floating toolbar with live timer, pause, resume, and instant cancel.
* **💾 True Zero-Footprint Portable Mode:** Place `dsnap.ini` next to `dsnap.exe` (or launch with `--portable`) to store all settings and captures locally without touching the Windows Registry.

---

## 🚀 Quick Start

### Option 1: Windows Package Manager (Winget)
```cmd
winget install datlp.dsnap
```

### Option 2: Setup Wizard (Recommended)
Download and run the dual-mode Inno Setup installer:
* [⬇️ **Download dsnap-v1.2.0-setup.exe**](https://github.com/datlp/dsnap/releases/latest/download/dsnap-v1.2.0-setup.exe) (~2.2 MB)
* Supports **All Users** (System-wide in `C:\Program Files\dsnap` with UAC), **Current User** (Non-admin in `%LocalAppData%\Programs\dsnap`), and optional **Always run as Administrator** with zero-UAC Task Scheduler autostart.

### Option 3: Portable Edition (No Install Required)
* [⚡ **dsnap-v1.2.0-windows-x64.exe**](https://github.com/datlp/dsnap/releases/latest/download/dsnap-v1.2.0-windows-x64.exe) (~476 KB standalone executable)
* [📦 **dsnap-v1.2.0-windows-x64.zip**](https://github.com/datlp/dsnap/releases/latest/download/dsnap-v1.2.0-windows-x64.zip) (~220 KB complete package with license & `dsnap.ini.example`)
* [🛡️ **SHA256SUMS.txt**](https://github.com/datlp/dsnap/releases/latest/download/SHA256SUMS.txt) (Cryptographic verification checksums)

---

## ⌨️ Keyboard Shortcuts

Global hotkeys are intercepted with instant responsiveness via low-level keyboard hook (`WH_KEYBOARD_LL`):

| Hotkey | Action | Output |
| :--- | :--- | :--- |
| <kbd>PrintScreen</kbd> | Freezes screen, select region to capture | Auto-copied to **Clipboard** (`Ctrl + V`) and saved to `Pictures\Screenshots` |
| <kbd>Shift</kbd> + <kbd>PrintScreen</kbd> | Freezes screen, select region to record MP4 video | Saves to `Videos\Captures\dsnap-recording-*.mp4` with floating toolbar |
| <kbd>Escape</kbd> | Dismisses freeze overlay or cancels selection | No files saved |

---

## 🖱️ System Tray Menu

Right-click the camera viewfinder icon in the system notification area to access:

* 📸 **Capture Screen** (`PrintScreen`)
* 🎥 **Record Screen** (`Shift + PrtScn`)
* 📁 **Open Screenshots Folder**
* 📁 **Open Recordings Folder**
* 🔊 **Record System Audio** *(Toggles WASAPI Output Loopback on/off)*
* 🎙️ **Record Microphone** *(Toggles Microphone capture on/off)*
* 🎧 **Audio Devices >** *(Select Default or multiple Speakers, Headphones & Microphones)*
* ✨ **Highlight Mouse Clicks** *(Toggles 60 FPS click ripple animations)*
* 🚀 **Start with Windows** *(Toggles Registry or zero-UAC Task Scheduler autostart)*
* 🌐 **Language >** *(Auto, English, Tiếng Việt, 简体中文, 한국어, 日本語)*
* **Exit** *(Terminates application and active recordings cleanly)*

---

## ✨ Features

* **Instant Freeze Engine:** `PrintScreen` triggers immediate GDI bit-block transfer before active menus or flyouts receive dismiss messages.
* **Orphan-Proof FFmpeg Controller:** FFmpeg child process is attached to a **Windows Job Object** (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`). Guarantees zero zombie processes or locked video files even if dsnap is abruptly terminated.
* **1-Click Automatic FFmpeg Setup:** If FFmpeg is missing from the system, dsnap prompts to download and extract portable FFmpeg from GitHub Releases CDN in the background using Windows native `tar.exe`.
* **Multi-Monitor & Mixed DPI Aware:** Per-Monitor DPI Awareness V2 spanning the entire virtual desktop (`SM_XVIRTUALSCREEN`, `SM_CXVIRTUALSCREEN`) with pixel-perfect rendering across mixed scale factors (100% to 200%).
* **High-Contrast Dual-Mode Icon:** Pixel-crafted camera viewfinder icon optimized for crisp clarity across both Windows Dark Mode and Light Mode taskbars.
* **Floating Control Bar:** Live timer badge (`● 00:00`), Pause/Resume via low-level `NtSuspendProcess` / `NtResumeProcess`, clean Stop, and instant Cancel (deletes partial video).

---

## 🛠️ Building from Source

### Prerequisites
* Windows 10 or 11 (64-bit)
* Any modern C++17 compiler (**LLVM Clang++**, **MinGW GCC**, or **MSVC**)

### Method 1: Root Build Script (`build.bat`)
Automatically detects your compiler and optimizes binary:
```cmd
git clone https://github.com/datlp/dsnap.git
cd dsnap
build.bat
```

### Method 2: Modern CMake
```cmd
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

## 📜 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.
