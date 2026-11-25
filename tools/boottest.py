#!/usr/bin/env python3
"""Boot the ROM in the snes9x core (via stable-retro), walk south from the start in the Fan
into Hollywood Cemetery, and check the screen at each step. Saves screenshots to <out-dir>.
Usage: boottest.py RiverCityQuest.sfc <out-dir>"""
import os, sys
import numpy as np
import stable_retro as retro
from PIL import Image

ORDER = ['B', 'Y', 'SELECT', 'START', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'X', 'L', 'R']
rom, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
emu = retro.RetroEmulator(rom)


def run(button, frames):
    mask = np.zeros(12, dtype=np.uint8)
    if button:
        mask[ORDER.index(button)] = 1
    for _ in range(frames):
        emu.set_button_mask(mask, 0)
        emu.step()
    return emu.get_screen().copy()


ok = True


def check(cond, msg):
    global ok
    print(('ok   ' if cond else 'FAIL ') + msg)
    ok &= bool(cond)


start = run(None, 120)
Image.fromarray(start).save(f'{out}/1-the-fan.png')
check(start.std() > 10, 'boots to a drawn map, not a blank screen')

walked = run('DOWN', 60)
check(np.abs(walked.astype(int) - start).mean() > 1, 'walking down scrolls the view')

south = run('DOWN', 400)
south = run(None, 40)
Image.fromarray(south).save(f'{out}/2-hollywood-cemetery.png')
# Hollywood Cemetery is mostly grass: far more green than the Fan's street grid.
green = lambda im: float(((im[..., 1] > im[..., 0] + 30) & (im[..., 1] > im[..., 2] + 30)).mean())
check(green(south) > green(start) + 0.15, f'walked into the next neighborhood (green {green(start):.2f} -> {green(south):.2f})')
sys.exit(0 if ok else 1)
