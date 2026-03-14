#!/usr/bin/env python3
"""
SPI 收发测试脚本 —— 在 Buildroot 系统上通过 /dev/spidev2.0 与 ESP32 通信。
依赖: python3 内置模块 (struct, fcntl, ctypes, os)，无需额外 pip 包。

用法:
  python3 spi.py                # 默认: 读一次状态字节
  python3 spi.py poll           # 持续轮询 READY_CODE
  python3 spi.py cmd <hex>      # poll → 发命令 → poll
  python3 spi.py info           # 发 GET_CAMERA_INFO 命令并读取 CameraInfo
  python3 spi.py frame          # 发 GET_CAMERA_FRAME 命令并读取帧
  python3 spi.py loopback <hex> # 发送并同时接收（环回测试）
  python3 spi.py raw <hex...>   # 发送任意字节，打印返回
"""

import sys
import os
import struct
import time
import fcntl
import ctypes
import array

# ──────────────────────────── 协议常量（与 control.h 一致）
SPI_CMD_INIT            = 0x01
SPI_CMD_GET_CAMERA_INFO = 0x02
SPI_CMD_GET_CAMERA_FRAME= 0x03
READY_CODE              = 0xA5

POLL_INTERVAL_S         = 0.0002   # 200 us
POLL_TIMEOUT_S          = 2.0
FRAME_CHUNK_SIZE        = 4096
FRAME_MAX_SIZE          = 512 * 1024

# ──────────────────────────── SPI ioctl 定义
SPI_IOC_MAGIC   = ord('k')
SPI_IOC_WR_MODE          = 0x40016B01
SPI_IOC_RD_MODE          = 0x80016B01
SPI_IOC_WR_MAX_SPEED_HZ  = 0x40046B04
SPI_IOC_RD_MAX_SPEED_HZ  = 0x80046B04
SPI_IOC_WR_BITS_PER_WORD = 0x40016B03
SPI_IOC_RD_BITS_PER_WORD = 0x80016B03
# SPI_MODE_1 = 0x03
SPI_MODE_1 = 0x0

def SPI_IOC_MESSAGE(n):
    # _IOW(SPI_IOC_MAGIC, 0, struct spi_ioc_transfer[n])
    return 0x40006B00 | (n * 32) << 16


class SpiIocTransfer(ctypes.Structure):
    _fields_ = [
        ("tx_buf",        ctypes.c_uint64),
        ("rx_buf",        ctypes.c_uint64),
        ("len",           ctypes.c_uint32),
        ("speed_hz",      ctypes.c_uint32),
        ("delay_usecs",   ctypes.c_uint16),
        ("bits_per_word", ctypes.c_uint8),
        ("cs_change",     ctypes.c_uint8),
        ("tx_nbits",      ctypes.c_uint8),
        ("rx_nbits",      ctypes.c_uint8),
        ("pad",           ctypes.c_uint16),
    ]


class SpiDev:
    """通过 ioctl 直接操作 /dev/spidevX.Y"""

    def __init__(self, bus=2, cs=0, mode=SPI_MODE_1, speed=1_000_000):
        path = f"/dev/spidev{bus}.{cs}"
        self.fd = os.open(path, os.O_RDWR)

        buf = struct.pack("B", mode)
        fcntl.ioctl(self.fd, SPI_IOC_WR_MODE, buf)

        buf = struct.pack("I", speed)
        fcntl.ioctl(self.fd, SPI_IOC_WR_MAX_SPEED_HZ, buf)

        buf = struct.pack("B", 8)
        fcntl.ioctl(self.fd, SPI_IOC_WR_BITS_PER_WORD, buf)

        self.speed = speed
        print(f"[spi] opened {path}  mode={mode} speed={speed}")

    def close(self):
        os.close(self.fd)

    def xfer(self, tx_data):
        """全双工传输: 发 tx_data 同时收相同长度的数据"""
        n = len(tx_data)
        tx = (ctypes.c_uint8 * n)(*tx_data)
        rx = (ctypes.c_uint8 * n)()

        xfer = SpiIocTransfer()
        xfer.tx_buf = ctypes.addressof(tx)
        xfer.rx_buf = ctypes.addressof(rx)
        xfer.len = n
        xfer.speed_hz = self.speed
        xfer.bits_per_word = 8

        fcntl.ioctl(self.fd, SPI_IOC_MESSAGE(1), xfer)
        time.sleep(0.01)
        return bytes(rx)

    def write(self, data):
        """只发不收"""
        n = len(data)
        tx = (ctypes.c_uint8 * n)(*data)

        xfer = SpiIocTransfer()
        xfer.tx_buf = ctypes.addressof(tx)
        xfer.rx_buf = 0
        xfer.len = n
        xfer.speed_hz = self.speed
        xfer.bits_per_word = 8

        fcntl.ioctl(self.fd, SPI_IOC_MESSAGE(1), xfer)
        time.sleep(0.01)

    def read(self, length):
        """只收不发（MOSI 输出 0x00）"""
        rx = (ctypes.c_uint8 * length)()

        xfer = SpiIocTransfer()
        xfer.tx_buf = 0
        xfer.rx_buf = ctypes.addressof(rx)
        xfer.len = length
        xfer.speed_hz = self.speed
        xfer.bits_per_word = 8

        fcntl.ioctl(self.fd, SPI_IOC_MESSAGE(1), xfer)
        time.sleep(0.01)
        return bytes(rx)


# ──────────────────────────── 协议操作
def hex_dump(data, prefix="", width=16):
    for i in range(0, len(data), width):
        chunk = data[i:i+width]
        hexs = " ".join(f"{b:02x}" for b in chunk)
        asciis = "".join(chr(b) if 0x20 <= b < 0x7f else "." for b in chunk)
        print(f"{prefix}{i:04x}: {hexs:<{width*3}}  {asciis}")


def poll_ready(spi, verbose=True):
    """轮询 READY_CODE，成功返回 True"""
    deadline = time.monotonic() + POLL_TIMEOUT_S
    count = 0
    while time.monotonic() < deadline:
        rx = spi.read(1)
        count += 1
        if rx[0] == READY_CODE:
            if verbose:
                print(f"[poll] READY (0x{READY_CODE:02X}) after {count} reads")
            return True
        time.sleep(POLL_INTERVAL_S)
    print(f"[poll] TIMEOUT after {count} reads ({POLL_TIMEOUT_S}s)")
    return False


def send_command(spi, cmd):
    """全双工发 cmd，同时收到 READY_CODE 才算成功，再 poll 等执行完毕"""
    print(f"[cmd] sending 0x{cmd:02X} ...")

    deadline = time.monotonic() + POLL_TIMEOUT_S
    count = 0
    while time.monotonic() < deadline:
        rx = spi.xfer([cmd])
        count += 1
        if rx[0] == READY_CODE:
            print(f"[cmd] 0x{cmd:02X} accepted (got READY on xfer, {count} tries)")
            break
        print(f"got wrong data 0x{rx[0]:02X}")
        time.sleep(POLL_INTERVAL_S)
    else:
        print(f"[cmd] 0x{cmd:02X} send TIMEOUT after {count} tries")
        return False

    # print(f"[cmd] waiting slave to finish processing...")
    # if not poll_ready(spi):
    #     print("[cmd] slave not ready after exec, abort")
    #     return False

    print(f"[cmd] 0x{cmd:02X} done")
    return True


def cmd_poll(spi):
    """持续轮询，打印每次读到的值"""
    print("[poll] press Ctrl+C to stop")
    try:
        while True:
            rx = spi.read(1)
            ts = time.strftime("%H:%M:%S")
            print(f"  {ts}  MISO: 0x{rx[0]:02X}" +
                  (" ← READY" if rx[0] == READY_CODE else ""))
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n[poll] stopped")


def cmd_info(spi):
    """发送 GET_CAMERA_INFO 并解析 CameraInfo"""
    if not send_command(spi, SPI_CMD_GET_CAMERA_INFO):
        return

    # time.sleep(0.01)
    time.sleep(5)
    data = spi.read(6)  # sizeof(CameraInfo) = 2+2+1+1 = 6
    print("[info] raw bytes:")
    hex_dump(data, "  ")
    # poll_ready(spi)

    width, height, fmt, connected = struct.unpack("<HHBB", data[:6])
    print(f"[info] width={width} height={height} format={fmt} connected={connected}")


def cmd_frame(spi):
    """发送 GET_CAMERA_FRAME，读取帧长度，再分块读取帧数据"""
    if not send_command(spi, SPI_CMD_GET_CAMERA_FRAME):
        return

    len_bytes = spi.read(4)
    frame_len = struct.unpack("<I", len_bytes)[0]
    print(f"[frame] length = {frame_len} bytes")

    if frame_len == 0 or frame_len > FRAME_MAX_SIZE:
        print(f"[frame] invalid length, abort")
        return

    buf = bytearray()
    remaining = frame_len
    while remaining > 0:
        chunk = min(FRAME_CHUNK_SIZE, remaining)
        data = spi.read(chunk)
        buf.extend(data)
        remaining -= chunk
        pct = (len(buf) * 100) // frame_len
        print(f"\r[frame] reading... {len(buf)}/{frame_len} ({pct}%)", end="", flush=True)

    print()
    print(f"[frame] done, {len(buf)} bytes received")
    hex_dump(buf[:64], "  ")
    if len(buf) > 64:
        print(f"  ... ({len(buf) - 64} more bytes)")

    out_path = "/tmp/esp_frame.bin"
    with open(out_path, "wb") as f:
        f.write(buf)
    print(f"[frame] saved to {out_path}")


def cmd_loopback(spi, hex_str):
    """全双工环回测试"""
    tx = bytes.fromhex(hex_str)
    print(f"[loopback] TX ({len(tx)} bytes):")
    hex_dump(tx, "  ")
    rx = spi.xfer(tx)
    print(f"[loopback] RX ({len(rx)} bytes):")
    hex_dump(rx, "  ")
    if tx == rx:
        print("[loopback] PASS: TX == RX")
    else:
        print("[loopback] MISMATCH: TX != RX")


def cmd_raw(spi, hex_args):
    """发送任意字节"""
    tx = bytes.fromhex("".join(hex_args))
    print(f"[raw] TX ({len(tx)} bytes):")
    hex_dump(tx, "  ")
    rx = spi.xfer(tx)
    print(f"[raw] RX ({len(rx)} bytes):")
    hex_dump(rx, "  ")


def cmd_read_once(spi):
    """读一次状态"""
    rx = spi.read(1)
    print(f"[read] MISO: 0x{rx[0]:02X}" +
          (" (READY)" if rx[0] == READY_CODE else ""))


def usage():
    print(__doc__)
    sys.exit(1)


def main():
    spi = SpiDev(bus=2, cs=0, mode=SPI_MODE_1, speed=1_000_000)

    try:
        if len(sys.argv) < 2:
            cmd_read_once(spi)
            return

        sub = sys.argv[1].lower()

        if sub == "poll":
            cmd_poll(spi)
        elif sub == "cmd":
            if len(sys.argv) < 3:
                usage()
            send_command(spi, int(sys.argv[2], 16))
        elif sub == "info":
            cmd_info(spi)
        elif sub == "frame":
            cmd_frame(spi)
        elif sub == "loopback":
            if len(sys.argv) < 3:
                usage()
            cmd_loopback(spi, sys.argv[2])
        elif sub == "raw":
            if len(sys.argv) < 3:
                usage()
            cmd_raw(spi, sys.argv[2:])
        else:
            usage()
    finally:
        spi.close()


if __name__ == "__main__":
    main()
