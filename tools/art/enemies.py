#!/usr/bin/env python3
"""The SNES edition's enemies (stage 12): 16x16, two frames each, shaded with colour ramps and plum outlines like the
16x32 people. Order: rat, squirrel, goose, scooter, crab, bat, ghost (ENEMY_TYPES in snesconv.py), frames a and b.
All face right; the ROM flips them. Writes art/enemies.png (14 frames left to right).
Usage: enemies.py [out.png]"""
import sys
from PIL import Image

K = (40, 24, 40)
WARM = {   # rat, squirrel, scooter, crab
    'k': K, 'e': (24, 16, 24), 'w': (255, 244, 224), 'p': (240, 154, 154),
    'd': (78, 68, 88), 'm': (128, 116, 132), 'l': (178, 168, 178),
    'B': (106, 52, 24), 'b': (168, 90, 40), 'o': (216, 138, 64), 'y': (240, 184, 112),
    'R': (122, 28, 32), 'r': (192, 58, 42), 'c': (238, 106, 72),
    't': (58, 138, 160), 'T': (112, 200, 216),
}
COOL = {   # goose, bat, ghost
    'k': K, 'w': (244, 240, 232), 'g': (200, 184, 168), 'G': (138, 122, 112), 'K': (58, 54, 66),
    'O': (240, 144, 48), 'V': (58, 32, 80), 'v': (106, 58, 138), 'm': (154, 104, 192), 'r': (232, 48, 72),
    'b': (168, 192, 232), 'B': (106, 128, 192),
}

E = "................"
RAT = [
    [E, E, E, E, E,
     "...........kk.k.",
     "..........kpkkpk",
     "......kkkkkmmmmk",
     "....kkllllmmmwek",
     "...klllllmmmmmmk",
     "pp.kmllmmmmmmmkp",
     ".pkmmmmmmmmmdkk.",
     "..kdmmmmmmddk...",
     "...kkdkkkkdk....",
     "....kk...kk.....", E],
    [E, E, E, E, E,
     "...........kk.k.",
     "..........kpkkpk",
     "......kkkkkmmmmk",
     "....kkllllmmmwek",
     "...klllllmmmmmmk",
     ".p.kmllmmmmmmmkp",
     "ppkmmmmmmmmmdkk.",
     "..kdmmmmmmddk...",
     "..kdkkkkkkdkk...",
     "..kk.....kk.....", E],
]
SQUIRREL = [
    [E,
     ".kkkk...........",
     "kbbobk.....k.k..",
     "kbooobk...kbkbk.",
     "kbooobk..kbbbbbk",
     "kBbooBk..kbwebbk",
     ".kBbobk.kbbbbbyk",
     ".kBbobkkbbbbyyk.",
     "..kBbokbbbbyyk..",
     "..kBbbkbbbbyk...",
     "...kBbkbbybbk...",
     "...kkbkbbyyBk...",
     "....kkBbbbbBk...",
     "......kBkkBBk...",
     "......kk..kkk...", E],
    [".kkkk...........",
     "kbbobk..........",
     "kbooobk....k.k..",
     "kbooobk...kbkbk.",
     "kBbooBk..kbbbbbk",
     ".kBbobk..kbwebbk",
     ".kBbobk.kbbbbbyk",
     "..kBbokkbbbbyyk.",
     "..kBbbkbbbbyyk..",
     "...kBbkbbbbyk...",
     "...kkbkbbybbk...",
     "....kkkbbyyBk...",
     "......kBbbbBk...",
     ".....kBk.kBk....",
     ".....kk..kkk....", E],
]
GOOSE = [
    ["..........kkk...",
     ".........kKKKk..",
     ".........kKwKkk.",
     ".........kKKKOOk",
     "..........kwkkk.",
     "..........kwk...",
     "..........kKk...",
     "..kkkkkkkkKKk...",
     ".kGggggggkKk....",
     "kGgwwwwwwgkk....",
     "kGgwwwwwwwwgk...",
     ".kGgwwwwwwggk...",
     "..kkGGggggGk....",
     "....kkkkkkk.....",
     ".....kO..kO.....",
     ".....kOk.kOk...."],
    ["..........kkk...",
     ".........kKKKk..",
     ".........kKwKkk.",
     ".........kKKKOOk",
     "..........kwkkk.",
     "..........kwk...",
     ".kk.......kKk...",
     "kGgkkkkkkkKKk...",
     "kGgggggggkKk....",
     ".kGwwwwwwgkk....",
     "kGgwwwwwwwwgk...",
     ".kGgwwwwwwggk...",
     "..kkGGggggGk....",
     "....kkkkkkk.....",
     "....kO....kO....",
     "....kOk...kOk..."],
]
SCOOTER = [
    [E,
     "..........kkkk..",
     ".........klllmk.",
     "..........kktk..",
     "...........kTk..",
     "..........krtk..",
     "..........krtk..",
     "...........kTk..",
     "...........kTk..",
     "...........kTk..",
     ".kkkkkkkkkkkTk..",
     "kTTTTTTTTTTTtk..",
     "kkkttttttttkkkk.",
     "kmlkk....kkmlk..",
     "klmk......klmk..",
     ".kk........kk..."],
    [E,
     "..........kkkk..",
     ".........klllmk.",
     "..........kktk..",
     "...........kTk..",
     "..........kptk..",
     "..........kptk..",
     "...........kTk..",
     "...........kTk..",
     "...........kTk..",
     ".kkkkkkkkkkkTk..",
     "kTTTTTTTTTTTtk..",
     "kkkttttttttkkkk.",
     "klmkk....kklmk..",
     "kmlk......kmlk..",
     ".kk........kk..."],
]
CRAB = [
    [E, E, E,
     ".kk..........kk.",
     "kcck........kcck",
     "kRck.kk..kk.kcRk",
     ".kRk.kwkkwk.kRk.",
     "..kkkkekkekkkk..",
     "..kkcccccccckk..",
     ".kRcrrrrrrrrcRk.",
     ".kRrrrrrrrrrrRk.",
     "..kRRrrrrrrRRk..",
     ".kkkkRRRRRRkkkk.",
     "kRk.kRkkkkRk.kRk",
     "kk..kk....kk..kk", E],
    [E, E,
     ".kk..........kk.",
     "kcck........kcck",
     "kRck........kcRk",
     ".kRk.kk..kk.kRk.",
     "..kk.kwkkwk.kk..",
     "..kkkkekkekkkk..",
     "..kkcccccccckk..",
     ".kRcrrrrrrrrcRk.",
     ".kRrrrrrrrrrrRk.",
     "..kRRrrrrrrRRk..",
     "kkkkkRRRRRRkkkkk",
     ".kRkkRkkkkRkkRk.",
     "..kk.kk..kk.kk..", E],
]
BAT = [
    [E, E,
     ".kk..........kk.",
     "kmvk........kvmk",
     "kvmvk.k..k.kvmvk",
     "kvvmvkvkkvkvmvvk",
     "kVvvmvvvvvvmvvVk",
     ".kVvvvrvvrvvvVk.",
     "..kVVvvvvvvVVk..",
     "...kkkvvvvkkk...",
     "......kVVk......",
     ".......kk.......", E, E, E, E],
    [E, E, E, E,
     "......k..k......",
     ".....kvkkvk.....",
     "..kkkvvvvvvkkk..",
     ".kvvvvrvvrvvvvk.",
     "kvmmvvvvvvvvmmvk",
     "kvmvVkvvvvkVvmvk",
     "kvvVk.kVVk.kVvvk",
     "kVVk...kk...kVVk",
     ".kk..........kk.", E, E, E],
]
GHOST = [
    [E,
     ".....kkkkkk.....",
     "...kkwwwwwwkk...",
     "..kwwwwwwwwwbk..",
     ".kwwwwwwwwwwbbk.",
     ".kwwkkwwwkkwbbk.",
     ".kwwkkwwwkkwbbk.",
     ".kwwwwwwwwwwbbk.",
     ".kwwwwkkkwwwbbk.",
     ".kwwwwwwwwwbbbk.",
     ".kwwwwwwwwwbbBk.",
     ".kwwwwwwwwbbbBk.",
     ".kwwbkwwwbkbbBk.",
     ".kwbk.kwbk.kbBk.",
     "..kk...kk...kk..", E],
    [E, E,
     ".....kkkkkk.....",
     "...kkwwwwwwkk...",
     "..kwwwwwwwwwbk..",
     ".kwwwwwwwwwwbbk.",
     ".kwwkkwwwkkwbbk.",
     ".kwwkkwwwkkwbbk.",
     ".kwwwwwwwwwwbbk.",
     ".kwwwwkkkwwwbbk.",
     ".kwwwwwwwwwbbbk.",
     ".kwwwwwwwwwbbBk.",
     ".kwbkwwwwbkbbBk.",
     ".kbk.kwwbk.kbBk.",
     "..k...kkk....kk.", E],
]
TYPES = [(RAT, WARM), (SQUIRREL, WARM), (GOOSE, COOL), (SCOOTER, WARM), (CRAB, WARM), (BAT, COOL), (GHOST, COOL)]


def frame(rows, pal):
    assert len(rows) == 16, len(rows)
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0))
    for y, r in enumerate(rows):
        assert len(r) == 16, (y, r)
        for x, ch in enumerate(r):
            if ch != '.': im.putpixel((x, y), pal[ch] + (255,))
    return im


def sheet():
    fs = [frame(f, pal) for art, pal in TYPES for f in art]
    im = Image.new('RGBA', (16 * len(fs), 16), (0, 0, 0, 0))
    for i, f in enumerate(fs): im.paste(f, (i * 16, 0))
    return im


if __name__ == '__main__':
    sheet().save(sys.argv[1] if len(sys.argv) > 1 else 'art/enemies.png')
