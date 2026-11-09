"""
dsnap - Python Standalone Edition
A lightweight region screenshot and screen recorder powered by Tkinter and FFmpeg.
"""

import os
import sys
import datetime
import subprocess
import ctypes
from ctypes import wintypes
import tkinter as tk

# Enable DPI awareness
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    try:
        ctypes.windll.user32.SetProcessDPIAware()
    except Exception:
        pass

ffmpeg_process = None

def get_known_folder(folder_guid):
    class GUID(ctypes.Structure):
        _fields_ = [
            ("Data1", wintypes.DWORD),
            ("Data2", wintypes.WORD),
            ("Data3", wintypes.WORD),
            ("Data4", wintypes.BYTE * 8)
        ]
    # FOLDERID_Screenshots: {b7bede81-df94-4682-a7d8-57a52620b86f}
    # FOLDERID_Captures:    {edc0fe99-aa4b-4770-81a2-056ee3dec1ab}
    # Fallback to standard User directories
    user_home = os.path.expanduser("~")
    if folder_guid == "screenshots":
        path = os.path.join(user_home, "Pictures", "Screenshots")
    else:
        path = os.path.join(user_home, "Videos", "Captures")
    os.makedirs(path, exist_ok=True)
    return path

def find_ffmpeg():
    # 1. Same directory
    local_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ffmpeg.exe")
    if os.path.isfile(local_path):
        return local_path
    # 2. In PATH
    import shutil
    ffmpeg_in_path = shutil.which("ffmpeg")
    if ffmpeg_in_path:
        return ffmpeg_in_path
    return None

def get_timestamp():
    return datetime.datetime.now().strftime("%Y%m%d-%H%M%S")

def save_screenshot(x, y, w, h):
    # Take screenshot via PIL / ImageGrab or PowerShell/Win32
    from PIL import ImageGrab
    bbox = (x, y, x + w, y + h)
    img = ImageGrab.grab(bbox=bbox, all_screens=True)
    
    out_dir = get_known_folder("screenshots")
    filepath = os.path.join(out_dir, f"dsnap-screenshot-{get_timestamp()}.png")
    img.save(filepath, "PNG")

    # Copy to clipboard
    try:
        import io
        output = io.BytesIO()
        img.convert("RGB").save(output, "BMP")
        data = output.getvalue()[14:] # Strip BMP header
        output.close()

        ctypes.windll.user32.OpenClipboard(0)
        ctypes.windll.user32.EmptyClipboard()
        CF_DIB = 8
        h_mem = ctypes.windll.kernel32.GlobalAlloc(0x0042, len(data))
        p_mem = ctypes.windll.kernel32.GlobalLock(h_mem)
        ctypes.cdll.msvcrt.memcpy(ctypes.c_void_p(p_mem), data, len(data))
        ctypes.windll.kernel32.GlobalUnlock(h_mem)
        ctypes.windll.user32.SetClipboardData(CF_DIB, h_mem)
        ctypes.windll.user32.CloseClipboard()
    except Exception as e:
        print(f"Clipboard error: {e}")

    try:
        ctypes.windll.user32.MessageBeep(0)
    except Exception:
        pass

def start_recording(x, y, w, h):
    global ffmpeg_process
    w = w - (w % 2)
    h = h - (h % 2)
    if w <= 0 or h <= 0:
        return

    ffmpeg_bin = find_ffmpeg()
    if not ffmpeg_bin:
        ctypes.windll.user32.MessageBoxW(
            0, "Không tìm thấy ffmpeg.exe trong hệ thống!", "dsnap - Lỗi", 0x30
        )
        return

    out_dir = get_known_folder("captures")
    output_path = os.path.join(out_dir, f"dsnap-recording-{get_timestamp()}.mp4")

    cmd = [
        ffmpeg_bin,
        "-y",
        "-f", "gdigrab",
        "-framerate", "30",
        "-offset_x", str(x),
        "-offset_y", str(y),
        "-video_size", f"{w}x{h}",
        "-i", "desktop",
        "-c:v", "libx264",
        "-preset", "ultrafast",
        "-crf", "23",
        "-pix_fmt", "yuv420p",
        output_path
    ]

    creationflags = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
    ffmpeg_process = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=creationflags
    )

    try:
        ctypes.windll.user32.MessageBeep(0x40) # Asterisk beep
    except Exception:
        pass

def stop_recording():
    global ffmpeg_process
    if ffmpeg_process and ffmpeg_process.poll() is None:
        try:
            ffmpeg_process.communicate(input=b"q\n", timeout=4)
        except Exception:
            ffmpeg_process.kill()
        finally:
            ffmpeg_process = None
        try:
            ctypes.windll.user32.MessageBeep(0)
        except Exception:
            pass

class SnippingOverlay:
    def __init__(self, mode="photo"):
        self.mode = mode
        self.root = tk.Tk()
        self.root.overrideredirect(True)
        self.root.attributes("-alpha", 0.25)
        self.root.attributes("-topmost", True)
        self.root.config(bg="black", cursor="cross")

        # Multi-monitor virtual screen metrics
        u32 = ctypes.windll.user32
        SM_XVIRTUALSCREEN = 76
        SM_YVIRTUALSCREEN = 77
        SM_CXVIRTUALSCREEN = 78
        SM_CYVIRTUALSCREEN = 79

        vx = u32.GetSystemMetrics(SM_XVIRTUALSCREEN)
        vy = u32.GetSystemMetrics(SM_YVIRTUALSCREEN)
        vw = u32.GetSystemMetrics(SM_CXVIRTUALSCREEN)
        vh = u32.GetSystemMetrics(SM_CYVIRTUALSCREEN)

        self.root.geometry(f"{vw}x{vh}+{vx}+{vy}")

        self.canvas = tk.Canvas(self.root, cursor="cross", bg="black", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True)

        self.start_x = 0
        self.start_y = 0
        self.rect = None
        self.coords = None

        self.canvas.bind("<ButtonPress-1>", self.on_press)
        self.canvas.bind("<B1-Motion>", self.on_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_release)
        self.root.bind("<Escape>", lambda e: self.root.destroy())

    def on_press(self, event):
        self.start_x = event.x_root
        self.start_y = event.y_root
        border = "#00a2e8" if self.mode == "photo" else "#ff3232"
        self.rect = self.canvas.create_rectangle(
            event.x, event.y, event.x, event.y, outline=border, width=2
        )

    def on_drag(self, event):
        self.canvas.coords(self.rect, self.start_x, self.start_y, event.x, event.y)

    def on_release(self, event):
        end_x = event.x_root
        end_y = event.y_root
        x1 = min(self.start_x, end_x)
        y1 = min(self.start_y, end_y)
        w = abs(end_x - self.start_x)
        h = abs(end_y - self.start_y)
        if w > 10 and h > 10:
            self.coords = (x1, y1, w, h)
        self.root.destroy()

    def get_selection(self):
        self.root.mainloop()
        return self.coords

def trigger_photo():
    overlay = SnippingOverlay(mode="photo")
    region = overlay.get_selection()
    if region:
        save_screenshot(*region)

def trigger_video():
    global ffmpeg_process
    if ffmpeg_process and ffmpeg_process.poll() is None:
        stop_recording()
    else:
        overlay = SnippingOverlay(mode="video")
        region = overlay.get_selection()
        if region:
            start_recording(*region)

if __name__ == "__main__":
    import keyboard
    print("dsnap is running in background...")
    print("PrtScn: Screenshot region -> Clipboard & File")
    print("Shift + PrtScn: Screen recording start/stop")
    keyboard.add_hotkey("print screen", trigger_photo)
    keyboard.add_hotkey("shift+print screen", trigger_video)
    keyboard.wait()

