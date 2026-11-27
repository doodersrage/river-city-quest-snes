#!/usr/bin/env python3
"""Play the ROM in the snes9x core and check each stage still works. Saves screenshots.
Usage: boottest.py RiverCityQuest.sfc RiverCityQuest.sym <out-dir>"""
import os, sys
import numpy as np
from sneslab import Lab, travel, _neighbours

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


def hero_name():
    return bytes(lab.ram('heroName', 1, index=i) for i in range(9)).split(b'\0')[0].decode('ascii', 'replace')


# Stage 11: the title screen, a new game in slot 1, and naming the hero.
lab = Lab(rom, sym, root)
lab.run(None, 150)
check(screen().std() > 10, 'boots to the title screen')
check(lab.ram('saveSlot') == 0 and lab.ram('curSong') == 7, f"no slot chosen yet, the title song plays ({lab.ram('curSong')})")
lab.screenshot(f'{out}/0-title.png')
lab.press('A', 60)                       # slot 1 is empty: name entry
for b in 'BBBB':                         # RORY out
    lab.press(b, 8)
for b in ['A', 'RIGHT', 'RIGHT', 'RIGHT', 'A', 'LEFT', 'LEFT', 'LEFT', 'A']:   # A, D, A
    lab.press(b, 8)
lab.screenshot(f'{out}/0-name.png')
lab.press('START', 120)
check(hero_name() == 'Ada', f'the hero is named ({hero_name()!r})')
check(lab.ram('saveSlot') == 1, 'the new game is in slot 1')
check(lab.ram('boxState') != 0, 'the opening is read')
lab.run(None, 200)
lab.screenshot(f'{out}/0-intro.png')
for _ in range(40):
    if not lab.ram('boxState'):
        break
    lab.press('A', 6)
check(lab.ram('boxState') == 0, 'the opening pages through')

# Stage 1: boot, walk, change neighborhood.
lab.run(None, 30)
check(screen().std() > 10, 'boots to a drawn map')
SONG = {1: 'town', 2: 'south', 3: 'dungeon', 4: 'boss', 5: 'forge', 6: 'win', 8: 'night'}
def song():
    return SONG.get(lab.ram('curSong'), lab.ram('curSong'))
check(song() == 'town', f'the town theme plays ({song()})')
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
used = None
for target in sorted(alive, key=lambda k: abs(lab.enemy(k)['x'] - lab.pos[0]) + abs(lab.enemy(k)['y'] - lab.pos[1])):
    used = lab.fight(target)     # (one that dodges somewhere we can't reach: try the next)
    if used is not None: break
check(used is not None, f'the paddle beats a {["rat","squirrel","goose","scooter","crab","bat","ghost"][lab.enemy(target)["type"]]} ({used} frames)')
lab.screenshot(f'{out}/4-fight.png')
for _ in range(4):                     # a rat bumping you cancels the charge: try again
    lab.run('A', 1); lab.run('A', 70)
    if lab.ram('charging') == 1 and lab.ram('charge') >= 40: break
    lab.run(None, 30)
check(lab.ram('charging') == 1 and lab.ram('charge') >= 40, 'holding A charges a spin')
lab.run(None, 3)
check(lab.ram('spin') > 0, 'letting go spins')
lab.screenshot(f'{out}/5-spin.png')
lab.run(None, 30)

# Stage 4: cut the bush hiding the Fan's chest, open it.
def close_box():
    for _ in range(60):
        if not lab.ram('boxState'):
            return
        # a question (stage 10's challenge offers): B says "not now"
        lab.press('B' if lab.ram('boxState') in (6, 8) else 'A', 6)   # a question or the card list: B backs out

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
check(lab.map == lab.map_named('Your Row House'), f'the door leads into your row house (map {lab.map})')
lab.screenshot(f'{out}/7-row-house.png')
lab.walk_to_cell(7, 8)
lab.run('DOWN', 40)
lab.run(None, 40)
check(lab.map == 11 and abs(lab.pos[0] - 72) < 24 and abs(lab.pos[1] - 280) < 24, f'the stairs lead back out to the Fan, by the door (map {lab.map}, {lab.pos})')  # a rat may bump you

# Stage 4: Carytown Goods. Buy a heart charm.
lab.walk_to_cell(0, 11)
lab.run('LEFT', 30)
lab.run(None, 40)
check(lab.map == 10, f'west into Carytown (map {lab.map})')
lab.walk_to_cell(6, 17)
lab.run('UP', 30)
lab.run(None, 40)
check(lab.map == lab.map_named('Carytown Goods'), f'into Carytown Goods (map {lab.map})')
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

# Stage 5: Tredegar Iron Works, start to finish.
def clear_room():
    start = lab.map
    missed = set()
    for _ in range(24):
        alive = [k for k in range(lab.ram('enCount')) if lab.enemy(k)['alive']]
        if not alive:
            break
        tries = [k for k in alive if k not in missed]
        if not tries:
            # everything left is out of reach (a bat over a pit): give it time to drift back
            missed.clear()
            lab.run(None, 90)
            continue
        x, y = lab.pos
        k = min(tries, key=lambda k: abs(lab.enemy(k)['x'] - x) + abs(lab.enemy(k)['y'] - y))
        m0 = lab.map
        if lab.fight(k, 600) is None:
            missed.add(k)
        if lab.map != m0:
            # knocked out of the room: step straight back in
            nb = _neighbours(root)
            back = next((k2 for d, k2 in (('N', 'UP'), ('S', 'DOWN'), ('W', 'LEFT'), ('E', 'RIGHT')) if nb[d][lab.map] == m0), 'UP')  # out the exit: back up through the door
            for _ in range(3):
                close_box(); lab.run(back, 30); lab.run(None, 40)
                if lab.map == m0: break
        if lab.ram('hp') <= 2:
            lab.poke('hp', lab.ram('hpMax'))  # this test checks the dungeon, not our dodging
    return lab.map == start and not any(lab.enemy(k)['alive'] for k in range(lab.ram('enCount')))

lab.walk_to_cell(28, 17)
prev=None
for f in range(120):
    lab.run('UP' if f<20 else None, 1)
    v=(lab.map, lab.ram('dentMap'), lab.ram('roomArea'), lab.ram('curMap'))
    if v!=prev: print(f, v, lab.pos); prev=v
