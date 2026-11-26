#!/usr/bin/env python3
"""The SNES edition's hero (stage 12): 16x32, shaded with colour ramps in a later-SNES style, from the web game's
design (orange cap, brown hair, blue shirt with a white stripe). Frames: down, up and side, each standing and
two steps. Writes art/hero32.png (frames left to right) for tools/snesconv.py.
Usage: hero32.py [out.png]"""
import sys
from PIL import Image

PAL = {
    'k': (40, 24, 40),      # outline: deep plum, not black
    'O': (176, 80, 32), 'o': (232, 128, 56), 'y': (248, 176, 96), 'W': (255, 244, 220),   # cap; W also the stripe
    'H': (74, 42, 24), 'h': (118, 70, 36),                                                 # hair
    'S': (201, 143, 98), 's': (242, 198, 152),                                             # skin
    'B': (40, 52, 128), 'b': (58, 94, 200), 'l': (112, 148, 232),                         # shirt
    'P': (36, 36, 60), 'p': (64, 66, 96),                                                  # trousers
    'n': (86, 50, 30),                                                                     # shoes
}

HEAD_DOWN = [
    "................",
    ".....kkkkkk.....",
    "...kkoooooykk...",
    "..koooooooyWyk..",
    ".kooooooooooyyk.",
    ".kOoooooooooooк.",
    ".kOOOOOOOOOOOOk.",
    ".kkhkkkkkkkkhkk.",
    ".kHhhSSSSSSShHk.",
    ".kHhssssssssshk.",
    ".kHsskssssksShk.",
    ".kHsskssssksSHk.",
    "..kSsssssssSSk..",
    "...kSsssssSSk...",
    "....kkSSSSkk....",
]
HEAD_UP = [
    "................",
    ".....kkkkkk.....",
    "...kkoooooykk...",
    "..kooooooooyyk..",
    ".kooooooooooyyk.",
    ".kOoooooooooook.",
    ".kOOOOOOOOOOOOk.",
    ".kkhkkkkkkkkhkk.",
    ".kHhhhhhhhhhhHk.",
    ".kHhhhhhhhhhhHk.",
    ".kHHhhhhhhhhHHk.",
    ".kSHHhhhhhhHHSk.",
    "..kSHHHHHHHHSk..",
    "...kSHHHHHHSk...",
    "....kkSSSSkk....",
]
HEAD_SIDE = [
    "................",
    ".....kkkkkk.....",
    "...kkoooooykk...",
    "..koooooooyWyk..",
    ".kooooooooooyyk.",
    ".kOooooooooooook",
    ".kOOOOOOOOOOOOOk",
    ".kkhhkkkkkkkkkk.",
    ".kHhhhSSSSSSSk..",
    ".kHhhssssssssk..",
    ".kHhhssssskssk..",
    ".kHHhsssssksSk..",
    "..kHHssssssSSk..",
    "...kHHsssSSSk...",
    "....kkkSSSkk....",
]
BODY_DOWN = [
    "...kkbbWWbbkk...",
    "..kbbbbWWbbbBk..",
    ".klbbbbWWbbbbBk.",
    ".klbbbbWWbbbbBk.",
    ".kslbbbWWbbbBsk.",
    ".kSkbbbbbbbbkSk.",
    "..kkBbbbbbbBkk..",
    "...kBBBBBBBBk...",
    "...kpppppPPPk...",
]
BODY_UP = [
    "...kkbbbbbbkk...",
    "..kbbbbbbbbbBk..",
    ".klbbbbbbbbbbBk.",
    ".klbbbbbbbbbbBk.",
    ".kslbbbbbbbbBsk.",
    ".kSkbbbbbbbbkSk.",
    "..kkBbbbbbbBkk..",
    "...kBBBBBBBBk...",
    "...kpppppPPPk...",
]
BODY_SIDE = [
    "....kkbbbbkk....",
    "...kbbbbbbbBk...",
    "...klbbbbbbBk...",
    "...klbblbbbBk...",
    "...klbbslbbBk...",
    "...kbbbSkbbBk...",
    "....kBbbbbBk....",
    "....kBBBBBBk....",
    "....kppppPPk....",
]
LEGS_FRONT = [   # standing, left step, right step
    ["...kppPkkpPPk...", "...kppPkkpPPk...", "...kppPkkpPPk...", "...kpPPkkpPPk...", "...knnnkknnnk...", "...kkkkkkkkkk...", "................"],
    ["...kppPkkpPPk...", "...kppPkkpPPk...", "...kppPk.kpPk...", "...knnnk.kPPk...", "...kkkkk.knnk...", ".........kkkk...", "................"],
    ["...kppPkkpPPk...", "...kppPkkpPPk...", "...kpPk.kpPPk...", "...kpPk.knnnk...", "...knnk.kkkkk...", "...kkkk.........", "................"],
]
LEGS_SIDE = [
    ["....kppPPPk.....", "....kppPPk......", "....kppPPk......", "....kpPPPk......", "....knnnnnk.....", "....kkkkkkk.....", "................"],
    ["...kppPkpPPk....", "...kppkkkpPk....", "..kppk..kpPPk...", "..kpPk...kpPk...", ".knnnk...knnnk..", ".kkkkk...kkkkk..", "................"],
    ["....kppPPPk.....", "....kpPkPPk.....", "...kpPk.kPPk....", "...knnk.kpPk....", "...kkkk.knnnk...", "........kkkkk...", "................"],
]


def frame(head, body, legs):
    rows = head + body + legs
    rows = rows + ["................"] * (32 - len(rows))
    assert len(rows) == 32, len(rows)
    im = Image.new('RGBA', (16, 32), (0, 0, 0, 0))
    for y, r in enumerate(rows):
        r = r.replace('к', 'k')
        assert len(r) == 16, (y, r)
        for x, ch in enumerate(r):
            if ch != '.': im.putpixel((x, y), PAL[ch] + (255,))
    return im


def sheet():
    frames = []
    for legs in LEGS_FRONT: frames.append(frame(HEAD_DOWN, BODY_DOWN, legs))
    for legs in LEGS_FRONT: frames.append(frame(HEAD_UP, BODY_UP, legs))
    for legs in LEGS_SIDE: frames.append(frame(HEAD_SIDE, BODY_SIDE, legs))
    im = Image.new('RGBA', (16 * len(frames), 32), (0, 0, 0, 0))
    for i, f in enumerate(frames): im.paste(f, (i * 16, 0))
    return im


if __name__ == '__main__':
    sheet().save(sys.argv[1] if len(sys.argv) > 1 else 'art/hero32.png')
