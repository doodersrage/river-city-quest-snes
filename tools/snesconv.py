#!/usr/bin/env python3
"""Convert the exported River City Quest maps and sprites into SNES data.

Backgrounds: 4bpp tiles (deduplicated, with H/V flips), up to 8 palettes of 15 colours,
and a 64x64 tilemap in the SNES four-screen layout. Collision: one byte per 16px cell.
Sprites: 4bpp 16x16 frames laid out 8 per row as the PPU expects, plus one palette.

Usage: snesconv.py <export-heroDir> <repo-root>
Writes <repo-root>/data/*, maps.asm, maps.h and src/maploader.c for the ROM build.
"""
import json, os, sys
from PIL import Image

MAX_PALS, PAL_COLORS, MAX_TILES = 8, 15, 1024

# BG3 (the 2bpp text layer) shares CGRAM 1-3 with BG palette 0, so those three
# colours are fixed in every map: box fill, gold (titles and frame), white (text).
def hexc(h):
    return (int(h[1:3], 16) >> 3, int(h[3:5], 16) >> 3, int(h[5:7], 16) >> 3)
TEXT_COLORS = [hexc('#1e2230'), hexc('#f2c14e'), hexc('#f4f1e8')]


def c15(rgb):
    r, g, b = rgb[:3]
    return (r >> 3, g >> 3, b >> 3)


def bgr555(c):
    r, g, b = c
    v = r | (g << 5) | (b << 10)
    return bytes((v & 0xFF, v >> 8))


def dist(a, b):
    return (a[0] - b[0]) ** 2 * 3 + (a[1] - b[1]) ** 2 * 4 + (a[2] - b[2]) ** 2 * 2


def nearest(c, pal):
    return min(range(len(pal)), key=lambda i: dist(c, pal[i]))


def tile4bpp(idx):
    """idx: 64 palette indices (0-15), row-major -> 32 bytes SNES 4bpp planar."""
    out = bytearray(32)
    for y in range(8):
        for x in range(8):
            v = idx[y * 8 + x]
            bit = 7 - x
            for p in range(4):
                if v >> p & 1:
                    off = y * 2 + (p & 1) + (16 if p >= 2 else 0)
                    out[off] |= 1 << bit
    return bytes(out)


def reduce_colors(im, n):
    """Median-cut the image to n colours (in 15-bit space)."""
    q = im.quantize(colors=n, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    return q.convert('RGB')


def tiles_of(im):
    W, H = im.size
    px = im.load()
    out = []
    for ty in range(0, H, 8):
        for tx in range(0, W, 8):
            out.append([c15(px[tx + x, ty + y]) for y in range(8) for x in range(8)])
    return out, W // 8, H // 8


def pack_palettes(tiles, allow_remap):
    """Greedy-pack each tile's colour set into <=8 palettes of <=15 colours."""
    sets = [frozenset(t) for t in tiles]
    order = sorted(set(sets), key=len, reverse=True)
    pals = [set(TEXT_COLORS)]
    assign = {}
    for s in order:
        if len(s) > PAL_COLORS and not allow_remap:
            return None
        best, cost = None, None
        for i, p in enumerate(pals):
            u = len(p | s)
            if u <= PAL_COLORS and (cost is None or u - len(p) < cost):
                best, cost = i, u - len(p)
        if best is None and len(pals) < MAX_PALS and len(s) <= PAL_COLORS:
            pals.append(set(s)); best = len(pals) - 1
        elif best is None:
            if not allow_remap:
                return None
            # Fit by remapping: pick the palette that represents this tile best.
            def err(p):
                pl = list(p) or [(0, 0, 0)]
                return sum(min(dist(c, q) for q in pl) for c in s)
            if len(pals) < MAX_PALS:
                pals.append(set(sorted(s, key=lambda c: -sum(1 for t in tiles for x in t if x == c))[:PAL_COLORS]))
                best = len(pals) - 1
            else:
                best = min(range(len(pals)), key=lambda i: err(pals[i]))
        else:
            pals[best] |= s
        assign[s] = best
    out = [TEXT_COLORS + sorted(pals[0] - set(TEXT_COLORS))] + [sorted(p) for p in pals[1:]]
    return out, [assign[s] for s in sets]


def convert_bg(path, flip_dedup=True):
    src = Image.open(path).convert('RGB')
    result = None
    for n in (128, 112, 96, 84, 72, 64, 56, 48):
        im = src if n == 128 and len({c15(c) for _, c in src.getcolors(1 << 24)}) <= 120 else reduce_colors(src, n)
        tiles, tw, th = tiles_of(im)
        r = pack_palettes(tiles, allow_remap=False)
        if r:
            result = (tiles, tw, th, r); break
    if not result:
        im = reduce_colors(src, 48)
        tiles, tw, th = tiles_of(im)
        result = (tiles, tw, th, pack_palettes(tiles, allow_remap=True))
    tiles, tw, th, (pals, assign) = result

    chr_data, lookup, entries = [], {}, []
    for t, pi in zip(tiles, assign):
        pal = pals[pi]
        idx = [pal.index(c) + 1 if c in pal else nearest(c, pal) + 1 for c in t]
        key = tuple(idx)
        hit = lookup.get(key)
        if hit is None:
            n = len(chr_data)
            chr_data.append(tile4bpp(idx))
            lookup[key] = (n, 0, 0)
            if flip_dedup:
                rows = [idx[y * 8:(y + 1) * 8] for y in range(8)]
                h = tuple(v for r in rows for v in r[::-1])
                v = tuple(v for r in rows[::-1] for v in r)
                hv = tuple(v for r in rows[::-1] for v in r[::-1])
                lookup.setdefault(h, (n, 1, 0)); lookup.setdefault(v, (n, 0, 1)); lookup.setdefault(hv, (n, 1, 1))
            hit = lookup[key]
        n, hf, vf = hit
        entries.append(n | (pi << 10) | (hf << 14) | (vf << 15))
    if len(chr_data) > MAX_TILES:
        raise SystemExit(f'{path}: {len(chr_data)} tiles, over the {MAX_TILES} limit')

    # 64x64 tilemap in four 32x32 screens: TL, TR, BL, BR.
    tm = bytearray(64 * 64 * 2)
    for cy in range(th):
        for cx in range(tw):
            e = entries[cy * tw + cx]
            off = ((cy // 32) * 2 + (cx // 32)) * 1024 + (cy % 32) * 32 + (cx % 32)
            tm[off * 2] = e & 0xFF; tm[off * 2 + 1] = e >> 8
    palbin = bytearray()
    for i in range(MAX_PALS):
        p = pals[i] if i < len(pals) else []
        palbin += bgr555((0, 0, 0))
        for j in range(PAL_COLORS):
            palbin += bgr555(p[j] if j < len(p) else (0, 0, 0))
    return b''.join(chr_data), bytes(tm), bytes(palbin), len(chr_data), len(pals)


def convert_sprite_strip(path):
    """16px-tall strip of 16x16 frames -> OBJ tiles (frame f at tile 2f, row width 16 tiles)."""
    im = Image.open(path).convert('RGBA')
    px = im.load()
    colors = sorted({c15(px[x, y]) for x in range(im.width) for y in range(im.height) if px[x, y][3] > 0})
    if len(colors) > 15:
        raise SystemExit(f'{path}: {len(colors)} colours, over 15')
    frames = im.width // 16
    if frames > 8:
        raise SystemExit('more than 8 frames per strip not supported yet')
    sheet = [bytes(32)] * 32   # two rows of 16 tiles
    for f in range(frames):
        for (qx, qy) in ((0, 0), (1, 0), (0, 1), (1, 1)):
            idx = []
            for y in range(8):
                for x in range(8):
                    p = px[f * 16 + qx * 8 + x, qy * 8 + y]
                    idx.append(colors.index(c15(p)) + 1 if p[3] > 0 else 0)
            sheet[qy * 16 + f * 2 + qx] = tile4bpp(idx)
    pal = bgr555((0, 0, 0)) + b''.join(bgr555(c) for c in colors) + bgr555((0, 0, 0)) * (15 - len(colors))
    return b''.join(sheet), pal


# ---------- Stage 2: text, signs, NPCs, font ----------
BOX_COLS, BOX_LINES = 28, 4
WLD_GRID, WLD_NNPC, WLD_NTXT, WLD_NPCS, WLD_TOFS, MAX_NPCS = 0, 704, 705, 706, 754, 8
ASCII_FIX = {'‘': "'", '’': "'", '“': '"', '”': '"', '—': '-', '–': '-', '…': '...', 'é': 'e'}


def ascii_text(t):
    t = ''.join(ASCII_FIX.get(c, c) for c in t)
    return ''.join(c if 32 <= ord(c) < 127 else '?' for c in t)


def wrap(text):
    """Word-wrap to BOX_COLS and split into pages of BOX_LINES lines."""
    lines, cur = [], ''
    for w in ascii_text(text).split():
        while len(w) > BOX_COLS:
            if cur: lines.append(cur); cur = ''
            lines.append(w[:BOX_COLS]); w = w[BOX_COLS:]
        if not cur: cur = w
        elif len(cur) + 1 + len(w) <= BOX_COLS: cur += ' ' + w
        else: lines.append(cur); cur = w
    if cur: lines.append(cur)
    return [lines[i:i + BOX_LINES] for i in range(0, len(lines), BOX_LINES)] or [['']]


def encode_text(title, pages):
    """title NUL, then pages: lines joined by 0x0A, pages by 0x0C, end NUL."""
    out = bytearray(ascii_text(title)[:BOX_COLS].encode() + b'\0')
    allp = [pg for p in pages for pg in wrap(p)]
    out += b'\x0c'.join(b'\n'.join(l.encode() for l in pg) for pg in allp) + b'\0'
    return bytes(out)


def build_world(w):
    """Per-map blob: read grid, NPC records, text offset table, texts (see WLD_* in maps.h)."""
    texts, tindex = [], {}

    def text_id(title, pages):
        k = (title, tuple(pages))
        if k not in tindex:
            tindex[k] = len(texts); texts.append(encode_text(title, pages))
        return tindex[k]

    grid = bytearray(704)
    for r in w['reads']:
        grid[r['ty'] * 32 + r['tx']] = text_id(r['title'], r['pages']) + 1
    npcs = w['npcs'][:MAX_NPCS]
    recs = bytearray()
    for k, n in enumerate(npcs):
        first = len(texts)
        for t in n['talks']:  # each talk gets its own entry, in order
            texts.append(encode_text(n['who'], t))
        recs += bytes((n['tx'], n['ty'], first, len(n['talks']), k + 1, 0))
    if len(texts) > 255: raise SystemExit('too many texts in one map')
    head = bytearray(WLD_TOFS + 2 * len(texts))
    head[:704] = grid
    head[WLD_NNPC], head[WLD_NTXT] = len(npcs), len(texts)
    head[WLD_NPCS:WLD_NPCS + len(recs)] = recs
    off = len(head)
    for i, t in enumerate(texts):
        head[WLD_TOFS + 2 * i] = off & 0xFF; head[WLD_TOFS + 2 * i + 1] = off >> 8
        off += len(t)
    return bytes(head) + b''.join(texts), npcs


def sprite_palette(colors):
    return bgr555((0, 0, 0)) + b''.join(bgr555(c) for c in colors) + bgr555((0, 0, 0)) * (15 - len(colors))


def convert_npcs(exp, npcs):
    """Up to 8 NPCs x 3 frames (down, up, side) as 16x16 OBJs from tile 32 on; one palette each."""
    sheet = [bytes(32)] * (16 * 6)   # 6 tile rows = 3 rows of 16x16 frames, 8 per row
    pals = b''
    for k, n in enumerate(npcs):
        im = Image.open(os.path.join(exp, n['file'])).convert('RGBA'); px = im.load()
        colors = sorted({c15(px[x, y]) for x in range(48) for y in range(16) if px[x, y][3] > 0})
        if len(colors) > 15:
            colors = colors[:15]
        pals += sprite_palette(colors)
        for f in range(3):
            idx_ = k * 3 + f
            col, row = (idx_ % 8) * 2, (idx_ // 8) * 2
            for qx, qy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                idx = []
                for y in range(8):
                    for x in range(8):
                        p = px[f * 16 + qx * 8 + x, qy * 8 + y]
                        idx.append(nearest(c15(p), colors) + 1 if p[3] > 0 else 0)
                sheet[(row + qy) * 16 + col + qx] = tile4bpp(idx)
    pals += bytes(32) * (MAX_NPCS - len(npcs))
    return b''.join(sheet), pals[:7 * 32]


def tile2bpp(idx):
    out = bytearray(16)
    for y in range(8):
        for x in range(8):
            v = idx[y * 8 + x]; bit = 7 - x
            if v & 1: out[y * 2] |= 1 << bit
            if v & 2: out[y * 2 + 1] |= 1 << bit
    return bytes(out)


def convert_font(path):
    """BG3 2bpp: tile 0 transparent, 1-96 white glyphs, 97-192 gold glyphs, 193-201 box frame."""
    im = Image.open(path).convert('L'); px = im.load()
    tiles = [bytes(16)]
    for ink in (3, 2):
        for i in range(96):
            gx, gy = (i % 16) * 8, (i // 16) * 8
            tiles.append(tile2bpp([ink if px[gx + x, gy + y] > 127 else 1 for y in range(8) for x in range(8)]))

    def frame(fn):
        return tile2bpp([fn(x, y) for y in range(8) for x in range(8)])
    # 1 = fill, 2 = gold border line two pixels in from the edge, 0 = outside the rounded corner
    def edge(x, y, top, bot, left, right):
        if (top and y == 0) or (bot and y == 7) or (left and x == 0) or (right and x == 7): return 0
        if (top and y == 2) or (bot and y == 5) or (left and x == 2) or (right and x == 5): return 2
        if (top and y < 2) or (bot and y > 5) or (left and x < 2) or (right and x > 5): return 1
        return 1
    def corner(top, left):
        def fn(x, y):
            cx, cy = (x if left else 7 - x), (y if top else 7 - y)
            if cx + cy < 2: return 0
            if (cy == 2 and cx >= 2) or (cx == 2 and cy >= 2): return 2
            return 1
        return fn
    tiles += [frame(corner(True, True)), frame(lambda x, y: edge(x, y, 1, 0, 0, 0)), frame(corner(True, False)),
              frame(lambda x, y: edge(x, y, 0, 0, 1, 0)), frame(lambda x, y: edge(x, y, 0, 0, 0, 1)),
              frame(corner(False, True)), frame(lambda x, y: edge(x, y, 0, 1, 0, 0)), frame(corner(False, False)),
              tile2bpp([3 if (y >= 2 and y <= 5 and abs(x - 3.5) <= (5 - y)) else 1 for y in range(8) for x in range(8)])]
    return b''.join(tiles)


def main():
    exp, root = sys.argv[1], sys.argv[2]
    out = os.path.join(root, 'data')
    os.makedirs(out, exist_ok=True)
    os.makedirs(os.path.join(root, 'src'), exist_ok=True)
    maps = [m for m in json.load(open(os.path.join(exp, 'maps.json'))) if m['ow']]
    maps.sort(key=lambda m: (m['ay'], m['ax']))
    solid = json.load(open(os.path.join(exp, 'solid.json')))['solid']
    pos = {(m['ax'], m['ay']): i for i, m in enumerate(maps)}

    asm = ['.include "hdr.asm"', '']
    hdr = ['/* Generated by tools/snesconv.py. Do not edit. */', '#ifndef MAPS_H', '#define MAPS_H', '',
           f'#define MAP_COUNT {len(maps)}', '#define MAP_W 32', '#define MAP_H 22', '']
    nb = {k: [] for k in 'NSEW'}
    report = []
    world = json.load(open(os.path.join(exp, 'world.json')))
    for i, m in enumerate(maps):
        chr_, tm, pal, nt, npal = convert_bg(os.path.join(exp, m['file']))
        col = bytes(1 if ch in solid else 0 for row in m['grid'] for ch in row)
        assert len(col) == 32 * 22, (m['id'], len(col))
        wld, npcs = build_world(world[m['id']])
        ochr, opal = convert_npcs(exp, npcs)
        for ext, data in (('chr', chr_), ('map', tm), ('pal', pal), ('col', col), ('wld', wld), ('och', ochr), ('opl', opal)):
            open(os.path.join(out, f'm{i}.{ext}'), 'wb').write(data)
        size = sum(map(len, (chr_, tm, pal, col, wld, ochr, opal)))
        if size > 0x8000: raise SystemExit(f'{m["id"]}: {size} bytes, over one 32 KB bank')
        asm += [f'.section ".map{i}" superfree',
                f'm{i}_chr:', f'.incbin "data/m{i}.chr"', f'm{i}_chrend:',
                f'm{i}_map:', f'.incbin "data/m{i}.map"',
                f'm{i}_pal:', f'.incbin "data/m{i}.pal"',
                f'm{i}_col:', f'.incbin "data/m{i}.col"',
                f'm{i}_wld:', f'.incbin "data/m{i}.wld"',
                f'm{i}_och:', f'.incbin "data/m{i}.och"',
                f'm{i}_opl:', f'.incbin "data/m{i}.opl"', '.ends', '']
        hdr.append(f'extern char m{i}_chr, m{i}_chrend, m{i}_map, m{i}_pal, m{i}_col, m{i}_wld, m{i}_och, m{i}_opl;  /* {m["name"]} */')
        ax, ay = m['ax'], m['ay']
        for k, (dx, dy) in zip('NSEW', ((0, -1), (0, 1), (1, 0), (-1, 0))):
            nb[k].append(pos.get((ax + dx, ay + dy), 255))
        report.append(f'{i:2} {m["id"]:8} {m["name"]:32} tiles {nt:4}  palettes {npal}  npcs {len(npcs)}  bytes {size}')

    hero_chr, hero_pal = convert_sprite_strip(os.path.join(exp, 'hero.png'))
    open(os.path.join(out, 'hero.chr'), 'wb').write(hero_chr)
    open(os.path.join(out, 'hero.pal'), 'wb').write(hero_pal)
    asm += ['.section ".hero" superfree', 'hero_chr:', '.incbin "data/hero.chr"', 'hero_chrend:',
            'hero_pal:', '.incbin "data/hero.pal"', 'hero_palend:', '.ends', '']
    font = convert_font(os.path.join(exp, 'font.png'))
    open(os.path.join(out, 'font.chr'), 'wb').write(font)
    asm += ['.section ".font" superfree', 'font_chr:', '.incbin "data/font.chr"', 'font_chrend:', '.ends', '']
    hdr += ['', 'extern char hero_chr, hero_chrend, hero_pal, hero_palend, font_chr, font_chrend;', '',
            f'#define WLD_GRID {WLD_GRID}', f'#define WLD_NNPC {WLD_NNPC}', f'#define WLD_NPCS {WLD_NPCS}',
            f'#define WLD_TOFS {WLD_TOFS}', f'#define MAX_NPCS {MAX_NPCS}', f'#define BOX_COLS {BOX_COLS}',
            '#define FONT_WHITE 1  /* tile for char c is FONT_WHITE + c - 32 */', '#define FONT_GOLD 97',
            '#define FRAME_TL 193', '#define FRAME_T 194', '#define FRAME_TR 195', '#define FRAME_L 196',
            '#define FRAME_R 197', '#define FRAME_BL 198', '#define FRAME_B 199', '#define FRAME_BR 200',
            '#define FRAME_MORE 201', '']
    for k in 'NSEW':
        hdr.append(f'static const u8 nb{k}[MAP_COUNT] = {{{", ".join(map(str, nb[k]))}}};')
    hdr += ['', f'#define MAP_START {pos[(1, 1)]}  /* The Fan */', '', '#endif', '']

    # One loader per map so the C side never needs a table of far pointers.
    loader = ['/* Generated by tools/snesconv.py. Do not edit. */', '#include <snes.h>', '#include "maps.h"', '',
              'extern void rcqUpload(u8 *chr, u16 chrSize, u8 *map, u8 *pal, u8 *col, u8 *wld, u8 *och, u8 *opl);', '',
              'void rcqLoadMap(u8 i)', '{', '    switch (i)', '    {']
    for i in range(len(maps)):
        loader.append(f'    case {i}: rcqUpload(&m{i}_chr, &m{i}_chrend - &m{i}_chr, &m{i}_map, &m{i}_pal, &m{i}_col, &m{i}_wld, &m{i}_och, &m{i}_opl); break;')
    loader += ['    }', '}', '']

    open(os.path.join(root, 'maps.asm'), 'w').write('\n'.join(asm))
    open(os.path.join(root, 'maps.h'), 'w').write('\n'.join(hdr))
    open(os.path.join(root, 'src', 'maploader.c'), 'w').write('\n'.join(loader))
    print('\n'.join(report))


if __name__ == '__main__':
    main()
