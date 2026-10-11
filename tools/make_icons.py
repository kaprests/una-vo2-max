#!/usr/bin/env python3
"""Generate Resources/icon_60x60.png and icon_30x30.png: a teal heart with "VO2".

The watch stores icons as ABGR2222 (4 levels per channel), so each image is
drawn at 4x, downscaled, and quantized the way the packer does, so the PNG
shows what the watch will show. Run: mise run icons
"""

import math
import os

from PIL import Image, ImageDraw, ImageFont

SCALE = 4
TEAL = (0, 160, 160, 255)  # SDK Color::TEAL, mid-level so it survives quantizing
WHITE = (255, 255, 255, 255)
FONT = os.path.join(os.environ["UNA_SDK"], "Examples", "Apps", "Running", "Software", "Apps",
                    "TouchGFX-GUI", "assets", "fonts", "Poppins-SemiBold.ttf")
OUT = os.path.join(os.path.dirname(__file__), "..", "Resources")


def heart(size):
    """Parametric heart polygon filling a size x size box."""
    pts = [(16 * math.sin(t) ** 3,
            -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)))
           for t in (2 * math.pi * i / 360 for i in range(360))]
    xs, ys = zip(*pts)
    span = max(max(xs) - min(xs), max(ys) - min(ys))
    return [((x - min(xs)) / span * size, (y - min(ys)) / span * size
             + (size - (max(ys) - min(ys)) / span * size) / 2) for x, y in pts]


def quantize(img):
    """Truncate each channel to 2 bits, as app_merging.py does, then expand."""
    return img.point(lambda v: (v >> 6) * 85)


def icon(px):
    big = px * SCALE
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.polygon(heart(big), fill=TEAL)
    font = ImageFont.truetype(FONT, int(big * 0.30))
    draw.text((big / 2, big * 0.44), "VO2", font=font, fill=WHITE, anchor="mm")
    return quantize(img.resize((px, px), Image.BOX))


for px in (60, 30):
    icon(px).save(os.path.join(OUT, f"icon_{px}x{px}.png"))
