#!/usr/bin/env python3
"""Generate the app icons in Resources/, in the style of icon_source.jpg (the
Gemini logo): a teal ring on black with a pulse line rising into a solid arrow.

icon_store.png  512x512, the full logo with "VO2" and "MAX", for the store.
icon_60x60.png  On-watch icon with "VO2".
icon_30x30.png  On-watch icon without text, which cannot be read at that size.

The on-watch icons are stored as ABGR2222 (4 levels per channel), so they are
drawn for the pixel grid: thick strokes and every pixel snapped to black, teal
or white, with partial alpha only on the outer edge. Run: mise run icons
"""

import os

from PIL import Image, ImageDraw, ImageFont

RES = os.path.join(os.path.dirname(__file__), "..", "Resources")
FONTS = os.path.join(os.environ["UNA_SDK"], "Examples", "Apps", "Running", "Software", "Apps",
                     "TouchGFX-GUI", "assets", "fonts")
FONT = os.path.join(FONTS, "Poppins-SemiBold.ttf")
STORE_FONT = os.path.join(FONTS, "Poppins-Medium.ttf")
SCALE = 8
BLACK = (0, 0, 0)
TEAL = (0, 170, 170)   # on the 4-level grid
WHITE = (255, 255, 255)
PALETTE = (BLACK, TEAL, WHITE)


def nearest(rgb):
    return min(PALETTE, key=lambda c: sum((a - b) ** 2 for a, b in zip(rgb, c)))


def arrow(draw, pts, width, head, big, fill=TEAL):
    """Polyline through unit points, ending in a solid arrowhead."""
    p = [(x * big, y * big) for x, y in pts]
    (x0, y0), (x1, y1) = p[-2], p[-1]
    n = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
    ux, uy = (x1 - x0) / n, (y1 - y0) / n
    s = head * big
    base = (x1 - ux * s, y1 - uy * s)
    draw.line(p[:-1] + [base], fill=fill, width=width, joint="curve")
    draw.polygon([(x1, y1),
                  (base[0] - uy * s * 0.6, base[1] + ux * s * 0.6),
                  (base[0] + uy * s * 0.6, base[1] - ux * s * 0.6)], fill=fill)


def icon(px, ring_px, line_px, pulse, head, text=None):
    big = px * SCALE
    img = Image.new("RGB", (big, big), BLACK)
    draw = ImageDraw.Draw(img)
    c = big / 2
    outer = big / 2 - SCALE * 0.5          # half a pixel in from the edge
    ring = ring_px * SCALE
    r = outer - ring / 2
    draw.ellipse((c - r, c - r, c + r, c + r), outline=TEAL, width=ring)
    arrow(draw, pulse, line_px * SCALE, head, big)
    if text:
        size, x, y = text
        font = ImageFont.truetype(FONT, round(size * big))
        draw.text((x * big, y * big), "VO2", font=font, fill=WHITE, anchor="mm")

    # Downscale, then snap each pixel to the palette: no in-between colours.
    rgb = img.resize((px, px), Image.BOX)
    for y in range(px):
        for x in range(px):
            rgb.putpixel((x, y), nearest(rgb.getpixel((x, y))))

    # Alpha: the disc, anti-aliased on its edge only, rounded to 4 levels.
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).ellipse((c - outer, c - outer, c + outer, c + outer), fill=255)
    alpha = mask.resize((px, px), Image.BOX).point(lambda v: round(v / 85) * 85)
    out = rgb.convert("RGBA")
    out.putalpha(alpha)
    return out


def store(px):
    """Full logo, drawn at 4x and downscaled; a normal PNG, not quantized."""
    big = px * 4
    teal = (32, 163, 150)  # the Gemini logo's teal; the store has full colour
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    c = big / 2
    outer = 0.459 * big  # flush with the ring's outer edge
    draw.ellipse((c - outer, c - outer, c + outer, c + outer), fill=BLACK)
    ring = round(0.028 * big)
    r = 0.445 * big
    draw.ellipse((c - r, c - r, c + r, c + r), outline=teal, width=ring)

    arrow(draw, [(0.07, 0.61), (0.23, 0.61), (0.265, 0.53), (0.31, 0.73), (0.37, 0.41),
                 (0.415, 0.79), (0.51, 0.45), (0.56, 0.53), (0.78, 0.23)],
          round(0.026 * big), 0.12, big, fill=teal)

    font = ImageFont.truetype(STORE_FONT, round(0.17 * big))
    draw.text((0.20 * big, 0.36 * big), "VO", font=font, fill=WHITE, anchor="ls")
    sub = ImageFont.truetype(STORE_FONT, round(0.10 * big))
    vo_right = draw.textbbox((0.20 * big, 0.36 * big), "VO", font=font, anchor="ls")[2]
    draw.text((vo_right + 0.004 * big, 0.38 * big), "2", font=sub, fill=WHITE, anchor="ls")
    draw.text((0.665 * big, 0.765 * big), "MAX", font=ImageFont.truetype(STORE_FONT, round(0.15 * big)),
              fill=WHITE, anchor="ms")
    return img.resize((px, px), Image.LANCZOS)


store(512).save(os.path.join(RES, "icon_store.png"))

# Pulse in unit coordinates: flat start, heartbeat spike, rising zigzag, arrow tip.
icon(60, ring_px=3, line_px=4,
     pulse=[(0.10, 0.70), (0.30, 0.70), (0.38, 0.52), (0.47, 0.86),
            (0.56, 0.62), (0.63, 0.72), (0.84, 0.42)],
     head=0.16, text=(0.30, 0.47, 0.30)).save(os.path.join(RES, "icon_60x60.png"))

icon(30, ring_px=2, line_px=2,
     pulse=[(0.12, 0.60), (0.28, 0.60), (0.36, 0.38), (0.45, 0.76),
            (0.54, 0.50), (0.61, 0.60), (0.80, 0.30)],
     head=0.20).save(os.path.join(RES, "icon_30x30.png"))
