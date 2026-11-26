#!/usr/bin/env python3
"""Play the ROM in the snes9x core and check each stage still works. Saves screenshots.
Usage: boottest.py RiverCityQuest.sfc RiverCityQuest.sym <out-dir>"""
import os, sys
import numpy as np
from sneslab import Lab

rom, sym, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ok = True


def check(cond, msg):
    global ok
    print(('ok   ' if cond else 'FAIL ') + msg)
    ok &= bool(cond)


def screen():
    return lab.emu.get_screen().astype(int)


# Stage 1: boot, walk, change neighborhood.
lab = Lab(rom, sym, root)
lab.run(None, 120)
check(screen().std() > 10, 'boots to a drawn map')
check(lab.map == 11, f'starts in the Fan (map {lab.map})')
lab.screenshot(f'{out}/1-the-fan.png')

# Stage 2: read the Arthur Ashe marker (cell 7,10) from the path above it.
lab.walk_to_cell(7, 9)
lab.face('down')
before = screen()
lab.press('A', 70)
check(lab.ram('boxState') != 0, 'A on a marker opens the text box')
check(np.abs(screen() - before).mean() > 3, 'the box is drawn on screen')
lab.screenshot(f'{out}/2-reading.png')
for _ in range(12):
    if lab.ram('boxState') == 0:
        break
    lab.press('A', 4)
check(lab.ram('boxState') == 0, 'A pages through and closes the box')

# Stage 2: talk to the Runner (cell 26,6) from below; he should turn to face us.
lab.walk_to_cell(26, 7)
lab.face('up')
lab.press('A', 60)
check(lab.ram('boxState') != 0, 'A next to a person starts a conversation')
check(lab.ram('npcDir', 1, index=1) == 0, 'the person turns to face the hero')
lab.screenshot(f'{out}/3-talking.png')
while lab.ram('boxState'):
    lab.press('A', 4)

# Stage 3: the HUD, a fight, and the spin attack.
check(lab.ram('hpMax') == 6 and 0 < lab.ram('hp') <= 6, f"three hearts in the HUD ({lab.ram('hp')}/6 health after the walk)")
alive = [k for k in range(lab.ram('enCount')) if lab.enemy(k)['alive']]
check(len(alive) > 0, f'enemies are out in the Fan ({len(alive)})')
target = min(alive, key=lambda k: abs(lab.enemy(k)['x'] - lab.pos[0]) + abs(lab.enemy(k)['y'] - lab.pos[1]))
used = lab.fight(target)
check(used is not None, f'the paddle beats a {["rat","squirrel","goose","scooter","crab","bat","ghost"][lab.enemy(target)["type"]]} ({used} frames)')
lab.screenshot(f'{out}/4-fight.png')
lab.run('A', 1); lab.run('A', 70)
check(lab.ram('charging') == 1 and lab.ram('charge') >= 40, 'holding A charges a spin')
lab.run(None, 3)
check(lab.ram('spin') > 0, 'letting go spins')
lab.screenshot(f'{out}/5-spin.png')
lab.run(None, 30)

# Stage 4: cut the bush hiding the Fan's chest, open it.
def close_box():
    for _ in range(15):
        if not lab.ram('boxState'):
            return
        lab.press('A', 6)

lab.walk_to_cell(3, 20)
lab.face('left')
lab.press('A', 20)
check(lab.ram('colRam', 1, False, 20 * 32 + 2) == 0, 'the paddle cuts a bush')
lab.run('LEFT', 12)
lab.run(None, 2)
lab.face('left')
coins = lab.ram('coins', 2)
lab.press('A', 60)
check(lab.ram('coins', 2) == min(999, coins + 50), f'the hidden chest holds 50 coins ({coins} -> {lab.ram("coins", 2)})')
lab.screenshot(f'{out}/6-chest.png')
close_box()

# Stage 4: in and out of the row house.
lab.walk_to_cell(4, 17)
lab.run('UP', 30)
lab.run(None, 40)
check(lab.map == 35, f'the door leads into your row house (map {lab.map})')
lab.screenshot(f'{out}/7-row-house.png')
lab.walk_to_cell(7, 8)
lab.run('DOWN', 40)
lab.run(None, 40)
check(lab.map == 11 and lab.pos[1] // 16 == 17, f'the stairs lead back out to the Fan (map {lab.map}, {lab.pos})')

# Stage 4: Carytown Goods. Buy a heart charm.
lab.walk_to_cell(0, 11)
lab.run('LEFT', 30)
lab.run(None, 40)
check(lab.map == 10, f'west into Carytown (map {lab.map})')
lab.walk_to_cell(6, 17)
lab.run('UP', 30)
lab.run(None, 40)
check(lab.map == 38, f'into Carytown Goods (map {lab.map})')
lab.walk_to_cell(8, 5)
lab.face('up')
lab.press('A', 10)
check(lab.ram('boxState') == 4, 'talking across the counter opens the shop')
for _ in range(8):
    lab.press('DOWN', 3)
lab.screenshot(f'{out}/8-shop.png')
coins = lab.ram('coins', 2)
lab.press('A', 60)
check(lab.ram('hpMax') == 8 and lab.ram('coins', 2) == coins - 15, f'a heart charm for 15 coins (hearts {lab.ram("hpMax") // 2}, coins {lab.ram("coins", 2)})')
lab.press('A', 10)
lab.press('B', 10)
check(lab.ram('boxState') == 0, 'B leaves the shop')
lab.walk_to_cell(7, 8)
lab.run('DOWN', 40)
lab.run(None, 40)
lab.walk_to_cell(31, 11)
lab.run('RIGHT', 30)
lab.run(None, 40)
check(lab.map == 11, f'back east to the Fan (map {lab.map})')

# Stage 1: walk off the south edge into Hollywood Cemetery.
lab.walk_to_cell(16, 21)
lab.run('DOWN', 60)
lab.run(None, 40)
check(lab.map == 17, f'walked south into Hollywood Cemetery (map {lab.map})')
lab.screenshot(f'{out}/9-hollywood-cemetery.png')

# Stage 3: let a ghost take the last half-heart; wake in Capitol Square.
lab.poke('hp', 1)
for _ in range(1500):
    if lab.ram('dying'):
        break
    lab.run(None, 1)
check(lab.ram('dying') == 1, 'a ghost takes the last heart')
lab.run(None, 60)
lab.screenshot(f'{out}/10-fog-takes-you.png')
for _ in range(10):
    if not lab.ram('boxState'):
        break
    lab.press('A', 6)
lab.run(None, 60)
check(lab.map == 12 and lab.pos == (120, 136), f'wakes by the fountain in Capitol Square (map {lab.map}, {lab.pos})')
check(lab.ram('hp') == lab.ram('hpMax'), 'with full hearts')
sys.exit(0 if ok else 1)
