#!/usr/bin/env python3
"""make_icon.py — build a macOS .icns from a PNG.

    ./make_icon.py icon.png OpenTyrian.icns

The image is scaled to 1024x1024 and given the rounded-square mask macOS
icons use, then handed to iconutil.  Standard library only; sips and
iconutil ship with macOS.
"""
import os
import struct
import subprocess
import sys
import tempfile
import zlib

S = 1024                      # master size
CORNER = 232                  # macOS icon-grid corner radius at 1024


def write_png(path, w, h, rgba):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(rgba[y * w * 4:(y + 1) * w * 4])
                   for y in range(h))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    open(path, "wb").write(png)


def rounded_alpha(x, y):
    rx = min(x, S - 1 - x)
    ry = min(y, S - 1 - y)
    if rx >= CORNER or ry >= CORNER:
        return 255
    dx, dy = CORNER - rx, CORNER - ry
    d = (dx * dx + dy * dy) ** 0.5
    if d <= CORNER - 1:
        return 255
    if d >= CORNER + 1:
        return 0
    return int(255 * (CORNER + 1 - d) / 2)


def load_image_1024(path, td):
    """Any image sips can read -> (S,S) RGB rows via BMP round-trip."""
    bmp = os.path.join(td, "icon.bmp")
    subprocess.run(["sips", "-s", "format", "bmp", "-z", str(S), str(S),
                    path, "--out", bmp], check=True, capture_output=True)
    d = open(bmp, "rb").read()
    off = struct.unpack("<I", d[10:14])[0]
    w, h = struct.unpack("<ii", d[18:26])
    bpp = struct.unpack("<H", d[28:30])[0]
    assert (w, abs(h), bpp) == (S, S, 24) or (w, abs(h), bpp) == (S, S, 32), "unexpected BMP"
    stride = ((w * (bpp // 8) + 3) // 4) * 4
    px = bytearray(S * S * 3)
    for y in range(S):
        src_y = (S - 1 - y) if h > 0 else y      # BMP bottom-up
        row = off + src_y * stride
        for x in range(S):
            b, g, r = d[row + x * (bpp // 8):row + x * (bpp // 8) + 3]
            i = (y * S + x) * 3
            px[i:i + 3] = bytes((r, g, b))
    return px


def emit_iconset(img, out_icns, td):
    iconset = os.path.join(td, "OpenTyrian.iconset")
    os.mkdir(iconset)
    write_png(os.path.join(iconset, "icon_512x512@2x.png"), S, S, img)
    for sz in (512, 256, 128, 64, 32, 16):
        subprocess.run(["sips", "-z", str(sz), str(sz),
                        os.path.join(iconset, "icon_512x512@2x.png"),
                        "--out", os.path.join(iconset, f"icon_{sz}x{sz}.png")],
                       check=True, capture_output=True)
    subprocess.run(["iconutil", "-c", "icns", iconset, "-o", out_icns],
                   check=True)


def main(path, out_icns):
    with tempfile.TemporaryDirectory() as td:
        px = load_image_1024(path, td)
        img = bytearray(S * S * 4)
        for y in range(S):
            for x in range(S):
                a = rounded_alpha(x, y)
                i3, i4 = (y * S + x) * 3, (y * S + x) * 4
                img[i4:i4 + 4] = bytes((px[i3], px[i3 + 1], px[i3 + 2], a))
        emit_iconset(img, out_icns, td)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(f"usage: {sys.argv[0]} <image> <out.icns>")
    main(sys.argv[1], sys.argv[2])
