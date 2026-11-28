#!/usr/bin/env python3
"""The SNES edition's cars and Pulse bus (stage 14), drawn at 32 px tall in place of the web game's 16 px side views
through Scale2x. Side view facing right (the ROM mirrors them going left), lit from above: a highlight along the
shoulder of the body, the panels shading down to a dark sill, glass that darkens to the top with a glint, chrome
bumpers, headlight and taillight, and tyres with lit hubs. Only sprite palette 7's colours are used (the cars share it
with the HUD and the items, and it has reds, yellows, greys, black and white but no blue).
Writes art/vehicles.png: red car (64x32), yellow cab (64x32), bus (96x32), left to right.
Usage: vehicles.py [out.png]"""
import sys
from PIL import Image, ImageDraw

INK = (24, 16, 32)       # #181020
DEEP = (40, 24, 40)      # #281828
GL0 = (56, 40, 64)       # #382840 glass, dark
GL1 = (88, 72, 104)      # #584868
GREY = (104, 104, 120)   # #686878
WHITE = (248, 248, 248)
CREAM = (248, 240, 168)  # #f8f0a8
TAN = (200, 160, 104)    # #c8a068
RED = [(160, 24, 48), (232, 48, 72), (248, 128, 152)]     # dark, body, light
YEL = [(168, 104, 24), (240, 192, 72), (248, 240, 168)]


def car(body, cab=False):
    im = Image.new('RGBA', (64, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    P = lambda x, y, c: d.point((x, y), fill=c + (255,))
    Rr = lambda x, y, w, h, c: d.rectangle((x, y, x + w - 1, y + h - 1), fill=c + (255,))
    D, M, L = body
    # the body: a long box with rounded ends, the cabin on top, the windscreen raked toward the front (right)
    for y in range(14, 28):
        inset = {14: 4, 15: 2, 16: 1, 26: 1, 27: 2}.get(y, 0)
        Rr(1 + inset, y, 62 - 2 * inset, 1, INK)
        if 15 <= y <= 26: Rr(2 + inset, y, 60 - 2 * inset, 1, M if y < 22 else D)
    Rr(3, 15, 58, 1, L); Rr(2, 16, 60, 1, L)          # the shoulder catches the light
    for x in range(2, 62, 2): P(x, 22, M)              # where the panel turns under, dithered
    Rr(2, 25, 60, 2, DEEP)                             # the sill
    # the cabin
    for y in range(3, 15):
        x0 = 16 - min(4, (y - 3) // 2) if y > 4 else 17
        x1 = 41 + (y - 3) if y > 4 else 41
        Rr(x0 - 1, y, x1 - x0 + 3, 1, INK)
        Rr(x0, y, x1 - x0 + 1, 1, M)
    Rr(17, 4, 25, 1, L)
    # glass: the back window and the side windows, darker toward the roof, a glint across
    for y in range(6, 14):
        a = 17 - min(3, (y - 5) // 2); b = 47 + (y - 6) - 7
        Rr(a, y, b - a, 1, GL0 if y < 8 else GL1 if y < 11 else GREY)
    Rr(30, 6, 2, 8, M)                                 # the pillar between the doors
    Rr(41, 6, 1, 8, M)
    for i in range(5): P(21 + i, 12 - i, WHITE); P(34 + i, 12 - i, WHITE)
    P(22, 12, CREAM); P(35, 12, CREAM)
    # the windscreen's rake, front right
    for y in range(6, 14):
        x = 42 + (y - 6)
        Rr(x, y, 2, 1, GL1 if y < 10 else GREY)
    # doors and handles
    Rr(30, 16, 1, 9, D); Rr(45, 16, 1, 9, D); Rr(17, 16, 1, 9, D)
    Rr(25, 18, 3, 1, WHITE); Rr(40, 18, 3, 1, WHITE)
    # wheel arches and tyres
    for cx in (14, 50):
        for y in range(20, 32):
            for x in range(cx - 8, cx + 9):
                dx, dy = x - cx, y - 26
                r2 = dx * dx + dy * dy
                if r2 <= 64 and y >= 20 and r2 > 49 and y < 26: P(x, y, INK)        # the arch
                if r2 <= 36: P(x, y, INK)                                          # the tyre
                if r2 <= 36 and r2 >= 25 and dy < 0: P(x, y, GL0)                   # its tread catching light
                if r2 <= 9: P(x, y, GREY)                                           # the hub
                if r2 <= 2: P(x, y, DEEP)
        P(cx - 1, 25, WHITE); P(cx - 2, 26, WHITE)
    # lights and chrome
    Rr(60, 17, 3, 3, CREAM); P(61, 17, WHITE); Rr(60, 20, 2, 1, TAN)                 # headlight
    Rr(1, 17, 2, 4, RED[0]); P(1, 17, RED[2]); P(2, 18, RED[1])                       # taillight
    Rr(57, 23, 6, 2, GREY); Rr(57, 23, 6, 1, WHITE)                                   # bumpers
    Rr(1, 23, 6, 2, GREY); Rr(1, 23, 6, 1, WHITE)
    Rr(45, 15, 3, 1, INK); P(48, 15, GREY)                                             # the wing mirror
    if cab:   # a cab: the roof light and a checker band
        Rr(26, 0, 12, 3, INK); Rr(27, 1, 10, 2, CREAM); Rr(27, 1, 10, 1, WHITE)
        for x in range(4, 58, 4): Rr(x, 20, 2, 1, INK); Rr(x + 2, 21, 2, 1, INK)
    return im


def bus():
    im = Image.new('RGBA', (96, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    P = lambda x, y, c: d.point((x, y), fill=c + (255,))
    Rr = lambda x, y, w, h, c: d.rectangle((x, y, x + w - 1, y + h - 1), fill=c + (255,))
    # the box, a rounded roof, the front (right) curving in
    Rr(1, 2, 94, 27, INK)
    Rr(2, 1, 90, 1, INK); Rr(3, 0, 86, 1, INK)
    Rr(2, 3, 92, 25, WHITE)
    Rr(3, 1, 87, 2, WHITE); Rr(3, 2, 88, 1, CREAM)          # the roof's lit edge
    for x in range(2, 94, 2): P(x, 3, CREAM)
    Rr(2, 22, 92, 6, GREY)                                  # the skirt in shade
    for x in range(2, 94, 2): P(x, 21, GREY)
    Rr(2, 27, 92, 1, DEEP)
    # the red band under the windows
    Rr(2, 16, 92, 3, RED[1]); Rr(2, 16, 92, 1, RED[2]); Rr(2, 18, 92, 1, RED[0])
    # windows: a long row, glass darker at the top, frames white, a glint in each
    for x in range(4, 70, 11):
        Rr(x, 5, 9, 9, INK)
        Rr(x + 1, 6, 7, 3, GL0); Rr(x + 1, 9, 7, 3, GL1); Rr(x + 1, 12, 7, 1, GREY)
        P(x + 2, 10, WHITE); P(x + 3, 9, WHITE); P(x + 4, 8, WHITE)
    # the doors: glass panels, front of the middle
    for x0 in (72,):
        Rr(x0, 4, 11, 22, INK)
        Rr(x0 + 1, 5, 4, 20, GL1); Rr(x0 + 6, 5, 4, 20, GL1)
        Rr(x0 + 1, 5, 4, 6, GL0); Rr(x0 + 6, 5, 4, 6, GL0)
        Rr(x0 + 1, 14, 4, 1, GREY); Rr(x0 + 6, 14, 4, 1, GREY)
        P(x0 + 2, 9, WHITE); P(x0 + 7, 9, WHITE)
    # the windscreen wrapping round the front, the destination sign above it
    Rr(85, 4, 9, 13, INK); Rr(86, 7, 8, 9, GL1); Rr(86, 7, 8, 3, GL0); P(88, 13, WHITE); P(89, 12, WHITE); P(90, 11, WHITE)
    Rr(84, 3, 10, 3, INK)
    for x in range(85, 93, 2): P(x, 4, (240, 192, 72))
    Rr(92, 19, 2, 3, CREAM); P(93, 19, WHITE)               # headlight
    Rr(1, 18, 2, 4, RED[0]); P(1, 18, RED[2])               # taillight
    Rr(88, 24, 7, 2, GREY); Rr(88, 24, 7, 1, WHITE); Rr(1, 24, 5, 2, GREY); Rr(1, 24, 5, 1, WHITE)
    # a bike rack on the nose
    Rr(94, 14, 1, 8, GREY)
    # wheels
    for cx in (18, 74):
        for y in range(19, 32):
            for x in range(cx - 8, cx + 9):
                dx, dy = x - cx, y - 26
                r2 = dx * dx + dy * dy
                if 49 < r2 <= 64 and y < 26: P(x, y, INK)
                if r2 <= 36: P(x, y, INK)
                if 25 <= r2 <= 36 and dy < 0: P(x, y, GL0)
                if r2 <= 9: P(x, y, GREY)
                if r2 <= 2: P(x, y, DEEP)
        P(cx - 1, 25, WHITE); P(cx - 2, 26, WHITE)
    return im


def sheet():
    out = Image.new('RGBA', (64 + 64 + 96, 32), (0, 0, 0, 0))
    out.paste(car(RED), (0, 0)); out.paste(car(YEL, cab=True), (64, 0)); out.paste(bus(), (128, 0))
    return out


if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else 'art/vehicles.png'
    sheet().save(path)
