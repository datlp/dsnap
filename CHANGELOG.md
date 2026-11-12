# Changelog 📜

All notable changes to **dsnap** will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.1.2] - 2026-11-13

### Added
- **Installer "Always Run as Administrator" Option (Issue #21):** Added a privilege checkbox in the Inno Setup wizard (`Select Additional Tasks`) that configures Windows Compatibility Layer (`AppCompatFlags\Layers` `~ RUNASADMIN`) and launches `dsnap` elevated after setup.
- **Zero-UAC Elevated Autostart via Task Scheduler (Issue #19):** Implemented Task Scheduler XML 1.2 engine (`dsnap_Autostart` with `HighestAvailable`, `InteractiveToken`, `DisallowStartIfOnBatteries=false`, and unlimited execution time `PT0S`) plus `--register-admin-task` / `--unregister-admin-task` CLI switches so elevated installations start automatically with Windows without UAC prompts or laptop battery restrictions.
- **Unified Autostart Synchronization:** Synchronized the system tray `Start with Windows` toggle across both standard Registry `Run` and elevated Task Scheduler modes.

---

## [1.1.1] - 2026-11-12

### Fixed
- **UTF-8 Source Encoding Mojibake (Issue #20):** Added `/utf-8` compiler flag to MSVC invocations in CI workflow and build scripts, preventing MSVC from misinterpreting multi-byte international strings as Windows-1252 ANSI.
- **Source Charset Standard:** Re-encoded internationalized source files (`src/i18n.cpp`, `src/tray.cpp`, `src/overlay.cpp`) with explicit UTF-8 BOM.
- **Refined Tray Typography:** Polished menu labels across all 5 languages (English, Tiếng Việt, 简体中文, 한국어, 日本語) for clean native Windows rendering without surrogate-pair emoji glitches.

---

## [1.1.0] - 2026-11-12

### Added
- **Hybrid Hotkey Engine (Issue #18):** Kernel-level `RegisterHotKey` for `PrintScreen` and `Shift + PrintScreen` combined with `WH_KEYBOARD_LL` fallback, allowing instant captures over elevated windows (CrystalDiskInfo, Task Manager, Registry Editor, Admin terminals) bypassing Windows UIPI.
- **Visual Mouse Click Effects (Issue #16):** 60 FPS smooth expanding ripple animations for screencasting (Cyan for Left-Click, Coral Red for Right-Click) with tray toggle and 0% idle CPU overhead.
- **Multi-Language Localization (Issue #17):** 5-language native i18n lookup table (English, Tiếng Việt, 简体中文, 한국어, 日本語) with auto-detection and runtime tray language switcher.
- **Multilingual Installer:** Inno Setup installer with native English, Japanese, and Korean wizard translations.
- **Portable Configuration Template:** Added `dsnap.ini.example` detailing all zero-registry portable options (Issue #11).

### Changed
- Bumped application version to `1.1.0` in `res/version.h` (`VERSIONINFO`) and installer.
- Refactored `src/hotkey.cpp` with 250ms debounce de-duplication guard.

---

## [1.0.2] - 2026-11-09

### Added
- Dual-mode Inno Setup installer supporting All-Users (`C:\Program Files\dsnap`) and Current User (`%LocalAppData%\Programs\dsnap`) (Issue #10).
- Zero-registry Portable Mode via `dsnap.ini` and `--portable` / `-p` flags (Issue #11).
- Versioned distribution assets pipeline with SHA256 checksums (Issue #12, #14).
- Embedded `VERSIONINFO` metadata resource in `dsnap.exe` (Issue #15).
- Windows Package Manager (Winget) package manifest definition (Issue #13).

---

## [1.0.0] - 2026-11-08

### Added
- Initial ultra-lightweight Win32 native C++17 screen capture and recording utility (< 400 KB).
- Freeze Desktop Snapshot architecture capturing active context menus and tray flyouts (Issue #4).
- Automatic FFmpeg discovery and background downloader for MP4 hardware encoding (Issue #1, #8).
- Floating recording control toolbar with pause/resume and live elapsed timer (Issue #5).
- DirectShow system loopback audio recording toggle (Issue #6).
- Global mutex single-instance application lifecycle enforcement (Issue #7).

