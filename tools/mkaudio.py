#!/usr/bin/env python3
"""Music and sound effects for the SNES: Impulse Tracker modules for SNESMOD (smconv).

The web game sequences its own chiptunes (js/music.js): each song is a lead line on a
square wave, a bass line on a triangle and a drum line (k kick, s snare, h hat), one token
per eighth note ('-' holds, '.' rests). Here each song becomes a 3-channel .it module with
looped single-cycle square and triangle samples and one-shot drum samples, one row per
eighth note (speed 12, tempo = bpm).

Sound effects are the web game's sfx()/tune() calls rendered into short samples, all in one
effects module (SNESMOD plays them with spcEffect; module 0 must be the effects).

usage: mkaudio.py build/export/music.json data/audio
writes data/audio/effects.it, one <song>.it per song, and data/audio/sfx.h
"""
import json
import math
import os
import random
import struct
import sys

SONG_ORDER = ['city', 'south', 'dungeon', 'boss', 'forge', 'win', 'title', 'night']

# ---------------------------------------------------------------- waveforms

CYCLE = 32          # samples in one cycle of the looped tone samples
C4 = 261.6256       # web C4 plays as IT C-5 (note 60)


def square_cycle():
    return [96 if i < CYCLE // 2 else -96 for i in range(CYCLE)]


def triangle_cycle():
    out = []
    for i in range(CYCLE):
        ph = i / CYCLE
        v = 4 * ph - 1 if ph < .5 else 3 - 4 * ph
        out.append(round(v * 120))
    return out


def noise(n, seed):
    r = random.Random(seed)
    return [r.uniform(-1, 1) for _ in range(n)]


def kick(rate):
    """web drum('k'): a sine falling 130 -> 40 Hz over .12 s, decaying over .14 s"""
    n = int(rate * .15)
    out, ph = [], 0.0
    for i in range(n):
        t = i / rate
        f = 130 * (40 / 130) ** min(1, t / .12)
        ph += 2 * math.pi * f / rate
        out.append(math.sin(ph) * (.001 / 1) ** min(1, t / .14))
    return [round(v * 120) for v in out]


def filtered_noise(rate, dur, hp, bp, seed):
    """web drum('s'/'h'): noise through a band-pass (snare) or high-pass (hat), decaying"""
    n = int(rate * dur)
    src = noise(n, seed)
    out, lp, prev = [], 0.0, 0.0
    for i, w in enumerate(src):
        if hp:
            y = w - prev          # crude high-pass
            prev = w
        else:
            lp = lp * .6 + w * .4  # crude band-pass: low-pass, then take off the lows
            y = lp - prev * .5
            prev = lp
        out.append(y * (.001) ** (i / n))
    peak = max(abs(v) for v in out) or 1
    return [round(v / peak * 110) for v in out]


def pad16(data, fill=0):
    while len(data) % 16:
        data.append(fill)
    return data

# ---------------------------------------------------------------- sound effects


def osc(kind, f, t):
    ph = (f * t) % 1
    if kind == 'square':
        return 1 if ph < .5 else -1
    if kind == 'triangle':
        return 4 * ph - 1 if ph < .5 else 3 - 4 * ph
    if kind == 'sawtooth':
        return 2 * ph - 1
    return math.sin(2 * math.pi * ph)


def render_sfx(parts, rate):
    """parts: list of (start s, freq Hz or (f0, f1), dur s, wave, gain); web sfx() decays exponentially"""
    end = max(s + d for s, f, d, w, g in parts)
    n = int(end * rate) + 1
    buf = [0.0] * n
    for s, f, d, w, g in parts:
        i0 = int(s * rate)
        for i in range(int(d * rate)):
            t = i / rate
            ff = f if not isinstance(f, tuple) else f[0] * (f[1] / f[0]) ** (t / d)
            env = (.0001 / g) ** (t / d) if g > 0 else 0
            if i0 + i < n:
                buf[i0 + i] += osc(w, ff, t) * g * env
    peak = max(abs(v) for v in buf) or 1
    return pad16([round(v / peak * 112) for v in buf])


def tune(seq, wave='square', step=1.0):
    """web tune(): notes one after another, each decaying over 95% of its length"""
    parts, t = [], 0.0
    for f, d in seq:
        if f:
            parts.append((t, f, d * .95, wave, .045))
        t += d
    return parts


def effects(music):
    J, S, F = music['jing'], music['secret'], music['fanfare']
    # name, parts, playback rate (Hz, a multiple of 4000), volume 0..15
    return [
        ('SWING', [(0, (1200, 700), .08, 'triangle', .04)], 16000, 9),
        ('HIT', [(0, 1500, .05, 'square', .03)], 16000, 10),
        ('CLANK', [(0, 1100, .1, 'square', .05), (0, 1650, .06, 'square', .02)], 16000, 10),
        ('HURT', [(0, 140, .18, 'sawtooth', .04)], 8000, 13),
        ('KILL', [(0, 90, .2, 'sawtooth', .06)], 8000, 12),
        ('COIN', [(0, 1200, .05, 'square', .04), (.05, 1600, .08, 'square', .04)], 16000, 9),
        ('HEART', [(0, 800, .1, 'triangle', .04)], 16000, 11),
        ('TALK', [(0, 700, .03, 'square', .04)], 16000, 7),
        ('MENU', [(0, 600, .02, 'square', .04)], 16000, 7),
        ('FUSE', [(0, 300, .05, 'square', .04)], 8000, 9),
        ('BOOM', [(0, (60, 40), .4, 'sawtooth', .1), (0, 110, .3, 'square', .06)], 8000, 14),
        ('JING', tune(J), 8000, 11),
        ('SECRET', tune(S), 8000, 11),
        ('SHOT', [(0, 520, .06, 'square', .04)], 16000, 8),
        ('SLAM', [(0, (80, 50), .4, 'sawtooth', .08)], 8000, 14),
        ('FANFARE', tune(F, 'triangle'), 8000, 12),
        ('HORN', [(0, 440, .12, 'square', .03)], 8000, 9),        # a car stopping for you (web updLife)
        ('FLAP', [(0, 1300, .04, 'square', .01)], 16000, 6),      # a pigeon taking off
        ('CHAL', [(0, 988, .06, 'triangle', .04)], 16000, 10),    # a challenge token, lamp or mural (web chalProgress)
        ('GO', tune([[523, .08], [784, .08], [1047, .16]], 'square'), 8000, 11),   # a challenge starts
        ('FAIL', [(0, 150, .3, 'sawtooth', .04)], 8000, 12),      # time's up
    ]

# ---------------------------------------------------------------- IT writer

NOTE_CUT = 254


def parse_note(tok):
    names = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
    k = names[tok[0]]
    rest = tok[1:]
    if rest[0] == '#':
        k += 1
        rest = rest[1:]
    elif rest[0] == 'b':
        k -= 1
        rest = rest[1:]
    return 12 * (int(rest) + 1) + k      # web C4 -> IT C-5 (60)


def toks(s):
    return [t for t in s.replace('|', ' ').split() if t]


class Sample:
    def __init__(self, name, data, c5speed, vol, loop=False):
        self.name, self.data, self.c5, self.vol, self.loop = name, data, c5speed, vol, loop


def write_it(path, title, samples, patterns, speed, tempo, channels):
    """samples: [Sample]; patterns: list of rows, each a list of per-channel (note, inst, vol) or None"""
    n_ord = len(patterns) + 1
    n_ins = n_smp = len(samples)
    n_pat = len(patterns)
    hdr = bytearray()
    hdr += b'IMPM' + title.encode()[:25].ljust(26, b'\0')
    hdr += struct.pack('<HHHHHHHHH', 0x0410, n_ord, n_ins, n_smp, n_pat, 0x0214, 0x0214, 1 | 4 | 8, 0)
    hdr += struct.pack('<BBBBBBHII', 128, 48, speed, tempo, 128, 0, 0, 0, 0)
    hdr += bytes([32 if i < channels else 32 | 128 for i in range(64)])
    hdr += bytes([64] * 64)
    assert len(hdr) == 0xC0
    orders = bytes(list(range(n_pat)) + [255])
    table_at = 0xC0 + n_ord
    body_at = table_at + 4 * (n_ins + n_smp + n_pat)
    ins_blobs, smp_hdrs, smp_data, pat_blobs = [], [], [], []
    for i, s in enumerate(samples):
        b = bytearray(b'IMPI' + b'\0' * 12 + b'\0')
        b += bytes([0, 0, 0]) + struct.pack('<H', 0) + bytes([0, 0, 128, 32 | 128, 0, 0])
        b += struct.pack('<H', 0x0214) + bytes([1, 0]) + b'\0' * 26 + bytes([0, 0, 0, 0]) + struct.pack('<H', 0)
        for n in range(120):
            b += bytes([n, i + 1])
        b += b'\0' * (82 * 3)
        b += b'\0' * (554 - len(b))
        ins_blobs.append(bytes(b))
    for p in patterns:
        packed = bytearray()
        for row in p:
            for ch, ev in enumerate(row):
                if ev is None:
                    continue
                note, inst, vol = ev
                mask = 1 | (2 if inst else 0) | (4 if vol is not None else 0)
                packed += bytes([(ch + 1) | 0x80, mask, note])
                if inst:
                    packed += bytes([inst])
                if vol is not None:
                    packed += bytes([vol])
            packed += b'\0'
        pat_blobs.append(struct.pack('<HH', len(packed), len(p)) + b'\0' * 4 + bytes(packed))
    off = body_at
    ins_off = []
    for b in ins_blobs:
        ins_off.append(off)
        off += len(b)
    smp_off = []
    for s in samples:
        smp_off.append(off)
        off += 0x50
    data_off = []
    for s in samples:
        data_off.append(off)
        off += len(s.data)
    pat_off = []
    for b in pat_blobs:
        pat_off.append(off)
        off += len(b)
    out = bytearray(hdr + orders)
    out += b''.join(struct.pack('<I', o) for o in ins_off + smp_off + pat_off)
    for b in ins_blobs:
        out += b
    for i, s in enumerate(samples):
        flags = 1 | (16 if s.loop else 0)
        h = bytearray(b'IMPS' + b'\0' * 12 + b'\0')
        h += bytes([64, flags, s.vol]) + s.name.encode()[:25].ljust(26, b'\0') + bytes([1, 32])
        h += struct.pack('<IIIIIII', len(s.data), 0, len(s.data) if s.loop else 0, s.c5, 0, 0, data_off[i])
        h += bytes([0, 0, 0, 0])
        assert len(h) == 0x50
        out += h
    for s in samples:
        out += bytes((v + 256) % 256 for v in s.data)
    for b in pat_blobs:
        out += b
    assert len(out) == off
    open(path, 'wb').write(out)


def song_module(name, song, path):
    L, B, D = toks(song['lead']), toks(song['bass']), toks(song['drum'])
    n = max(len(L), len(B))
    rate = 16000
    lead_tri = song['lt'] == 'triangle'
    # web gains: square lead .022, triangle lead (night) .05, bass .07, kick .12, snare .05, hat .018
    lead_vol = 34 if lead_tri else 22
    samples = [
        Sample('lead', (triangle_cycle() if lead_tri else square_cycle()), round(CYCLE * C4), lead_vol, loop=True),
        Sample('bass', triangle_cycle(), round(CYCLE * C4), 48, loop=True),
        Sample('kick', pad16(kick(rate)), rate, 64),
        Sample('snare', pad16(filtered_noise(rate, .12, False, 1800, 1)), rate, 30),
        Sample('hat', pad16(filtered_noise(rate, .04, True, 7000, 2)), rate, 14),
    ]
    rows = []
    for i in range(n):
        row = [None, None, None]
        for ch, line, inst in ((0, L, 1), (1, B, 2)):
            t = line[i % len(line)] if line else '.'
            if t == '.':
                prev = line[(i - 1) % len(line)] if line else '.'
                row[ch] = (NOTE_CUT, 0, None) if prev != '.' else None
            elif t != '-':
                row[ch] = (parse_note(t), inst, None)
        if D:
            d = D[i % len(D)]
            if d in 'ksh':
                row[2] = (60, {'k': 3, 's': 4, 'h': 5}[d], None)
        rows.append(row)
    patterns = [rows[i:i + 64] for i in range(0, len(rows), 64)]
    write_it(path, name, samples, patterns, 12, song['bpm'], 3)


def main():
    music = json.load(open(sys.argv[1]))
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    fx = effects(music)
    samples = [Sample(name.lower(), render_sfx(parts, rate), rate, 64) for name, parts, rate, vol in fx]
    # an effects module still needs a pattern
    write_it(os.path.join(out, 'effects.it'), 'effects', samples, [[[None]]], 6, 125, 1)
    for name in SONG_ORDER:
        song_module(name, music['songs'][name], os.path.join(out, name + '.it'))
    h = ['/* generated by tools/mkaudio.py: sound effects for spcEffect, and the songs */',
         '#ifndef RCQ_SFX_H', '#define RCQ_SFX_H']
    for i, (name, parts, rate, vol) in enumerate(fx):
        h.append(f'#define SFX_{name} {i}')
    h.append(f'#define SFX_COUNT {len(fx)}')
    h.append('static const u8 sfxPitch[%d] = {%s}; /* 1 = 4 kHz */' % (len(fx), ', '.join(str(r // 4000) for _, _, r, _ in fx)))
    h.append('static const u8 sfxVol[%d] = {%s};' % (len(fx), ', '.join(str(v) for *_, v in fx)))
    for i, name in enumerate(SONG_ORDER):
        h.append(f'#define SONG_{name.upper()} {i + 1}   /* module number: 0 is the effects */')
    h.append('#endif')
    open(os.path.join(out, 'sfx.h'), 'w').write('\n'.join(h) + '\n')
    for f in sorted(os.listdir(out)):
        print(f, os.path.getsize(os.path.join(out, f)))


if __name__ == '__main__':
    main()
