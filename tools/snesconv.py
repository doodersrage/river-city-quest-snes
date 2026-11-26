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


def convert_bg(path, extra=(), flip_dedup=True):
    """Tiles, 64x64 tilemap and palettes for one map. Tile 0 is left blank (colour 0, so the
    backdrop shows) for the area outside rooms smaller than the screen. `extra` is a list of
    8x8 tiles (64 15-bit colours each) to add with the same palettes, e.g. opened chests;
    their tilemap entries are returned in the same order."""
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

    chr_data, lookup = [bytes(32)], {}

    def add(idx):
        key = tuple(idx)
        if key not in lookup:
            n = len(chr_data)
            chr_data.append(tile4bpp(idx))
            lookup[key] = (n, 0, 0)
            if flip_dedup:
                rows = [idx[y * 8:(y + 1) * 8] for y in range(8)]
                h = tuple(v for r in rows for v in r[::-1])
                v = tuple(v for r in rows[::-1] for v in r)
                hv = tuple(v for r in rows[::-1] for v in r[::-1])
                lookup.setdefault(h, (n, 1, 0)); lookup.setdefault(v, (n, 0, 1)); lookup.setdefault(hv, (n, 1, 1))
        return lookup[key]

    def entry(idx, pi):
        n, hf, vf = add(idx)
        return n | (pi << 10) | (hf << 14) | (vf << 15)

    entries = []
    for t, pi in zip(tiles, assign):
        pal = pals[pi]
        entries.append(entry([pal.index(c) + 1 if c in pal else nearest(c, pal) + 1 for c in t], pi))
    extra_entries = []
    for t in extra:
        pi = min(range(len(pals)), key=lambda i: sum(min(dist(c, q) for q in pals[i]) for c in t))
        extra_entries.append(entry([nearest(c, pals[pi]) + 1 for c in t], pi))
    if len(chr_data) > MAX_TILES:
        raise SystemExit(f'{path}: {len(chr_data)} tiles, over the {MAX_TILES} limit')

    # 64x64 tilemap in four 32x32 screens: TL, TR, BL, BR. Unused cells stay tile 0 (blank).
    tm = bytearray(64 * 64 * 2)
    for cy in range(th):
        for cx in range(tw):
            e = entries[cy * tw + cx]
            off = tm_offset(cx, cy)
            tm[off * 2] = e & 0xFF; tm[off * 2 + 1] = e >> 8
    palbin = bytearray()
    for i in range(MAX_PALS):
        p = pals[i] if i < len(pals) else []
        palbin += bgr555((0, 0, 0))
        for j in range(PAL_COLORS):
            palbin += bgr555(p[j] if j < len(p) else (0, 0, 0))
    return b''.join(chr_data), bytes(tm), bytes(palbin), len(chr_data), len(pals), extra_entries


def tm_offset(cx, cy):
    return ((cy // 32) * 2 + (cx // 32)) * 1024 + (cy % 32) * 32 + (cx % 32)


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
WLD_GRID, WLD_NNPC, WLD_NTXT, WLD_NPCS, WLD_TOFS, MAX_NPCS = 0, 704, 705, 706, 754, 4
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


def build_world(w, extra_texts=()):
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
        shop = {'shop': 1, 'market': 2}.get(n.get('shop'), 0)
        recs += bytes((n['tx'], n['ty'], first, len(n['talks']), k + 1, shop))
    extra_ids = []
    for title, pages in extra_texts:
        extra_ids.append(text_id(title, pages))
    if len(texts) > 255: raise SystemExit('too many texts in one map')
    if len(w['npcs']) > MAX_NPCS: raise SystemExit(f"{len(w['npcs'])} NPCs, over {MAX_NPCS}")
    head = bytearray(WLD_TOFS + 2 * len(texts))
    head[:704] = grid
    head[WLD_NNPC], head[WLD_NTXT] = len(npcs), len(texts)
    head[WLD_NPCS:WLD_NPCS + len(recs)] = recs
    off = len(head)
    for i, t in enumerate(texts):
        head[WLD_TOFS + 2 * i] = off & 0xFF; head[WLD_TOFS + 2 * i + 1] = off >> 8
        off += len(t)
    return bytes(head) + b''.join(texts), npcs, extra_ids


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
    return b''.join(sheet), pals[:MAX_NPCS * 32]


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


# ---------- Stage 3: combat art ----------
ENEMY_TYPES = ['rat', 'squirrel', 'goose', 'scooter', 'crab', 'bat', 'ghost']
ENEMY_HP = [1, 1, 2, 2, 2, 1, 2]          # HPS in the web game
ENEMY_FRAME_OF = [0, 1, 2, 3, 4, 5, 7]    # bat animates between frames 5 and 6
OBJ_ENEMY, OBJ_PADDLE, OBJ_HUD = 128, 160, 224
HUD_HEART, HUD_HALF, HUD_EMPTY, HUD_COIN, HUD_DIGIT = 0, 1, 2, 3, 5


def obj_tile_of_frame(base, i):
    """16x16 frame i in a 16-tile-wide OBJ sheet starting at tile `base` (a multiple of 32)."""
    return base + (i // 8) * 32 + (i % 8) * 2


def pack_sprite_palettes(sets, n):
    pals, which = [], []
    for st_ in sets:
        best = None
        for i, p in enumerate(pals):
            if len(p | st_) <= 15 and (best is None or len(p | st_) < len(pals[best] | st_)):
                best = i
        if best is None:
            if len(pals) == n: raise SystemExit('sprite colours do not fit')
            pals.append(set()); best = len(pals) - 1
        pals[best] |= st_
        which.append(best)
    return [sorted(p) for p in pals], which


def convert_combat(exp):
    """OBJ tiles 128-255 (enemies, paddle angles, HUD) and sprite palettes 5-7."""
    d = os.path.join(exp, 'combat')
    sheet = [bytes(32)] * 128   # tiles 128..255
    def put16(tile, idx16):      # idx16: 256 palette indices for a 16x16 frame
        for qx, qy in ((0, 0), (1, 0), (0, 1), (1, 1)):
            sheet[tile - 128 + qy * 16 + qx] = tile4bpp([idx16[(qy * 8 + y) * 16 + qx * 8 + x] for y in range(8) for x in range(8)])

    en = Image.open(os.path.join(d, 'enemies.png')).convert('RGBA')
    frames = [[en.getpixel((i * 16 + x, y)) for y in range(16) for x in range(16)] for i in range(8)]
    sets = [frozenset(c15(p) for p in f if p[3]) for f in frames]
    epals, ewhich = pack_sprite_palettes(sets, 2)
    for i, f in enumerate(frames):
        pal = epals[ewhich[i]]
        put16(obj_tile_of_frame(OBJ_ENEMY, i), [pal.index(c15(p)) + 1 if p[3] else 0 for p in f])

    # Palette 7: paddle, hearts, coins, digits.
    pd = Image.open(os.path.join(d, 'paddles.png')).convert('RGBA')
    hud = Image.open(os.path.join(d, 'hud.png')).convert('RGBA')
    p7 = sorted({c15(p) for im in (pd, hud) for p in im.getdata() if p[3]} | {(0, 0, 0), (31, 31, 31)})
    if len(p7) > 15: raise SystemExit(f'palette 7 has {len(p7)} colours')
    ix = lambda p: p7.index(c15(p)) + 1 if p[3] else 0

    pad_ox, pad_oy = [], []
    for a in range(16):
        crop = pd.crop((a * 40, 0, a * 40 + 40, 40))
        bb = crop.getbbox()
        x0, y0, x1, y1 = bb
        # Hero centre is (20, 23). If wider than 16, trim the end nearest the hand.
        if x1 - x0 > 16:
            x0, x1 = (x1 - 16, x1) if (x0 + x1) / 2 > 20 else (x0, x0 + 16)
        if y1 - y0 > 16:
            y0, y1 = (y1 - 16, y1) if (y0 + y1) / 2 > 20 else (y0, y0 + 16)
        x0 -= (16 - (x1 - x0)) // 2; y0 -= (16 - (y1 - y0)) // 2
        px_ = [crop.getpixel((x0 + x, y0 + y)) if 0 <= x0 + x < 40 and 0 <= y0 + y < 40 else (0, 0, 0, 0)
               for y in range(16) for x in range(16)]
        put16(obj_tile_of_frame(OBJ_PADDLE, a), [ix(p) for p in px_])
        pad_ox.append(x0 - 20); pad_oy.append(y0 - 23)

    # 8x8 HUD tiles: heart, half heart, empty heart, coin x2, digits 0-9.
    def hud8(ox):
        cols = [c for c in range(9) if c != 4]   # 9px hearts -> 8px by dropping the middle column
        return [[hud.getpixel((ox + c, y)) for c in cols] for y in range(8)]
    full, empty = hud8(0), hud8(9)
    half = [r[:4] + e[4:] for r, e in zip(full, empty)]
    coin0 = [[hud.getpixel((18 + x, y)) for x in range(8)] for y in range(8)]
    coin1 = [[hud.getpixel((27 + x, y)) for x in range(8)] for y in range(8)]
    hud_tiles = [full, half, empty, coin0, coin1]
    font = Image.open(os.path.join(exp, 'font.png')).convert('L')
    white, black = (255, 255, 255, 255), (0, 0, 0, 255)
    for dgt in range(10):
        gx, gy = ((16 + dgt) % 16) * 8, ((16 + dgt) // 16) * 8
        ink = [[font.getpixel((gx + x, gy + y)) > 127 for x in range(8)] for y in range(8)]
        t = []
        for y in range(8):
            row = []
            for x in range(8):
                if ink[y][x]: row.append(white)
                elif any(0 <= y + j < 8 and 0 <= x + i < 8 and ink[y + j][x + i] for j in (-1, 0, 1) for i in (-1, 0, 1)): row.append(black)
                else: row.append((0, 0, 0, 0))
            t.append(row)
        hud_tiles.append(t)
    for i, t in enumerate(hud_tiles):
        sheet[OBJ_HUD - 128 + i] = tile4bpp([ix(p) for r in t for p in r])
    # spark shot x2, hook head, chain link (nearest palette-7 colours)
    sm = Image.open(os.path.join(d, 'small.png')).convert('RGBA')
    for i in range(4):
        idx = []
        for y in range(8):
            for x in range(8):
                p_ = sm.getpixel((i * 8 + x, y))
                idx.append(nearest(c15(p_), p7) + 1 if p_[3] else 0)
        sheet[OBJ_HUD - 128 + 15 + i] = tile4bpp(idx)

    pals = b''
    for i in range(2):
        pals += sprite_palette(epals[i]) if i < len(epals) else bytes(32)
    pals += sprite_palette(p7)
    tables = ['/* Combat tables (generated). */',
              f'#define OBJ_ENEMY {OBJ_ENEMY}', f'#define OBJ_PADDLE {OBJ_PADDLE}', f'#define OBJ_HUD {OBJ_HUD}',
              f'#define HUD_HEART {OBJ_HUD + HUD_HEART}', f'#define HUD_HALF {OBJ_HUD + HUD_HALF}',
              f'#define HUD_EMPTY {OBJ_HUD + HUD_EMPTY}', f'#define HUD_COIN {OBJ_HUD + HUD_COIN}',
              f'#define HUD_DIGIT {OBJ_HUD + HUD_DIGIT}', '#define PAL_HUD 7',
              f'#define OBJ_SPARK {OBJ_HUD + 15}', f'#define OBJ_HOOK {OBJ_HUD + 17}', f'#define OBJ_CHAIN {OBJ_HUD + 18}',
              f'#define ENEMY_TYPES {len(ENEMY_TYPES)}',
              '/* ' + ' '.join(f'{i}={t}' for i, t in enumerate(ENEMY_TYPES)) + ' */',
              f'static const u8 enemyHp[{len(ENEMY_TYPES)}] = {{{", ".join(map(str, ENEMY_HP))}}};',
              f'static const u16 enemyTile[{len(ENEMY_TYPES)}] = {{{", ".join(str(obj_tile_of_frame(OBJ_ENEMY, f)) for f in ENEMY_FRAME_OF)}}};',
              f'static const u8 enemyPal[{len(ENEMY_TYPES)}] = {{{", ".join(str(5 + ewhich[f]) for f in ENEMY_FRAME_OF)}}};',
              f'#define BAT_TILE2 {obj_tile_of_frame(OBJ_ENEMY, 6)}',
              f'static const u16 paddleTile[16] = {{{", ".join(str(obj_tile_of_frame(OBJ_PADDLE, a)) for a in range(16))}}};',
              f'static const s16 paddleOx[16] = {{{", ".join(map(str, pad_ox))}}};',
              f'static const s16 paddleOy[16] = {{{", ".join(map(str, pad_oy))}}};']
    # Swing: angle goes base-1.1 .. base+1.1 rad over atk 14..6 (web drawPaddle); in 16ths of a turn.
    import math
    swing = []
    for atk in range(15):
        t = min(1, max(0, (14 - atk) / 8))
        swing.append(round((-1.1 + 2.2 * t) * 16 / (2 * math.pi)) % 16)
    spin = [round((1 - sp_ / 22) * 16) % 16 for sp_ in range(23)]
    tables += [f'static const u8 swingStep[15] = {{{", ".join(map(str, swing))}}};  /* by atk */',
               f'static const u8 spinStep[23] = {{{", ".join(map(str, spin))}}};   /* by spin */',
               '/* 0..63 -> sin * 64 */',
               'static const s8 sinTab[64] = {' + ', '.join(str(round(math.sin(i * 2 * math.pi / 64) * 64)) for i in range(64)) + '};']
    return b''.join(sheet), pals, tables


# ---------- Stage 5: bosses (second OBJ name table, tiles 256-511) ----------
def convert_boss(exp):
    d = os.path.join(exp, 'combat')
    fm = Image.open(os.path.join(d, 'foreman.png')).convert('RGBA')
    ct = Image.open(os.path.join(d, 'container.png')).convert('RGBA')
    nframes = fm.width // 48
    sheet = [bytes(32)] * 256
    cells = []   # (image, x0, y0)
    for f in range(nframes):
        for k in range(9):
            cells.append((fm, f * 48 + (k % 3) * 16, (k // 3) * 16))
    cells.append((ct, 0, 0))
    pal1 = sorted({c15(p) for p in fm.getdata() if p[3]})
    pal2 = sorted({c15(p) for p in ct.getdata() if p[3]})
    if len(pal1) > 15 or len(pal2) > 15: raise SystemExit('boss colours do not fit')
    tiles = []
    for i, (im, x0, y0) in enumerate(cells):
        pal = pal2 if im is ct else pal1
        px_ = [im.getpixel((x0 + x, y0 + y)) for y in range(16) for x in range(16)]
        if not any(p[3] for p in px_):
            tiles.append(0); continue
        t = obj_tile_of_frame(256, i)
        for qx, qy in ((0, 0), (1, 0), (0, 1), (1, 1)):
            sheet[t - 256 + qy * 16 + qx] = tile4bpp([pal.index(c15(p)) + 1 if p[3] else 0
                for p in (px_[(qy * 8 + y) * 16 + qx * 8 + x] for y in range(8) for x in range(8))])
        tiles.append(t)
    pals = sprite_palette(pal1) + sprite_palette(pal2)
    defs = [f'#define FOREMAN_FRAMES {nframes}',
            'static const u16 foremanTile[%d] = {%s};  /* frame*9 + cell, 0 = empty */' % (nframes * 9, ', '.join(map(str, tiles[:nframes * 9]))),
            f'#define OBJ_CONTAINER {tiles[-1]}', '#define PAL_BOSS 1', '#define PAL_CONTAINER 2']
    return b''.join(sheet), pals, defs


# ---------- Stage 4: shops ----------
SHOP_NAME, SHOP_LOCK, SHOP_HEAD, SHOP_ITEM = 21, 9, 26, 36
ONCE, REPEAT, LOCKED = 0, 1, 2
EFFECT_NONE, EFFECT_HEART = 0, 1


def convert_shops(extra):
    """Fixed records the ROM can index: header (title[24], count, pad), then items
    (name[21], lock[9], price, kind, requires, effect, message offset u16). Messages follow."""
    blob, msgs, offsets = bytearray(), bytearray(), {}
    order = ['shop', 'market']
    recs = []
    for key in order:
        sh = extra['shops'][key]
        offsets[key] = len(blob)
        head = ascii_text(sh['title'])[:23].encode().ljust(24, b'\0') + bytes((len(sh['items']), 0))
        blob += head
        names = [it['name'] for it in sh['items']]
        for it in sh['items']:
            why = it['whyAtStart']
            kind, req, lock = ONCE, 255, ''
            if it['name'] in ('Virginia Ham', 'Bag of Flour', 'Bag of Sugar', 'Landmark Card Pack'):
                kind = REPEAT
            if it['name'] == 'Cupcake':
                kind, req, lock = REPEAT, names.index('Picnic Basket'), '???'
            elif why:
                kind, lock = LOCKED, why           # needs something from a later stage
            msg = it['msg']
            if it['name'] == 'Heart Charm': msg = 'A heart charm! Your hearts grew by one.'
            if it['name'] == 'Landmark Card Pack': msg = 'You got a landmark card!'
            msg = msg.split(' You have ')[0] or 'Thank you!'
            effect = EFFECT_HEART if it['name'] == 'Heart Charm' else EFFECT_NONE
            mo = len(msgs)
            msgs += encode_text(it['name'], [msg])
            blob += (ascii_text(it['name'])[:SHOP_NAME - 1].encode().ljust(SHOP_NAME, b'\0') +
                     ascii_text(lock)[:SHOP_LOCK - 1].encode().ljust(SHOP_LOCK, b'\0') +
                     bytes((min(255, it['price']), kind, req, effect, mo & 0xFF, mo >> 8)))
    defs = [f'#define SHOP_HEAD {SHOP_HEAD}', f'#define SHOP_ITEM {SHOP_ITEM}', f'#define SHOP_NAME {SHOP_NAME}',
            f'#define SHOP_LOCK {SHOP_LOCK}', '#define SHOP_ONCE 0', '#define SHOP_REPEAT 1', '#define SHOP_LOCKED 2',
            '#define EFFECT_HEART 1', f'#define SHOP_OFS_SHOP {offsets["shop"]}', f'#define SHOP_OFS_MARKET {offsets["market"]}',
            f'#define SHOP_MSGS {len(blob)}', '#define MAX_SHOP_ITEMS 16']
    return bytes(blob + msgs), defs


DOOR_OPEN, DOOR_KEYSTONES, DOOR_CARKEY, DOOR_LATER = 0, 1, 2, 3
CELL_OPEN, CELL_SOLID, CELL_DOOR, CELL_EXIT, CELL_COUNTER, CELL_BUSH, CELL_POST, CELL_LOW, CELL_LOCK = 0, 1, 2, 3, 4, 5, 6, 7, 8


def main():
    exp, root = sys.argv[1], sys.argv[2]
    out = os.path.join(root, 'data')
    os.makedirs(out, exist_ok=True)
    os.makedirs(os.path.join(root, 'src'), exist_ok=True)
    allmaps = json.load(open(os.path.join(exp, 'maps.json')))
    maps = sorted([m for m in allmaps if m['ow']], key=lambda m: (m['ay'], m['ax']))
    maps += sorted([m for m in allmaps if m['port'] and not m['ow']], key=lambda m: m['id'])
    solid = json.load(open(os.path.join(exp, 'solid.json')))['solid']
    pos = {(m['ax'], m['ay']): i for i, m in enumerate(maps) if m['ow']}
    index_of = {m['id']: i for i, m in enumerate(maps)}
    extra = json.load(open(os.path.join(exp, 'extra.json')))
    chest_gid = {}
    dungeon_areas = sorted({m['area'] for m in maps if not m['ow'] and sum(1 for q in maps if q['area'] == m['area']) > 1})
    area_id = {a: i + 1 for i, a in enumerate(dungeon_areas)}   # 0 = not a dungeon
    link_flag = {}
    items_list = ['hook']

    asm = ['.include "hdr.asm"', '']
    hdr = ['/* Generated by tools/snesconv.py. Do not edit. */', '#ifndef MAPS_H', '#define MAPS_H', '',
           f'#define MAP_COUNT {len(maps)}', '#define COL_STRIDE 32', '']
    nb = {k: [] for k in 'NSEW'}
    report = []
    world = json.load(open(os.path.join(exp, 'world.json')))
    combat = json.load(open(os.path.join(exp, 'combat', 'combat.json')))
    for i, m in enumerate(maps):
        # Tiles the ROM swaps in at runtime, cut from the alternate renders (see export.js):
        # chests closed and open, bushes cut, locked doors / gates / cracked walls open, boss stairs.
        chests = m.get('chests') or []
        bushes = (m.get('bushes') or [])[:64]
        dyn = m.get('dyn') or []
        stairs = [(7, 8), (8, 8)] if m.get('bossT') else []
        swaps = []   # (image key, cx, cy)
        for q in chests: swaps.append(('alt', q['tx'], q['ty']))
        for q in chests: swaps.append(('open', q['tx'], q['ty']))
        for b in bushes: swaps.append(('alt', b[0], b[1]))
        for d in dyn:
            for c in d['cells']: swaps.append(('alt', c[0], c[1]))
        for c in stairs: swaps.append(('alt', c[0], c[1]))
        extra_tiles = []
        if swaps:
            imgs = {'alt': Image.open(os.path.join(exp, m['altFile'])).convert('RGB'),
                    'open': Image.open(os.path.join(exp, m['openFile'])).convert('RGB')}
            for key, cx_, cy_ in swaps:
                for qy in (0, 1):
                    for qx in (0, 1):
                        x0, y0 = cx_ * 16 + qx * 8, cy_ * 16 + qy * 8
                        extra_tiles.append([c15(imgs[key].getpixel((x0 + x, y0 + y))) for y in range(8) for x in range(8)])
        def words(k):
            return b''.join(bytes((e & 0xFF, e >> 8)) for e in open_entries[k * 4:k * 4 + 4])
        chr_, tm, pal, nt, npal, open_entries = convert_bg(os.path.join(exp, m['file']), extra_tiles)
        def cell(ch):
            return CELL_BUSH if ch == 'b' else CELL_COUNTER if ch == 'V' else CELL_POST if ch == '+' else \
                CELL_LOCK if ch == 'L' else CELL_LOW if ch in '_w~' and ch in solid else CELL_SOLID if ch in solid else CELL_DOOR if ch == 'D' else CELL_EXIT if ch == 'E' else CELL_OPEN
        col = bytearray(32 * 22)
        for y, row in enumerate(m['grid']):
            for x, ch in enumerate(row):
                col[y * 32 + x] = cell(ch)
        col = bytes(col)
        chest_texts = [('Treasure', extra['gives'].get(q['item']) or ['You found something!']) for q in chests]
        wld, npcs, chest_txt = build_world(world[m['id']], chest_texts)
        # Enemies the ROM knows; bosses are handled by the room's boss type.
        ens = [e for e in combat['enemies'][m['id']] if e[0] in ENEMY_TYPES]
        ene = bytes([len(ens)]) + b''.join(bytes((ENEMY_TYPES.index(t), tx, ty)) for t, tx, ty in ens)
        # Doors, then chests (see maps.h XTR_*).
        xtr = bytearray((m['w'], m['h'], len(m['doors'])))
        for d in m['doors']:
            tgt = index_of.get(d['to'], 255)
            need = DOOR_KEYSTONES if d['need'] == 'keystones' else DOOR_CARKEY if d['need'] == 'carkey' else DOOR_OPEN
            if tgt == 255: need = DOOR_LATER if need == DOOR_OPEN else need
            xtr += bytes((d['tx'], d['ty'], tgt, need))
        # Chests: tx, ty, id, effect, amount, text, appears-when-solved, 4 words closed, 4 words open.
        xtr.append(len(chests))
        nC = len(chests)
        for k, q in enumerate(chests):
            gid = chest_gid.setdefault(m['id'] + ':' + str(q['tx']) + ',' + str(q['ty']), len(chest_gid))
            item = q['item']
            if item.startswith('coins'): effect, amount = 0, int(item[5:])
            elif item == 'hc': effect, amount = 1, 2
            elif item == 'key': effect, amount = 3, 1
            elif item in items_list: effect, amount = 4, items_list.index(item) + 1
            else: effect, amount = 2, 0
            xtr += bytes((q['tx'], q['ty'], gid, effect, amount, chest_txt[k], 1 if q.get('appear') else 0))
            xtr += words(k) + words(nC + k)
        # Bushes: tx, ty and the four tilemap words of cut ground.
        xtr.append(len(bushes))
        base = 2 * nC
        for k, (bx, by) in enumerate(bushes):
            xtr += bytes((bx, by)) + words(base + k)
        base += len(bushes)
        # Room: area, solve (0 none, 1 clear), boss type (0 none, 1 foreman), entry room?
        solve = {'clear': 1, 'lit': 2, 'switch': 3}.get(m.get('solve'), 0)
        boss = {'foreman': 1}.get(m.get('bossT'), 0)
        xtr += bytes((area_id.get(m['area'], 0), solve, boss, 1 if m.get('entry') else 0))
        # Dynamic cell groups: type (1 lock, 2 cracked wall, 3 gate), flag id or solved room, cells (tx, ty, 4 words).
        xtr.append(len(dyn))
        for d in dyn:
            kind = {'L': 1, '%': 2, 'H': 3}.get(d['closed'], 0)
            if kind == 3: ref = index_of[d['solved']]
            else: ref = link_flag.setdefault(d['flag'], len(link_flag))
            xtr += bytes((kind, ref, len(d['cells'])))
            for c in d['cells']:
                xtr += bytes((c[0], c[1])) + words(base)
                base += 1
        # Boss stairs.
        xtr.append(len(stairs))
        for c in stairs:
            xtr += bytes(c) + words(base)
            base += 1
        ochr, opal = convert_npcs(exp, npcs)
        for ext, data in (('chr', chr_), ('map', tm), ('pal', pal), ('col', col), ('wld', wld), ('och', ochr), ('opl', opal), ('ene', ene), ('xtr', xtr)):
            open(os.path.join(out, f'm{i}.{ext}'), 'wb').write(data)
        size = sum(map(len, (chr_, tm, pal, col, wld, ochr, opal, ene, xtr)))
        if size > 0x8000: raise SystemExit(f'{m["id"]}: {size} bytes, over one 32 KB bank')
        asm += [f'.section ".map{i}" superfree',
                f'm{i}_chr:', f'.incbin "data/m{i}.chr"', f'm{i}_chrend:',
                f'm{i}_map:', f'.incbin "data/m{i}.map"',
                f'm{i}_pal:', f'.incbin "data/m{i}.pal"',
                f'm{i}_col:', f'.incbin "data/m{i}.col"',
                f'm{i}_wld:', f'.incbin "data/m{i}.wld"',
                f'm{i}_och:', f'.incbin "data/m{i}.och"',
                f'm{i}_opl:', f'.incbin "data/m{i}.opl"',
                f'm{i}_ene:', f'.incbin "data/m{i}.ene"',
                f'm{i}_xtr:', f'.incbin "data/m{i}.xtr"', '.ends', '']
        hdr.append(f'extern char m{i}_chr, m{i}_chrend, m{i}_map, m{i}_pal, m{i}_col, m{i}_wld, m{i}_och, m{i}_opl, m{i}_ene, m{i}_xtr;  /* {m["name"]} */')
        ax, ay = m['ax'], m['ay']
        for k, (dx, dy) in zip('NSEW', ((0, -1), (0, 1), (1, 0), (-1, 0))):
            if m['ow']: nb[k].append(pos.get((ax + dx, ay + dy), 255))
            elif m['area'] in area_id: nb[k].append(index_of.get(f"{m['area']}:{ax + dx},{ay + dy}", 255))
            else: nb[k].append(255)
        report.append(f'{i:2} {m["id"]:14} {m["name"]:32} tiles {nt:4}  palettes {npal}  npcs {len(npcs)}  bytes {size}')

    hero_chr, hero_pal = convert_sprite_strip(os.path.join(exp, 'hero.png'))
    open(os.path.join(out, 'hero.chr'), 'wb').write(hero_chr)
    open(os.path.join(out, 'hero.pal'), 'wb').write(hero_pal)
    asm += ['.section ".hero" superfree', 'hero_chr:', '.incbin "data/hero.chr"', 'hero_chrend:',
            'hero_pal:', '.incbin "data/hero.pal"', 'hero_palend:', '.ends', '']
    boss_text = json.load(open(os.path.join(exp, 'combat', 'boss.json')))['text']
    # System texts, from the web game's hurt() and the swing tutorial.
    sys_texts = [('DEAD', '', ['The fog takes you...', 'You wake by the fountain in Capitol Square.']),
                 # door bounces, from the web game
                 ('KEYSTONES', '', ['The tunnel mouth is sealed. Four hollows in the stone wait for keystones.']),
                 ('CARKEY', '', ['The Carillon door is locked. The caretaker nearby might know where the key went.']),
                 ('LATER', '', ["The way in is dark. This part of Richmond isn't open yet in the SNES version."]),
                 ('NOCOINS', '', ["You don't have enough coins."]),
                 ('SOLDOUT', '', ['Sold out.']),
                 ('NOTYET', '', ["You can't buy that yet."]),
                 ('LOCKED', '', ['Locked tight. A small key from somewhere in here would open it.']),
                 ('PLATE', '', ['The chest plate rips loose!']),
                 ('DEADIN', '', ['The fog takes you...', 'You come to at the entrance.']),
                 ('CONTAINER', '', ['A heart charm! Your hearts grew by one.']),
                 ('FOREMAN_IN', 'Furnace Hall', boss_text['foremanIntro']),
                 ('FOREMAN_OUT', 'Furnace Hall', boss_text['foreman'])]
    blob, defs = b'', []
    for name, title, pages in sys_texts:
        defs.append(f'#define TXT_{name} {len(blob)}')
        blob += encode_text(title, pages)
    open(os.path.join(out, 'sys.txt'), 'wb').write(blob)
    asm += ['.section ".systext" superfree', 'sys_txt:', '.incbin "data/sys.txt"', '.ends', '']
    hdr += ['extern char sys_txt;'] + defs + [f'#define MAP_CAPITOL {pos[(2, 1)]}', '']
    shops, shop_defs = convert_shops(extra)
    open(os.path.join(out, 'shops.bin'), 'wb').write(shops)
    asm += ['.section ".shops" superfree', 'shops_bin:', '.incbin "data/shops.bin"', '.ends', '']
    hdr += ['extern char shops_bin;'] + shop_defs + [f'#define CHEST_COUNT {len(chest_gid)}',
            '#define CELL_OPEN 0', '#define CELL_SOLID 1', '#define CELL_DOOR 2', '#define CELL_EXIT 3', '#define CELL_COUNTER 4', '#define CELL_BUSH 5', '#define CELL_POST 6', '#define CELL_LOW 7', '#define CELL_LOCK 8',
            '#define DOOR_OPEN 0', '#define DOOR_KEYSTONES 1', '#define DOOR_CARKEY 2', '#define DOOR_LATER 3', '']
    bobj, bpal, bdefs = convert_boss(exp)
    open(os.path.join(out, 'boss.chr'), 'wb').write(bobj)
    open(os.path.join(out, 'boss.pal'), 'wb').write(bpal)
    asm += ['.section ".boss" superfree', 'boss_chr:', '.incbin "data/boss.chr"', 'boss_chrend:',
            'boss_pal:', '.incbin "data/boss.pal"', '.ends', '']
    hdr += ['extern char boss_chr, boss_chrend, boss_pal;'] + bdefs + [
            f'#define AREA_COUNT {len(dungeon_areas) + 1}', f'#define LINK_COUNT {max(1, len(link_flag))}',
            '#define ITEM_HOOK 1', '#define EFFECT_KEY 3', '#define EFFECT_ITEM 4', '']
    cobj, cpal, ctables = convert_combat(exp)
    open(os.path.join(out, 'combat.chr'), 'wb').write(cobj)
    open(os.path.join(out, 'combat.pal'), 'wb').write(cpal)
    asm += ['.section ".combat" superfree', 'combat_chr:', '.incbin "data/combat.chr"', 'combat_chrend:',
            'combat_pal:', '.incbin "data/combat.pal"', '.ends', '']
    hdr += ['extern char combat_chr, combat_chrend, combat_pal;', ''] + ctables + ['']
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
              'extern void rcqUpload(u8 *chr, u16 chrSize, u8 *map, u8 *pal, u8 *col, u8 *wld, u8 *och, u8 *opl, u8 *ene, u8 *xtr);', '',
              'void rcqLoadMap(u8 i)', '{', '    switch (i)', '    {']
    for i in range(len(maps)):
        loader.append(f'    case {i}: rcqUpload(&m{i}_chr, &m{i}_chrend - &m{i}_chr, &m{i}_map, &m{i}_pal, &m{i}_col, &m{i}_wld, &m{i}_och, &m{i}_opl, &m{i}_ene, &m{i}_xtr); break;')
    loader += ['    }', '}', '']

    open(os.path.join(root, 'maps.asm'), 'w').write('\n'.join(asm))
    open(os.path.join(root, 'maps.h'), 'w').write('\n'.join(hdr))
    open(os.path.join(root, 'src', 'maploader.c'), 'w').write('\n'.join(loader))
    print('\n'.join(report))


if __name__ == '__main__':
    main()
