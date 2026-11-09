# GitHub Issues Roadmap for datlp/dsnap 🚀

This document tracks the technical specifications, architectural designs, and progress of the core issues in `datlp/dsnap`, aligned with standard open-source conventions.

---

## Issue #1: [Core] Initial Win32 native architecture and FFmpeg controller

**Labels:** `enhancement`, `core`, `win32`

### Description
Implement the core architecture of `dsnap`: an ultra-lightweight (< 400 KB) native screen capture and recording utility written in pure C++ Win32 API with zero external runtime dependencies.

### Tasks
- [x] Register global keyboard hooks via Low-Level Keyboard Hook (`WH_KEYBOARD_LL`) for `PrintScreen` and `Shift + PrintScreen`.
- [x] Implement semi-transparent selection overlay window (`WS_EX_LAYERED`).
- [x] Integrate GDI `BitBlt` snapshotting and GDI+ PNG Encoder to save images into `Pictures\Screenshots` and copy to Clipboard (`CF_BITMAP`).
- [x] Automatically discover `ffmpeg.exe` in local directory or system `PATH`.
- [x] Launch background FFmpeg process via Anonymous Pipes, sending graceful `'q'` command to finalize MP4 containers.
- [x] Enable Per-Monitor DPI Awareness V2 and full virtual screen spanning.

**Commit:** `feat: initial Win32 native implementation of dsnap (#1)`

---

## Issue #2: [UI] Add high-contrast dual-mode icon for Dark & Light taskbars

**Labels:** `ui`, `enhancement`, `design`

### Description
The application icon in the system tray was difficult to see on contrasting Windows Taskbar color themes (e.g. pure dark vs. bright light mode).

### Solution
- Create a multi-resolution `dsnap.ico` file containing 7 standard sizes (`16x16`, `24x24`, `32x32`, `48x48`, `64x64`, `128x128`, `256x256`).
- Render camera viewfinder silhouette with an electric blue accent that pops brightly on dark themes.
- Add dual-contrast stroke and drop-shadow to guarantee sharp 100% visibility on light themes.
- Pixel-hint the small 16x16 icon for tray clarity.
- Embed resources via `res/dsnap.rc` (`IDI_APPICON 101`).

**Commit:** `feat(ui): add high-contrast dual-mode pixel-crafted icon (#2)`

---

## Issue #3: [Overlay] Fix stacking overlay transparency and nested message loops

**Labels:** `bug`, `ui`, `performance`

### Description
Pressing `PrintScreen` multiple times in rapid succession caused overlapping translucent windows to make the screen darker and induced application freezing.

### Root Cause
- Missing state guard for active overlay (`g_isOverlayActive`).
- Each keypress spawned a new `WS_EX_LAYERED` window with stacked alpha opacity ($1 - (1 - 0.24)^n$).
- Each overlay spawned a nested `while (GetMessage)` loop breaking message flow.

### Solution
- Register `DsnapOverlay` window class once at application startup.
- Unify event dispatching into a single centralized message loop in `WinMain`.
- Add `g_isOverlayActive` guard to ignore duplicate triggers while selection is active.

**Commit:** `fix: prevent duplicate overlay stacking and eliminate nested message loops (#3)`

---

## Issue #4: [Capture] Implement Freeze Desktop Snapshot to capture context menus

**Labels:** `bug`, `core`, `ux`

### Description
Right-clicking the system tray or open menus and pressing `PrintScreen` caused menus to dismiss immediately before they could be captured.

### Root Cause
- Windows Shell (`explorer.exe`) claims exclusive mouse capture via `SetCapture()` while menus are open.
- When an overlay window activates, the focus change triggers Windows to dismiss the popup menu in under 1ms.

### Solution
- Switch to a **Freeze Desktop Snapshot** architecture:
  1. Instantly blit the full virtual desktop to RAM via `BitBlt` within 2ms before Windows dismisses menus.
  2. Release mouse capture (`EndMenu`).
  3. Render the frozen snapshot and display real-time spotlight illumination with double-buffering at 60+ FPS.
  4. Provide direct capture and record options in the system tray context menu.

**Commit:** `fix: implement Freeze Desktop Snapshot to capture system tray and context menus (#4)`

---

## Issue #5: [Recorder] Add non-intrusive region border and floating control toolbar

**Labels:** `enhancement`, `recorder`, `ui`

### Description
During screen recording, users had no visual indicator of the active recording boundary and lacked mouse controls to pause or stop recording.

### Solution
- **Highlight Border around Recording Area:**
  - 3px perimeter border with `WS_EX_TRANSPARENT` for 100% mouse click-through.
  - Excluded from video stream via `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`.
- **Top-Right Floating Recording Toolbar:**
  - Real-time elapsed timer `● MM:SS`.
  - `⏸ / ▶` (Pause / Resume) via low-level `NtSuspendProcess` / `NtResumeProcess`.
  - `⏹` (Stop & Save): Finalizes MP4 recording cleanly.
  - `✕` (Cancel): Instantly terminates and deletes temporary video file.

**Commit:** `feat(recorder): add click-through region border and top-right floating toolbar (#5)`

---

## Issue #6: [Audio] Add optional DirectShow audio recording toggle

**Labels:** `enhancement`, `audio`, `recorder`

### Description
Users requested optional audio recording capability (Microphone or Stereo Mix) during screen capture, defaulting to off for minimal file sizes.

### Solution
- Automatically enumerate available audio input devices via DirectShow `CLSID_AudioInputDeviceCategory`.
- Add toggle menu item in system tray: `[ ] 🎙️ Record Audio` (defaults to disabled).
- Append AAC audio stream arguments (`-c:a aac -b:a 128k`) when enabled.
- Persist setting in Windows Registry (`HKCU\Software\dsnap\RecordAudio`).

**Commit:** `feat(audio): add optional audio recording toggle in tray menu (#6)`

---

## Issue #7: [App] Enforce strict single-instance lifecycle with Global Mutex

**Labels:** `enhancement`, `stability`, `win32`

### Description
Prevent users from launching multiple instances of `dsnap.exe`, which created conflicting keyboard hooks and duplicate tray icons.

### Solution
- **Two-Layer Protection:**
  - Probe background window with `FindWindowW(BG_CLASS, L"dsnap_bg")`. If present, wake running instance and exit immediately.
  - Named Mutex: `Global\Dsnap_SingleInstance_Mutex_9999` across all user sessions.
- **User Feedback:** Running instance displays a balloon tip: *"dsnap is already running in the system tray!"*.
- **Explorer Crash Recovery:** Handles `RegisterWindowMessageW(L"TaskbarCreated")` to re-register tray icon if Windows Explorer restarts.

**Commit:** `fix: enforce single-instance lifecycle using Global Mutex and FindWindow (#7)`
