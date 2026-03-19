#!/usr/bin/env python3
"""
Real-time JPEG stream viewer for ESP32 camera over UART.

Imports uart.py as a module (no modifications needed) and displays
the camera stream in a tkinter window.

Usage:
    python3 uart_viewer.py --dev /dev/ttyS1 --baud 115200
"""

import argparse
import io
import queue
import struct
import sys
import threading
import time
import tkinter as tk

from PIL import Image, ImageTk

import uart

POLL_INTERVAL_MS = 5


def grab_frame(proto: uart.UartProtocol) -> bytes:
    """Request one JPEG frame and return the raw bytes."""
    rsp = proto.request(uart.CMD_GET_CAMERA_FRAME)
    if len(rsp) < 4:
        raise RuntimeError("frame response too short")
    frame_len = struct.unpack("<I", rsp[:4])[0]
    if frame_len == 0 or frame_len > uart.MAX_FRAME_SIZE:
        raise RuntimeError(f"invalid frame length {frame_len}")

    data = bytearray()
    if len(rsp) > 4:
        data.extend(rsp[4:])

    while len(data) < frame_len:
        ptype, _seq, payload = proto.recv_packet(uart.FRAME_CHUNK_TIMEOUT_S)
        if ptype != uart.RESP_FRAME_CHUNK:
            raise RuntimeError(f"unexpected packet type=0x{ptype:02X}")
        data.extend(payload)

    return bytes(data[:frame_len])


class FrameWorker(threading.Thread):
    """Background thread that continuously grabs frames from the camera."""

    def __init__(self, proto: uart.UartProtocol, frame_queue: queue.Queue):
        super().__init__(daemon=True)
        self.proto = proto
        self.q = frame_queue
        self._stop_event = threading.Event()

    def run(self):
        while not self._stop_event.is_set():
            try:
                jpeg = grab_frame(self.proto)
                self.q.put(jpeg)
            except Exception as exc:
                print(f"[worker] {exc}", file=sys.stderr)
                time.sleep(0.2)

    def stop(self):
        self._stop_event.set()


class ViewerApp:
    def __init__(self, root: tk.Tk, frame_queue: queue.Queue):
        self.root = root
        self.q = frame_queue
        self._tk_img = None
        self._frame_count = 0
        self._fps = 0.0
        self._last_fps_time = time.monotonic()
        self._last_size = 0

        root.title("ESP32 Camera Viewer")
        root.configure(bg="#1e1e1e")
        root.minsize(640, 480)

        self.canvas = tk.Canvas(root, bg="#1e1e1e", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True)

        self.status = tk.Label(
            root, text="Connecting...", anchor=tk.W,
            bg="#2d2d2d", fg="#cccccc", padx=8, pady=4,
            font=("monospace", 10),
        )
        self.status.pack(fill=tk.X, side=tk.BOTTOM)

        self.canvas.bind("<Configure>", self._on_resize)
        self._canvas_w = 640
        self._canvas_h = 480
        self._pil_img = None

        self._poll()

    def _on_resize(self, event):
        self._canvas_w = event.width
        self._canvas_h = event.height
        if self._pil_img:
            self._draw(self._pil_img)

    def _draw(self, img: Image.Image):
        iw, ih = img.size
        cw, ch = self._canvas_w, self._canvas_h
        scale = min(cw / iw, ch / ih)
        nw, nh = int(iw * scale), int(ih * scale)
        resized = img.resize((nw, nh), Image.LANCZOS)

        self._tk_img = ImageTk.PhotoImage(resized)
        self.canvas.delete("all")
        self.canvas.create_image(cw // 2, ch // 2, image=self._tk_img)

    def _poll(self):
        try:
            while True:
                jpeg = self.q.get_nowait()
                self._pil_img = Image.open(io.BytesIO(jpeg))
                self._draw(self._pil_img)
                self._last_size = len(jpeg)
                self._frame_count += 1
        except queue.Empty:
            pass

        now = time.monotonic()
        elapsed = now - self._last_fps_time
        if elapsed >= 1.0:
            self._fps = self._frame_count / elapsed
            self._frame_count = 0
            self._last_fps_time = now

        size_kb = self._last_size / 1024
        self.status.config(
            text=f"FPS: {self._fps:.1f}  |  Frame: {size_kb:.1f} KB  |  "
                 f"Resolution: {self._pil_img.size[0]}x{self._pil_img.size[1]}"
                 if self._pil_img else "Waiting for first frame..."
        )

        self.root.after(POLL_INTERVAL_MS, self._poll)


def main() -> int:
    parser = argparse.ArgumentParser(description="ESP32 Camera UART Stream Viewer")
    parser.add_argument("--dev", default="/dev/ttyS1", help="UART device")
    parser.add_argument("--baud", type=int, default=115200, help="baudrate")
    parser.add_argument("--timeout", type=float, default=uart.DEFAULT_TIMEOUT_S,
                        help="request timeout (seconds)")
    args = parser.parse_args()

    port = uart.UartPort(args.dev, args.baud)
    proto = uart.UartProtocol(port, timeout_s=args.timeout)

    frame_q: queue.Queue = queue.Queue(maxsize=2)
    worker = FrameWorker(proto, frame_q)
    worker.start()

    root = tk.Tk()
    ViewerApp(root, frame_q)

    try:
        root.mainloop()
    except KeyboardInterrupt:
        pass
    finally:
        worker.stop()
        worker.join(timeout=2)
        port.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
