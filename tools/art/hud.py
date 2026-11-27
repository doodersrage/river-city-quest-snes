#!/usr/bin/env python3
"""The SNES edition's HUD icons (stage 12): 8x8, shaded like the rest: a full heart, an empty heart, and the coin
face-on and edge-on (it spins). Writes art/hud8.png (four tiles left to right). Usage: hud.py [out.png]"""
import sys
from PIL import Image

P = {'k': (40, 24, 40), 'R': (160, 24, 48), 'r': (232, 48, 72), 'p': (255, 128, 152), 'w': (255, 255, 255),
     'e': (58, 40, 64), 'E': (90, 72, 104), 'D': (168, 104, 24), 'o': (242, 193, 78), 'y': (255, 242, 168)}
TILES = [
    [".kk.kk..", "kpwkrrk.", "kprrrRk.", "krrrrRk.", ".krrRk..", "..kRk...", "...k....", "........"],
    [".kk.kk..", "kEekeek.", "keeeeek.", "keeeeek.", ".keeek..", "..kek...", "...k....", "........"],
    ["..kkkk..", ".kyyook.", "kyoyooDk", "kyoyooDk", "kyoyooDk", "koooooDk", ".kDDDDk.", "..kkkk.."],
    ["...kk...", "..kyok..", "..kyok..", "..kyok..", "..kyok..", "..kyDk..", "..kDDk..", "...kk..."],
]


def sheet():
    im = Image.new('RGBA', (32, 8), (0, 0, 0, 0))
    for i, t in enumerate(TILES):
        for y, row in enumerate(t):
            assert len(row) == 8
            for x, ch in enumerate(row):
                if ch != '.': im.putpixel((i * 8 + x, y), P[ch] + (255,))
    return im


if __name__ == '__main__':
    sheet().save(sys.argv[1] if len(sys.argv) > 1 else 'art/hud8.png')
