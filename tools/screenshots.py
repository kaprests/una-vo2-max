#!/usr/bin/env python3
"""Convert ScreenshotTour's raw RGB565 dumps to PNGs as the watch shows them.

Each channel is cut to its top two bits, as the LVGL port does for the
ABGR2222 display, and pixels outside the round display are made transparent.
Writes <name>.png (240 x 240) and <name>@2x.png.

Usage: screenshots.py <raw dir> <out dir>
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 240


def to_image(raw):
    rgb = bytearray()
    for i in range(0, len(raw), 2):
        px = raw[i] | raw[i + 1] << 8
        r, g, b = px >> 11, (px >> 5) & 0x3F, px & 0x1F
        rgb += bytes(((r >> 3) * 85, (g >> 4) * 85, (b >> 3) * 85))
    img = Image.frombytes("RGB", (SIZE, SIZE), bytes(rgb)).convert("RGBA")
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, SIZE - 1, SIZE - 1), fill=255)
    img.putalpha(mask)
    return img


raw_dir, out_dir = map(Path, sys.argv[1:3])
out_dir.mkdir(parents=True, exist_ok=True)
for raw in sorted(raw_dir.glob("*.rgb565")):
    img = to_image(raw.read_bytes())
    img.save(out_dir / f"{raw.stem}.png")
    img.resize((SIZE * 2, SIZE * 2), Image.NEAREST).save(out_dir / f"{raw.stem}@2x.png")
    print(out_dir / f"{raw.stem}.png")
