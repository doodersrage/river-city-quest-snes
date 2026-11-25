#!/usr/bin/env python3
"""Decode converted SNES map data back into a PNG, the way the PPU would draw it.
Usage: preview.py <data-dir> <map-index> <out.png>"""
import sys
from PIL import Image


def decode_tile(b):
    idx = []
    for y in range(8):
        for x in range(8):
            bit = 7 - x
            v = 0
            for p in range(4):
                off = y * 2 + (p & 1) + (16 if p >= 2 else 0)
                v |= (b[off] >> bit & 1) << p
            idx.append(v)
    return idx


def main():
    d, i, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    chr_ = open(f'{d}/m{i}.chr', 'rb').read()
    tm = open(f'{d}/m{i}.map', 'rb').read()
    pal = open(f'{d}/m{i}.pal', 'rb').read()
    cols = []
    for k in range(0, len(pal), 2):
        v = pal[k] | pal[k + 1] << 8
        cols.append(((v & 31) << 3, (v >> 5 & 31) << 3, (v >> 10 & 31) << 3))
    tiles = [decode_tile(chr_[k:k + 32]) for k in range(0, len(chr_), 32)]
    im = Image.new('RGB', (512, 352))
    px = im.load()
    for cy in range(44):
        for cx in range(64):
            off = ((cy // 32) * 2 + (cx // 32)) * 1024 + (cy % 32) * 32 + (cx % 32)
            e = tm[off * 2] | tm[off * 2 + 1] << 8
            t, p, hf, vf = e & 1023, e >> 10 & 7, e >> 14 & 1, e >> 15 & 1
            for y in range(8):
                for x in range(8):
                    sx, sy = (7 - x if hf else x), (7 - y if vf else y)
                    px[cx * 8 + x, cy * 8 + y] = cols[p * 16 + tiles[t][sy * 8 + sx]]
    im.save(out)


if __name__ == '__main__':
    main()
