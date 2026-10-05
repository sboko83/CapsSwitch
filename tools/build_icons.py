"""Build Windows ICO frames with GDI+ and the Python standard library."""

import argparse
import ctypes as ct
from ctypes import wintypes as wt
from pathlib import Path
import shutil
import struct
import zlib

SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
ARGB = 0x26200A


class StartupInput(ct.Structure):
    _fields_ = [("version", wt.UINT), ("callback", ct.c_void_p),
                ("suppress_thread", wt.BOOL), ("suppress_codecs", wt.BOOL)]


class Rect(ct.Structure):
    _fields_ = [("x", ct.c_int), ("y", ct.c_int),
                ("width", ct.c_int), ("height", ct.c_int)]


class BitmapData(ct.Structure):
    _fields_ = [("width", wt.UINT), ("height", wt.UINT), ("stride", ct.c_int),
                ("format", ct.c_int), ("scan", ct.c_void_p), ("reserved", ct.c_size_t)]


def check(status):
    if status:
        raise RuntimeError("GDI+ status " + str(status))


def pixels(gdi, bitmap, width, height):
    data = BitmapData()
    rect = Rect(0, 0, width, height)
    check(gdi.GdipBitmapLockBits(bitmap, ct.byref(rect), 1, ARGB, ct.byref(data)))
    try:
        return b"".join(ct.string_at(data.scan + y * data.stride, width * 4)
                        for y in range(height))
    finally:
        check(gdi.GdipBitmapUnlockBits(bitmap, ct.byref(data)))


def frame(gdi, source, bounds, size):
    bitmap = ct.c_void_p()
    graphics = ct.c_void_p()
    check(gdi.GdipCreateBitmapFromScan0(size, size, 0, ARGB, None, ct.byref(bitmap)))
    try:
        check(gdi.GdipGetImageGraphicsContext(bitmap, ct.byref(graphics)))
        try:
            check(gdi.GdipGraphicsClear(graphics, 0))
            check(gdi.GdipSetInterpolationMode(graphics, 7))  # HighQualityBicubic
            check(gdi.GdipSetPixelOffsetMode(graphics, 4))  # Half
            left, top, width, height = bounds
            # One transparent pixel at tray sizes; proportional padding above 32 px.
            padding = max(1, round(size * 0.03))
            scale = (size - 2 * padding) / max(width, height)
            dest_width, dest_height = round(width * scale), round(height * scale)
            x, y = (size - dest_width) // 2, (size - dest_height) // 2
            check(gdi.GdipDrawImageRectRectI(graphics, source, x, y, dest_width, dest_height,
                                            left, top, width, height, 2, None, None, None))
        finally:
            check(gdi.GdipDeleteGraphics(graphics))
        return pixels(gdi, bitmap, size, size)
    finally:
        check(gdi.GdipDisposeImage(bitmap))


def png(width, height, bgra):
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(
            ">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    rgba = bytearray(bgra)
    rgba[0::4], rgba[2::4] = bgra[2::4], bgra[0::4]
    rows = b"".join(b"\0" + rgba[y * width * 4:(y + 1) * width * 4]
                    for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def ico(frames):
    payloads = [png(size, size, data) for size, data in frames]
    offset = 6 + 16 * len(frames)
    directory = bytearray(struct.pack("<HHH", 0, 1, len(frames)))
    for (size, _), payload in zip(frames, payloads):
        directory += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0,
                                 1, 32, len(payload), offset)
        offset += len(payload)
    return directory + b"".join(payloads)


def preview(frames, paused):
    width, height = 940, 350
    board = bytearray(width * height * 4)
    for y in range(height):
        background = 249 if y < height // 2 else 32
        board[y * width * 4:(y + 1) * width * 4] = bytes(
            (background, background, background, 255)) * width
    for row in range(2):
        x = 12
        for size, data in frames:
            if size > 128:
                continue
            for y in range(size):
                for px in range(size):
                    source = (y * size + px) * 4
                    dest = ((row * 175 + 18 + y) * width + x + px) * 4
                    alpha = data[source + 3]
                    for channel in range(3):
                        board[dest + channel] = (data[source + channel] * alpha
                                                 + board[dest + channel] * (255 - alpha) + 127) // 255
            x += size + 18
        # The 16 px active and paused frames enlarged 6x without smoothing.
        for col, data in enumerate((frames[0][1], paused[0][1])):
            for y in range(96):
                for px in range(96):
                    source = ((y // 6) * 16 + px // 6) * 4
                    dest = ((row * 175 + 18 + y) * width + 710 + col * 110 + px) * 4
                    alpha = data[source + 3]
                    for channel in range(3):
                        board[dest + channel] = (data[source + channel] * alpha
                                                 + board[dest + channel] * (255 - alpha) + 127) // 255
    return png(width, height, board)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--apply", action="store_true", help="Write assets; default prints the plan")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source_path = (args.source or root / "assets/CapsSwitch.png").resolve()
    gdi = ct.WinDLL("gdiplus")
    # Specify pointer-bearing APIs to avoid truncation on 64-bit Windows.
    ptr = ct.c_void_p
    signatures = {
        "GdiplusStartup": [ct.POINTER(ct.c_size_t), ct.POINTER(StartupInput), ptr],
        "GdiplusShutdown": [ct.c_size_t],
        "GdipCreateBitmapFromFile": [wt.LPCWSTR, ct.POINTER(ptr)],
        "GdipGetImageWidth": [ptr, ct.POINTER(wt.UINT)],
        "GdipGetImageHeight": [ptr, ct.POINTER(wt.UINT)],
        "GdipBitmapLockBits": [ptr, ct.POINTER(Rect), wt.UINT, ct.c_int, ct.POINTER(BitmapData)],
        "GdipBitmapUnlockBits": [ptr, ct.POINTER(BitmapData)],
        "GdipCreateBitmapFromScan0": [ct.c_int, ct.c_int, ct.c_int, ct.c_int, ptr, ct.POINTER(ptr)],
        "GdipGetImageGraphicsContext": [ptr, ct.POINTER(ptr)],
        "GdipGraphicsClear": [ptr, wt.UINT],
        "GdipSetInterpolationMode": [ptr, ct.c_int],
        "GdipSetPixelOffsetMode": [ptr, ct.c_int],
        "GdipDrawImageRectRectI": [ptr, ptr] + [ct.c_int] * 9 + [ptr, ptr, ptr],
        "GdipDeleteGraphics": [ptr],
        "GdipDisposeImage": [ptr],
    }
    for name, signature in signatures.items():
        getattr(gdi, name).argtypes = signature
    token = ct.c_size_t()
    check(gdi.GdiplusStartup(ct.byref(token), ct.byref(StartupInput(1)), None))
    source = ptr()
    try:
        check(gdi.GdipCreateBitmapFromFile(str(source_path), ct.byref(source)))
        width, height = wt.UINT(), wt.UINT()
        check(gdi.GdipGetImageWidth(source, ct.byref(width)))
        check(gdi.GdipGetImageHeight(source, ct.byref(height)))
        data = pixels(gdi, source, width.value, height.value)
        opaque = [(i % width.value, i // width.value)
                  for i, alpha in enumerate(data[3::4]) if alpha >= 128]
        left = min(x for x, _ in opaque)
        top = min(y for _, y in opaque)
        bounds = (left, top, max(x for x, _ in opaque) - left + 1,
                  max(y for _, y in opaque) - top + 1)
        print("Source:", source_path)
        print("Crop:", bounds, "Frames:", ", ".join(map(str, SIZES)))
        print("Write: assets/CapsSwitch.png, assets/CapsSwitch.ico, "
              "assets/CapsSwitch.Paused.ico, obj/icon-sizes.png")
        if not args.apply:
            print("Dry run: no files changed.")
            return
        frames = [(size, frame(gdi, source, bounds, size)) for size in SIZES]
        paused = []
        for size, data in frames:
            dimmed = bytearray(data)
            dimmed[3::4] = bytes((alpha * 150 + 127) // 255 for alpha in data[3::4])
            paused.append((size, dimmed))
        target = root / "assets/CapsSwitch.png"
        if source_path != target.resolve():
            shutil.copyfile(source_path, target)
        (root / "assets/CapsSwitch.ico").write_bytes(ico(frames))
        (root / "assets/CapsSwitch.Paused.ico").write_bytes(ico(paused))
        (root / "obj").mkdir(exist_ok=True)
        (root / "obj/icon-sizes.png").write_bytes(preview(frames, paused))
        print("Generated active and paused icons, 10 frames each, plus preview.")
    finally:
        if source:
            gdi.GdipDisposeImage(source)
        gdi.GdiplusShutdown(token)


if __name__ == "__main__":
    main()
