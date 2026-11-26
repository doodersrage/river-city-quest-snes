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


BARE_DOWN = ["................", "................", ".....kkkkkk.....", "...kkoooooykk...", "..koooooooyyyk..",
             ".kooooooooooyok.", ".kOoooooooooooOk", ".kOoOoooooOoOOk."]
BARE_UP = ["................", "................", ".....kkkkkk.....", "...kkoooooykk...", "..koooooooyyyk..",
           ".kooooooooooyok.", ".kOoooooooooooOk", ".kOooooooooooOk."]
BARE_SIDE = ["................", "................", ".....kkkkkk.....", "...kkoooooykk...", "..koooooooyyyk..",
             ".kooooooooooyok.", ".kOoooooooooook.", ".kOoOooooOoOOk.."]


def frame(head, body, legs, pal=PAL):
    rows = head + body + legs
    rows = rows + ["................"] * (32 - len(rows))
    assert len(rows) == 32, len(rows)
    im = Image.new('RGBA', (16, 32), (0, 0, 0, 0))
    for y, r in enumerate(rows):
        r = r.replace('к', 'k')
        assert len(r) == 16, (y, r)
        for x, ch in enumerate(r):
            if ch != '.': im.putpixel((x, y), pal[ch] + (255,))
    return im


def frames(pal=PAL, bare=False, stripe=True):
    """Nine 16x32 frames: down, up, side (facing right), each standing and two steps."""
    def head(h, b):
        return (b + h[8:]) if bare else h
    def body(rows):
        return rows if stripe else [r.replace('W', 'b') for r in rows]
    out = []
    for legs in LEGS_FRONT: out.append(frame(head(HEAD_DOWN, BARE_DOWN), body(BODY_DOWN), legs, pal))
    for legs in LEGS_FRONT: out.append(frame(head(HEAD_UP, BARE_UP), body(BODY_UP), legs, pal))
    for legs in LEGS_SIDE: out.append(frame(head(HEAD_SIDE, BARE_SIDE), body(BODY_SIDE), legs, pal))
    return out


def rgb(c):
    return tuple(int(c[i:i + 2], 16) for i in (1, 3, 5))


def darker(c, t=.35):
    r, g, b = rgb(c)
    return (round(r + (40 - r) * t), round(g + (24 - g) * t), round(b + (56 - b) * t))


def lighter(c, t=.35):
    r, g, b = rgb(c)
    return (round(r + (255 - r) * t), round(g + (244 - g) * t), round(b + (214 - b) * t))


def person(look):
    """A townsperson from the web game's look (hat and shirt ramps, skin, hair, trousers): about half go bareheaded."""
    bare = (look['q'] >> 9) % 2 == 1
    hat, shirt, skin = look['hat'], look['shirt'], look['skin']
    hair = look.get('hair') or '#6a4020'
    pants = look.get('pants') or '#3c3c54'
    pal = dict(PAL)
    if bare:
        pal.update(o=rgb(hair), O=darker(hair), y=lighter(hair), W=lighter(hair, .6))
    else:
        pal.update(o=rgb(hat[0]), O=rgb(hat[1]), y=rgb(hat[2]), W=lighter(hat[2], .5))
    pal.update(H=darker(hair), h=rgb(hair), S=rgb(skin[1]), s=rgb(skin[0]),
               B=rgb(shirt[1]), b=rgb(shirt[0]), l=rgb(shirt[2]), p=rgb(pants), P=darker(pants))
    return frames(pal, bare, stripe=False)


def strip(fs):
    im = Image.new('RGBA', (16 * len(fs), 32), (0, 0, 0, 0))
    for i, f in enumerate(fs): im.paste(f, (i * 16, 0))
    return im


if __name__ == '__main__':
    strip(frames()).save(sys.argv[1] if len(sys.argv) > 1 else 'art/hero32.png')
