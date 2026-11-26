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


GIFT_ITEMS = ['hook', 'powder', 'lantern', 'shield', 'bug', 'can', 'balls', 'shoes', 'bike', 'bottle']   # ITEM_* in the ROM, from 1
NPC_REC = 12
RADII = [15, 32, 38, 44, 46, 48, 54, 68]   # windows, night, dark room, brazier, torch, lamp, night+lantern, dark+lantern


def light_tables(maps):
    """Per map light sources (pixel x/2, y/2, kind 0 torch 1 lamp 2 window) and circle half-widths per radius."""
    import math
    out, data, offs = [], [], []
    for m in maps:
        offs.append(len(data) // 3)
        L = m.get('lights') or {}
        church = m['id'] == 'ow:3,0'          # web litLamp: Church Hill's lamps stay dark (the lamplighter challenge)
        for kind, pts in ((0, L.get('torches', [])), (1, [] if church else L.get('lamps', [])), (2, L.get('wins', []))):
            for x, y in pts:
                data += [min(255, int(x) // 2), min(255, int(y) // 2), kind]
    offs.append(len(data) // 3)
    out.append('static const u16 lightOff[%d] = {%s};' % (len(offs), ', '.join(map(str, offs))))
    out.append('static const u8 lightData[%d] = {%s};' % (max(1, len(data)), ', '.join(map(str, data or [0]))))
    for r in RADII:
        out.append('static const u8 hw%d[%d] = {%s};' % (r, r + 1, ', '.join(str(int(math.sqrt(r * r - d * d))) for d in range(r + 1))))
    return out


LIT_AREAS = {'bank', 'byrd', 'capitol', 'vmfa', 'taproom', 'tearoom', 'hippo', 'stjohn', 'shop', 'rowhouse', 'station',
             'mansion', 'boathouse', 'studio'}   # web maps with lit:true (the Conservatory is lit but plays the dungeon song)
TRADE = {'none': 0, 'melon': 1, 'music': 2, 'rivermap': 3, 'kayak': 4, 'bib': 5, 'shoes': 6}


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
        gift_talk, gift_item = 255, 0
        for t_, item in n.get('gifts') or []:
            if item in GIFT_ITEMS:
                gift_talk, gift_item = t_, GIFT_ITEMS.index(item) + 1
        # trading quest: what this person wants, what they say, what they give
        q_need, q_text, q_give = 255, 0, 0
        for q in n.get('quest') or []:
            q_need, q_give = TRADE[q['need']], TRADE[q['give']]
            q_text = len(texts)
            texts.append(encode_text(n['who'], q['pages']))
        # coming back with the bottle empty or the balls low (the Brewer, the Ballplayer)
        refill = 255
        if n.get('refill'):
            refill = len(texts)
            texts.append(encode_text(n['who'], n['refill']['pages']))
        recs += bytes((n['tx'], n['ty'], first, len(n['talks']), k + 1, shop, gift_talk, gift_item, q_need, q_text, q_give, refill))
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
    for i in range(11):
        idx = []
        for y in range(8):
            for x in range(8):
                p_ = sm.getpixel((i * 8 + x, y))
                idx.append(nearest(c15(p_), p7) + 1 if p_[3] else 0)
        sheet[OBJ_HUD - 128 + 15 + i] = tile4bpp(idx)

    p7x = item_palette(p7)
    pals = b''
    for i in range(2):
        pals += sprite_palette(epals[i]) if i < len(epals) else bytes(32)
    pals += sprite_palette(p7x)
    items_chr, item_defs = convert_items(exp, p7x)
    open(os.path.join(exp, '..', 'items.chr'), 'wb').write(items_chr)
    tables = ['/* Combat tables (generated). */',
              f'#define OBJ_ENEMY {OBJ_ENEMY}', f'#define OBJ_PADDLE {OBJ_PADDLE}', f'#define OBJ_HUD {OBJ_HUD}',
              f'#define HUD_HEART {OBJ_HUD + HUD_HEART}', f'#define HUD_HALF {OBJ_HUD + HUD_HALF}',
              f'#define HUD_EMPTY {OBJ_HUD + HUD_EMPTY}', f'#define HUD_COIN {OBJ_HUD + HUD_COIN}',
              f'#define HUD_DIGIT {OBJ_HUD + HUD_DIGIT}', '#define PAL_HUD 7',
              f'#define OBJ_SPARK {OBJ_HUD + 15}', f'#define OBJ_HOOK {OBJ_HUD + 17}', f'#define OBJ_CHAIN {OBJ_HUD + 18}',
              f'#define OBJ_ORB {OBJ_HUD + 19}', f'#define OBJ_REFL {OBJ_HUD + 20}', f'#define OBJ_FEATHER {OBJ_HUD + 21}', f'#define OBJ_RING {OBJ_HUD + 22}', f'#define OBJ_SEED {OBJ_HUD + 23}', f'#define OBJ_RAIN {OBJ_HUD + 24}', f'#define OBJ_SNOW {OBJ_HUD + 25}',
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
    tables += item_defs
    return b''.join(sheet), pals, tables


# ---------- Stages 5-6: the second OBJ name table (tiles 256-511) ----------
# Tiles 256-351 (24 cells) hold items, loaded once: keg, hook, 4 keystones, heart container,
# lantern, explosion (2 frames x 4 cells), Gold-Bug, small key. Tiles 352+ hold the current boss,
# loaded in its room; only its non-empty cells take space.
ITEM_CELLS = {'keg': 0, 'hook': 1, 'granite': 2, 'iron': 3, 'brick': 4, 'bronze': 5, 'container': 6, 'lantern': 7,
              'boom': 8, 'bug': 16, 'key': 17, 'carkey': 18, 'piece': 19, 'block': 20, 'can': 21,
              'ball': 22, 'shoes': 23, 'bike': 24, 'bottle': 25, 'bib': 26, 'bikeside': 27, 'bikefront': 28}
ITEM_SHEET_CELLS = 30
BOSS_FIRST_CELL = 30
BOSSES = ['foreman', 'turtle', 'wraith', 'raven', 'bell', 'arum', 'vampire']          # boss type 1, 2, ...


def item_palette(p7):
    """Palette 7 plus three greys/blues for the hook and the granite and iron keystones."""
    return p7 + [hexc('#6c6e7a'), hexc('#a3a6ad'), hexc('#4d5f7a')][:15 - len(p7)]


def convert_items(exp, pal):
    d = os.path.join(exp, 'combat')
    sheet = [bytes(32)] * ((ITEM_SHEET_CELLS + 7) // 8 * 32)   # whole rows of 8 cells; the boss loads over the rest later
    def put(cell, im, x0, y0):
        t = obj_tile_of_frame(0, cell)
        for qx, qy in ((0, 0), (1, 0), (0, 1), (1, 1)):
            idx = []
            for y in range(8):
                for x in range(8):
                    p_ = im.getpixel((x0 + qx * 8 + x, y0 + qy * 8 + y))
                    idx.append(nearest(c15(p_), pal) + 1 if p_[3] > 96 else 0)
            sheet[t + qy * 16 + qx] = tile4bpp(idx)
    icons = Image.open(os.path.join(d, 'icons.png')).convert('RGBA')
    for i, name in enumerate(['keg', 'hook', 'granite', 'iron', 'brick', 'bronze', 'lantern', 'bug', 'key', 'carkey', 'piece', 'can', 'ball', 'shoes', 'bike', 'bottle', 'bib']):
        put(ITEM_CELLS[name], icons, i * 16, 0)
    # the push block has its own palette (sprite palette 3, free in dungeon rooms)
    blk = Image.open(os.path.join(d, 'block.png')).convert('RGBA')
    bpal = sorted({c15(p_) for p_ in blk.getdata() if p_[3]})
    t = obj_tile_of_frame(0, ITEM_CELLS['block'])
    for qx, qy in ((0, 0), (1, 0), (0, 1), (1, 1)):
        sheet[t + qy * 16 + qx] = tile4bpp([bpal.index(c15(blk.getpixel((qx * 8 + x, qy * 8 + y)))) + 1
            if blk.getpixel((qx * 8 + x, qy * 8 + y))[3] else 0 for y in range(8) for x in range(8)])
    open(os.path.join(exp, '..', 'block.pal'), 'wb').write(sprite_palette(bpal))
    bikes = Image.open(os.path.join(d, 'bikes.png')).convert('RGBA')
    put(ITEM_CELLS['bikeside'], bikes, 0, 0)
    put(ITEM_CELLS['bikefront'], bikes, 16, 0)
    put(ITEM_CELLS['container'], Image.open(os.path.join(d, 'container.png')).convert('RGBA'), 0, 0)
    boom = Image.open(os.path.join(d, 'boom.png')).convert('RGBA')
    for f in range(2):
        for k in range(4):
            put(ITEM_CELLS['boom'] + f * 4 + k, boom, f * 32 + (k % 2) * 16, (k // 2) * 16)
    defs = [f'#define OBJ_I_{n.upper()} {256 + obj_tile_of_frame(0, c)}' for n, c in ITEM_CELLS.items()]
    defs.append('#define OBJ_I_KEYSTONE OBJ_I_GRANITE  /* +2 per keystone: granite, iron, brick, bronze */')
    return b''.join(sheet), defs


def convert_bosses(exp):
    d = os.path.join(exp, 'combat')
    blobs, pals, defs = [], [], []
    for bi, name in enumerate(BOSSES):
        im = Image.open(os.path.join(d, f'{name}.png')).convert('RGBA')
        nframes = im.width // 48
        pal = sorted({c15(p) for p in im.getdata() if p[3]})
        if len(pal) > 15: raise SystemExit(f'{name}: {len(pal)} colours')
        first = BOSS_FIRST_CELL * 4      # tile offset of the boss area within the second table
        sheet = [bytes(32)] * (256 - first)
        tiles = []
        cell = BOSS_FIRST_CELL
        for f in range(nframes):
            for k in range(9):
                x0, y0 = f * 48 + (k % 3) * 16, (k // 3) * 16
                px_ = [im.getpixel((x0 + x, y0 + y)) for y in range(16) for x in range(16)]
                if not any(p[3] for p in px_):
                    tiles.append(0); continue
                t = obj_tile_of_frame(0, cell)
                cell += 1
                if t + 17 >= 256: raise SystemExit(f'{name}: too many sprite cells')
                for qx, qy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    sheet[t - first + qy * 16 + qx] = tile4bpp([pal.index(c15(p)) + 1 if p[3] else 0
                        for p in (px_[(qy * 8 + y) * 16 + qx * 8 + x] for y in range(8) for x in range(8))])
                tiles.append(256 + t)
        used = max((t - 256 - first for t in tiles if t), default=0) + 18
        blobs.append(b''.join(sheet[:used]))
        pals.append(sprite_palette(pal))
        defs.append('static const u16 boss%dTile[%d] = {%s};  /* %s: frame*9 + cell, 0 = empty */'
                    % (bi + 1, nframes * 9, ', '.join(map(str, tiles)), name))
    defs.append(f'#define VRAM_BOSS_OFS {BOSS_FIRST_CELL * 4 * 16}  /* words into the second OBJ table */')
    return blobs, pals, defs


# ---------- Stage 4: shops ----------
SHOP_NAME, SHOP_LOCK, SHOP_HEAD, SHOP_ITEM = 21, 9, 26, 36
ONCE, REPEAT, LOCKED, NEEDS = 0, 1, 2, 3
EFFECT_NONE, EFFECT_HEART, EFFECT_BALLS, EFFECT_KEGS, EFFECT_GINGER, EFFECT_SATCHEL = 0, 1, 2, 3, 4, 5


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
            needs = {'10 Baseballs': (7, EFFECT_BALLS), '4 Powder Kegs': (2, EFFECT_KEGS), 'Ginger Beer': (10, EFFECT_GINGER)}
            if it['name'] in needs:
                kind, req, lock = NEEDS, needs[it['name']][0], why    # lock text until you have the item
            if it['name'] == 'Cupcake':
                kind, req, lock = REPEAT, names.index('Picnic Basket'), '???'
            elif why and kind != NEEDS:
                kind, lock = LOCKED, why           # needs something from a later stage
            msg = it['msg']
            if it['name'] == 'Heart Charm': msg = 'A heart charm! Your hearts grew by one.'
            if it['name'] == 'Landmark Card Pack': msg = 'You got a landmark card!'
            effect = {'Heart Charm': EFFECT_HEART, 'Big Satchel': EFFECT_SATCHEL}.get(it['name'], EFFECT_NONE)
            if kind == NEEDS: effect = needs[it['name']][1]
            if not msg: msg = {EFFECT_BALLS: 'Ten baseballs.', EFFECT_KEGS: 'Four powder kegs.', EFFECT_GINGER: 'Your bottle is full of ginger beer.',
                               EFFECT_SATCHEL: 'A big satchel. You can carry more baseballs and kegs now.'}.get(effect, '')
            msg = msg.split(' You have ')[0] or 'Thank you!'
            mo = len(msgs)
            msgs += encode_text(it['name'], [msg])
            blob += (ascii_text(it['name'])[:SHOP_NAME - 1].encode().ljust(SHOP_NAME, b'\0') +
                     ascii_text(lock)[:SHOP_LOCK - 1].encode().ljust(SHOP_LOCK, b'\0') +
                     bytes((min(255, it['price']), kind, req, effect, mo & 0xFF, mo >> 8)))
    defs = [f'#define SHOP_HEAD {SHOP_HEAD}', f'#define SHOP_ITEM {SHOP_ITEM}', f'#define SHOP_NAME {SHOP_NAME}',
            f'#define SHOP_LOCK {SHOP_LOCK}', '#define SHOP_ONCE 0', '#define SHOP_REPEAT 1', '#define SHOP_LOCKED 2', '#define SHOP_NEEDS 3',
            '#define EFFECT_HEART 1', '#define EFFECT_BALLS 2', '#define EFFECT_KEGS 3', '#define EFFECT_GINGER 4', '#define EFFECT_SATCHEL 5', f'#define SHOP_OFS_SHOP {offsets["shop"]}', f'#define SHOP_OFS_MARKET {offsets["market"]}',
            f'#define SHOP_MSGS {len(blob)}', '#define MAX_SHOP_ITEMS 16']
    return bytes(blob + msgs), defs


DOOR_OPEN, DOOR_KEYSTONES, DOOR_CARKEY, DOOR_LATER = 0, 1, 2, 3
CELL_OPEN, CELL_SOLID, CELL_DOOR, CELL_EXIT, CELL_COUNTER, CELL_BUSH, CELL_POST, CELL_LOW, CELL_LOCK, CELL_BRAZIER, CELL_RIVER, CELL_SPROUT = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11


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
    pickup_gid = {}
    items_list = GIFT_ITEMS

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
        braz = m.get('braz') or []
        swaps = []   # (image key, cx, cy)
        for q in chests: swaps.append(('alt', q['tx'], q['ty']))
        for q in chests: swaps.append(('open', q['tx'], q['ty']))
        for b in bushes: swaps.append(('alt', b[0], b[1]))
        for d in dyn:
            for c in d['cells']: swaps.append(('alt', c[0], c[1]))
        for c in stairs: swaps.append(('alt', c[0], c[1]))
        for c in braz: swaps.append(('alt', c[0], c[1]))
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
            return CELL_BRAZIER if ch in 'jJ' else CELL_BUSH if ch == 'b' else CELL_COUNTER if ch == 'V' else CELL_POST if ch == '+' else \
                CELL_LOCK if ch == 'L' else CELL_SPROUT if ch == 'z' else CELL_RIVER if ch == '~' else CELL_LOW if ch in '_w' and ch in solid else CELL_SOLID if ch in solid else CELL_DOOR if ch == 'D' else CELL_EXIT if ch == 'E' else CELL_OPEN
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
            elif item == 'piece': effect, amount = 5, 1
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
        solve = {'clear': 1, 'lit': 2, 'switch': 3}.get(m.get('solve'), 0)   # SOLVE_*
        boss = BOSSES.index(m['bossT']) + 1 if m.get('bossT') in BOSSES else 0
        reward = ['granite', 'iron', 'brick', 'bronze'].index(m['reward']) + 1 if m.get('reward') else 0
        xtr += bytes((area_id.get(m['area'], 0), solve, boss, (1 if m.get('entry') else 0) | (reward << 4)))
        # Dynamic cell groups: type (1 lock, 2 cracked wall, 3 gate), flag id or solved room, cells (tx, ty, 4 words).
        xtr.append(len(dyn))
        for d in dyn:
            kind = {'L': 1, '%': 2, 'H': 3, 'z': 4, '_': 5}.get(d['closed'], 0)   # 4 sprout, 5 its vine bridge
            if kind == 3: ref = index_of[d['solved']]
            else: ref = link_flag.setdefault(d['flag'], len(link_flag))
            xtr += bytes((kind, ref, len(d['cells']), CELL_DOOR if d['open'] == 'D' else CELL_OPEN))   # a grown sprout is the foot of its vine: you climb across
            for c in d['cells']:
                xtr += bytes((c[0], c[1])) + words(base)
                base += 1
        # Boss stairs.
        xtr.append(len(stairs))
        for c in stairs:
            xtr += bytes(c) + words(base)
            base += 1
        # Braziers: tx, ty and the four tilemap words of a lit brazier.
        xtr.append(len(braz))
        for c in braz:
            xtr += bytes(c) + words(base)
            base += 1
        # Pickups lying in the room (the key on the Enchanted Garden island): x, y (2 bytes each), kind, id.
        pk = m.get('pickups') or []
        xtr.append(len(pk))
        for it in pk:
            gid = pickup_gid.setdefault(m['id'] + ':' + it['id'], len(pickup_gid))
            kind = {'skey': 9, 'carkey': 10, 'piece': 11, 'bib': 12}[it['kind']]
            xtr += bytes((it['x'] & 0xFF, it['x'] >> 8, it['y'] & 0xFF, it['y'] >> 8, kind, gid))
        # Push blocks and the switches they go on (tx, ty each).
        bl, sw = (m.get('blocks') or [])[:4], m.get('switches') or []
        xtr.append(len(bl))
        for c in bl: xtr += bytes(c)
        xtr.append(len(sw))
        for c in sw: xtr += bytes(c)
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
    boss_json = json.load(open(os.path.join(exp, 'combat', 'boss.json')))
    boss_text = dict(boss_json['text'], keynames=boss_json['keynames'])
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
                 ('FOREMAN_OUT', 'Furnace Hall', boss_text['foreman']),
                 ('TURTLE_IN', 'Quarry Pond', boss_text['turtleIntro']),
                 ('TURTLE_OUT', 'Quarry Pond', boss_text['turtle']),
                 ('WRAITH_IN', 'Heart of the Pyramid', boss_text['wraithIntro']),
                 ('WRAITH_OUT', 'Heart of the Pyramid', boss_text['wraith']),
                 ('RAVEN_IN', "The Raven's Roost", boss_text['ravenIntro']),
                 ('RAVEN_OUT', "The Raven's Roost", boss_text['raven']),
                 ('SKEY', '', ['The Gold-Bug brought back a small key!']),
                 ('BELL_IN', 'The Great Bell', boss_text['bellIntro']),
                 ('BELL_OUT', 'The Great Bell', boss_text['bell']),
                 ('VAMPIRE_IN', 'Engine 231', boss_text['vampireIntro']),
                 ('VAMPIRE_OUT', 'Engine 231', boss_text['vampire']),
                 ('SWARM', '', ['It bursts into a swarm of bats!']),
                 ('DESPERATE', '', ['Engine 231 groans. The thing is desperate!']),
                 ('ARUM_IN', 'Under the Dome', boss_text['arumIntro']),
                 ('ARUM_OUT', 'Under the Dome', boss_text['arum']),
                 ('GROW', '', [boss_text['can']]),
                 ('BLOOM', '', [boss_text['bloom']]),
                 ('CARKEY_GOT', '', ['You found the Carillon Key!', 'Bring it to the Carillon in Byrd Park.']),
                 ('PIECE1', 'Treasure', ['A heart piece! (1 of 4) Find four to grow a heart.']),
                 ('PIECE2', 'Treasure', ['A heart piece! (2 of 4) Find four to grow a heart.']),
                 ('PIECE3', 'Treasure', ['A heart piece! (3 of 4) Find four to grow a heart.']),
                 ('PIECE4', 'Treasure', ['A heart piece! That makes four. Your hearts grew by one.']),
                 ('NOPOWDER', '', ['Out of blasting powder.']),
                 ('NOBALLS', '', ['Out of baseballs.']),
                 ('NOBIKE', '', ['Save the bike for outdoors.']),
                 ('DRINK', '', ['Ahh. Every heart refilled.']),
                 ('EMPTY', '', ['The bottle is empty. The brewer refills it.']),
                 ('FULL', '', ["You can't carry any more of that."]),
                 ('BIB', 'Race Bib', boss_text['bib'])]
    # "You found the Granite Keystone! (1 of 4)" for every keystone and count (web collect())
    for k, kname in enumerate(['granite', 'iron', 'brick', 'bronze']):
        for n in range(1, 5):
            sys_texts.append((f'KEY{k}_{n}', 'Keystone', [f"You found the {boss_text['keynames'][kname]} Keystone! ({n} of 4)",
                              'All four hum together. The tunnel on Church Hill is waiting.' if n >= 4 else 'Take the stairs to head back up.']))
    blob, defs = b'', []
    for name, title, pages in sys_texts:
        defs.append(f'#define TXT_{name} {len(blob)}')
        blob += encode_text(title, pages)
    open(os.path.join(out, 'sys.txt'), 'wb').write(blob)
    asm += ['.section ".systext" superfree', 'sys_txt:', '.incbin "data/sys.txt"', '.ends', '']
    # The ending (web startRelight and CREDITS): five neighbourhoods, then pages of credits.
    relight = boss_text['relight']
    relight_maps = [index_of[r[0]] for r in relight]
    win_door = next(d for m in maps if m['id'] == 'ow:3,0' for d in m['doors'] if d['to'].startswith('tunnel'))
    def wrap(line, w=28):
        out, cur = [], ''
        for word in line.split(' '):
            if cur and len(cur) + 1 + len(word) > w: out.append(cur); cur = word
            else: cur = (cur + ' ' + word).strip()
        return out + [cur]
    groups, g = [], []
    for line in boss_text['credits'] + ['']:
        if line: g += wrap(ascii_text(line))
        elif g: groups.append(g); g = []
    pages, pg = [], []
    for g in groups:
        while len(g) > 6:                      # a long list gets split over pages
            if pg: pages.append(pg); pg = []
            pages.append(g[:5]); g = g[5:]
        if pg and len(pg) + 1 + len(g) > 7: pages.append(pg); pg = []
        pg = pg + ([''] if pg else []) + g
    if pg: pages.append(pg)
    credit_lines = [l for p_ in pages for l in p_ + ['\f']]
    hdr += ['extern char sys_txt;'] + defs + [
            # web wantSong(): outdoors 'south' below the river (ay 3) else 'city'; the Tredegar works 'forge';
            # lit interiors 'city'; every other dungeon, cave and museum 'dungeon' (song numbers from tools/mkaudio.py)
            'static const u8 mapSong[MAP_COUNT] = {%s};' % ', '.join(str(
                (2 if m['ay'] == 3 else 1) if m['ow'] else 5 if m['area'] == 'tredegar' else
                1 if m['area'] in LIT_AREAS else 3) for m in maps),
            # Lighting (stage 8, web drawNight/drawLight): 0 outdoors (dark at night), 1 a lit interior, 2 a dark room, 3 the tunnel
            'static const u8 mapDark[MAP_COUNT] = {%s};' % ', '.join(str(
                0 if m['ow'] else 1 if m.get('lit') and m['area'] != 'conserv' else 3 if m['area'] == 'tunnel' else 2) for m in maps),
            *light_tables(maps),
            '#define RELIGHT_N %d' % len(relight),
            '#define MAP_WIN %d' % index_of['ow:3,0'],
            '#define WIN_X %d' % (win_door['ret'][0] * 16 + 8), '#define WIN_Y %d' % (win_door['ret'][1] * 16 + 8),
            'static const u8 relightMap[%d] = {%s};' % (len(relight), ', '.join(map(str, relight_maps))),
            'static const char *const relightName[%d] = {%s};' % (len(relight), ', '.join('"%s"' % ascii_text(r[1]) for r in relight)),
            '#define CREDIT_LINES %d' % len(credit_lines),
            'static const char *const creditLine[%d] = {%s};  /* "\\f" ends a page */' % (len(credit_lines), ', '.join('"%s"' % l.replace('"', '\\"').replace('\f', '\\f') for l in credit_lines)),
            f'#define MAP_CAPITOL {pos[(2, 1)]}',
            'static const u16 keystoneTxt[16] = {%s};' % ', '.join(f'TXT_KEY{k}_{n}' for k in range(4) for n in range(1, 5)),
            'static const u16 bossInTxt[8] = {0, TXT_FOREMAN_IN, TXT_TURTLE_IN, TXT_WRAITH_IN, TXT_RAVEN_IN, TXT_BELL_IN, TXT_ARUM_IN, TXT_VAMPIRE_IN};',
            'static const u16 bossOutTxt[8] = {0, TXT_FOREMAN_OUT, TXT_TURTLE_OUT, TXT_WRAITH_OUT, TXT_RAVEN_OUT, TXT_BELL_OUT, TXT_ARUM_OUT, TXT_VAMPIRE_OUT};',
            'static const u16 pieceTxt[4] = {TXT_PIECE4, TXT_PIECE1, TXT_PIECE2, TXT_PIECE3};  /* by pieces % 4 */', '']
    shops, shop_defs = convert_shops(extra)
    open(os.path.join(out, 'shops.bin'), 'wb').write(shops)
    asm += ['.section ".shops" superfree', 'shops_bin:', '.incbin "data/shops.bin"', '.ends', '']
    hdr += ['extern char shops_bin;'] + shop_defs + [f'#define CHEST_COUNT {len(chest_gid)}',
            '#define CELL_OPEN 0', '#define CELL_SOLID 1', '#define CELL_DOOR 2', '#define CELL_EXIT 3', '#define CELL_COUNTER 4', '#define CELL_BUSH 5', '#define CELL_POST 6', '#define CELL_LOW 7', '#define CELL_LOCK 8', '#define CELL_BRAZIER 9', '#define CELL_RIVER 10', '#define CELL_SPROUT 11',
            '#define DOOR_OPEN 0', '#define DOOR_KEYSTONES 1', '#define DOOR_CARKEY 2', '#define DOOR_LATER 3', '']
    bblobs, bpals, bdefs = convert_bosses(exp)
    for i, (bb, bp) in enumerate(zip(bblobs, bpals)):
        open(os.path.join(out, f'boss{i + 1}.chr'), 'wb').write(bb)
        open(os.path.join(out, f'boss{i + 1}.pal'), 'wb').write(bp)
        asm += [f'.section ".boss{i + 1}" superfree', f'boss{i + 1}_chr:', f'.incbin "data/boss{i + 1}.chr"', f'boss{i + 1}_chrend:',
                f'boss{i + 1}_pal:', f'.incbin "data/boss{i + 1}.pal"', '.ends', '']
        hdr.append(f'extern char boss{i + 1}_chr, boss{i + 1}_chrend, boss{i + 1}_pal;')
    hdr += bdefs + ['#define PAL_BOSS 1', '#define PAL_ITEM 7',
            f'#define AREA_COUNT {len(dungeon_areas) + 1}', f'#define LINK_COUNT {max(1, len(link_flag))}',
            '#define ITEM_HOOK 1', '#define ITEM_POWDER 2', '#define ITEM_LANTERN 3', '#define ITEM_SHIELD 4', '#define ITEM_BUG 5', '#define ITEM_CAN 6', '#define ITEM_BALLS 7', '#define ITEM_SHOES 8', '#define ITEM_BIKE 9', '#define ITEM_BOTTLE 10',
            '#define EFFECT_KEY 3', '#define EFFECT_ITEM 4', '#define EFFECT_PIECE 5', f'#define PICKUP_COUNT {max(1, len(pickup_gid))}',
            '#define PAL_BLOCK 3', '']
    cobj, cpal, ctables = convert_combat(exp)
    open(os.path.join(out, 'combat.chr'), 'wb').write(cobj)
    open(os.path.join(out, 'combat.pal'), 'wb').write(cpal)
    asm += ['.section ".combat" superfree', 'combat_chr:', '.incbin "data/combat.chr"', 'combat_chrend:',
            'combat_pal:', '.incbin "data/combat.pal"', '.ends', '']
    import shutil
    shutil.copy(os.path.join(exp, '..', 'items.chr'), os.path.join(out, 'items.chr'))
    shutil.copy(os.path.join(exp, '..', 'block.pal'), os.path.join(out, 'block.pal'))
    asm += ['.section ".items" superfree', 'items_chr:', '.incbin "data/items.chr"', 'items_chrend:',
            'block_pal:', '.incbin "data/block.pal"', '.ends', '']
    hdr += ['extern char items_chr, items_chrend, block_pal;']
    hdr += ['extern char combat_chr, combat_chrend, combat_pal;', ''] + ctables + ['']
    font = convert_font(os.path.join(exp, 'font.png'))
    open(os.path.join(out, 'font.chr'), 'wb').write(font)
    asm += ['.section ".font" superfree', 'font_chr:', '.incbin "data/font.chr"', 'font_chrend:', '.ends', '']
    hdr += ['', 'extern char hero_chr, hero_chrend, hero_pal, hero_palend, font_chr, font_chrend;', '',
            f'#define WLD_GRID {WLD_GRID}', f'#define WLD_NNPC {WLD_NNPC}', f'#define WLD_NPCS {WLD_NPCS}',
            f'#define WLD_TOFS {WLD_TOFS}', f'#define MAX_NPCS {MAX_NPCS}', f'#define NPC_REC {NPC_REC}', f'#define BOX_COLS {BOX_COLS}',
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
