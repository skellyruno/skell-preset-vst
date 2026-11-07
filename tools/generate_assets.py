#!/usr/bin/env python3
"""Generate deterministic original SKELL placeholder art using only stdlib.
Use when the source release excludes the large binary PNGs. No external assets.
"""
from pathlib import Path
import math
import struct
import zlib
import random

ROOT = Path(__file__).resolve().parent.parent / 'assets'
ROOT.mkdir(parents=True, exist_ok=True)

def png(path, w, h, pixel):
    rows = bytearray()
    rng = random.Random(0x5CE11)
    for y in range(h):
        rows.append(0)
        for x in range(w):
            rows.extend(pixel(x, y, rng))
    def chunk(tag, data):
        return struct.pack('!I', len(data)) + tag + data + struct.pack('!I', zlib.crc32(tag + data) & 0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', w, h, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))

def tone(bg, accent, glow, noise):
    return tuple(min(255, max(0, int(bg[k] + accent[k] * glow + noise))) for k in range(3))

def background(x, y, rng):
    radial = max(0.0, 1.0 - math.hypot((x - 720) / 1100, (y - 100) / 650))
    grid = (1.8 if x % 80 == 0 or y % 80 == 0 else 0.0)
    return tone((7, 15, 28), (12, 27, 39), radial * .55, rng.random() * 1.4 + grid)

def skull_mask(x, y, w, h):
    # Winged skull silhouette, eyes, teeth and small temples. Procedural, original.
    xx = (x - w * .5) / (w * .5)
    yy = (y - h * .48) / (h * .48)
    head = (xx / .44) ** 2 + ((yy + .12) / .64) ** 2 < 1
    jaw = abs(xx) < .28 and .10 < yy < .65
    eye = ((xx - .16) / .12) ** 2 + ((yy + .07) / .15) ** 2 < 1 or ((xx + .16) / .12) ** 2 + ((yy + .07) / .15) ** 2 < 1
    nose = abs(xx) < .05 and .12 < yy < .28
    teeth = .43 < yy < .60 and (int((xx + 1) * 28) % 4 == 0)
    wing = abs(xx) > .42 and abs(xx) < .92 and (yy < -.14 + (abs(xx) - .42) * 1.0) and (yy > -.5 + (abs(xx) - .42) * .7)
    return (head or jaw or wing) and not (eye or nose or teeth)

def logo(x, y, rng):
    if skull_mask(x, y, 96, 96):
        return 57, 255, 80
    return 8, 18, 30

def cover(index):
    shades = ((22, 68, 49), (42, 45, 79), (46, 76, 94), (72, 40, 65), (42, 65, 59), (42, 64, 90))
    hue = shades[index]
    def pixel(x, y, rng):
        edge = max(0.0, 1 - math.hypot((x - 310) / 400, (y - 85) / 150))
        bands = max(0.0, math.sin(x * .026 + y * .008 + index * 2.2)) * .12
        base = tone((13, 24, 35), hue, edge * .75 + bands, rng.random() * 2.5)
        if 380 < x < 470 and 10 < y < 190 and skull_mask(x - 377, y - 10, 94, 180):
            return 50, 175 + index * 8, 75
        return base
    return pixel

if __name__ == '__main__':
    names = ['bg_panel.png', 'logo_skull.png'] + [f'cover_{i}.png' for i in range(6)]
    if any(not (ROOT / name).exists() for name in names):
        png(ROOT / 'bg_panel.png', 1168, 688, background)
        png(ROOT / 'logo_skull.png', 96, 96, logo)
        for i in range(6):
            png(ROOT / f'cover_{i}.png', 480, 200, cover(i))
    print('SKELL assets ready:', ', '.join(names))
