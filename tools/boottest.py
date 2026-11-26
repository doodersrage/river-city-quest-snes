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
    for _ in range(60):
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
check(lab.map == lab.map_named('Your Row House'), f'the door leads into your row house (map {lab.map})')
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
    for _ in range(12):
        alive = [k for k in range(lab.ram('enCount')) if lab.enemy(k)['alive']]
        if not alive:
            break
        x, y = lab.pos
        lab.fight(min(alive, key=lambda k: abs(lab.enemy(k)['x'] - x) + abs(lab.enemy(k)['y'] - y)), 900)
        if lab.ram('hp') <= 2:
            lab.poke('hp', lab.ram('hpMax'))  # this test checks the dungeon, not our dodging
    return not any(lab.enemy(k)['alive'] for k in range(lab.ram('enCount')))

lab.walk_to_cell(28, 17)
lab.run('UP', 20)
lab.run(None, 70)
check(lab.map == lab.map_named('Foundry Gate') and lab.ram('dentMap') == lab.map, 'the Tredegar door leads into the Foundry Gate')
check(clear_room() and lab.ram('roomFlags', 1, False, lab.map) & 1, 'the Foundry Gate is cleared')
lab.walk_to_cell(14, 5)
lab.run('RIGHT', 40)
lab.run(None, 40)
check(lab.map == lab.map_named('Pattern Shop'), 'east into the Pattern Shop')
check(clear_room(), 'the Pattern Shop is cleared')
lab.walk_to_cell(4, 4)
lab.face('up')
lab.press('A', 60)
check(lab.ram('hasHook') == 1, 'a chest appears with the Grappling Hook')
lab.screenshot(f'{out}/10-grappling-hook.png')
close_box()
lab.walk_to_cell(12, 6)
lab.face('up')
lab.press('B', 50)
check(lab.pos == (200, 56), f'the hook pulls you across the pit to the post ({lab.pos})')
lab.walk_to_cell(11, 3)
lab.face('up')
lab.press('A', 60)
close_box()
area = lab.ram('roomArea')
check(lab.ram('keys', 1, False, area) == 1, 'a small key')
lab.walk_to_cell(12, 3)
lab.face('down')
lab.press('B', 50)
lab.walk_to_cell(1, 5)
lab.run('LEFT', 40)
lab.run(None, 40)
lab.walk_to_cell(7, 1)
lab.run('UP', 10)
check(lab.ram('keys', 1, False, area) == 0 and lab.ram('colRam', 1, False, 7) == 0, 'the key opens the locked door')
lab.run('UP', 40)
lab.run(None, 40)
check(lab.map == lab.map_named('Casting Floor'), 'north into the Casting Floor')
check(clear_room() and lab.ram('colRam', 1, False, 7) == 0, 'clearing it opens the gate')
lab.walk_to_cell(7, 7)
lab.face('up')
lab.press('B', 50)
lab.walk_to_cell(7, 1)
lab.run('UP', 40)
lab.run(None, 60)
check(lab.map == lab.map_named('Furnace Hall') and lab.ram('boxState'), 'the Iron Foreman wakes up')
close_box()
lab.screenshot(f'{out}/11-iron-foreman.png')
clanked = stunned = False
for _ in range(6000):
    if lab.ram('boxState'):
        break
    if not lab.ram('bossOn'):
        break
    if lab.ram('hp') <= 2:
        lab.poke('hp', lab.ram('hpMax'))
    bx, by = lab.ram('bossX', 2, True) >> 4, lab.ram('bossY', 2, True) >> 4
    x, y = lab.pos
    dx, dy = bx - x, (by - 8) - y
    if lab.ram('bossState') == 2:
        stunned = True
        if abs(dx) + abs(dy) < 22 and (abs(dx) < 10 or abs(dy) < 12):
            lab.run(('RIGHT' if dx > 0 else 'LEFT') if abs(dx) >= abs(dy) else ('DOWN' if dy > 0 else 'UP'), 1)
            lab.press('A', 12)
        else:
            lab.run(('RIGHT' if dx > 0 else 'LEFT') if abs(dx) > abs(dy) else ('DOWN' if dy > 0 else 'UP'), 1)
    else:
        # hook him from the side, keeping both of you in the middle of the hall
        want_x = bx - 48 if bx > 128 else bx + 48
        want_y = by - 8
        if abs(y - want_y) > 4:
            lab.run('DOWN' if want_y > y else 'UP', 1)
        elif abs(x - want_x) > 6:
            lab.run('RIGHT' if want_x > x else 'LEFT', 1)
        else:
            lab.run('RIGHT' if bx > x else 'LEFT', 1)
            lab.press('B', 3)
check(stunned, 'the hook rips the Foreman\'s chest plate loose')
lab.run(None, 30)
lab.screenshot(f'{out}/12-foreman-beaten.png')
check(not lab.ram('bossOn') and lab.ram('roomFlags', 1, False, lab.map) & 2, 'the Iron Foreman is beaten')
close_box()
hpmax = lab.ram('hpMax')
try:
    lab.walk_to_cell(9, 3)      # the container is at (144, 80); walking past it picks it up
except RuntimeError:
    pass
close_box()
if lab.ram('hpMax') == hpmax:
    lab.walk_to_cell(9, 3)
    for _ in range(40):
        if lab.ram('boxState'):
            break
        lab.run('DOWN', 1)
    lab.run(None, 30)
check(lab.ram('hpMax') == hpmax + 2, 'he leaves a heart container')
close_box()
lab.walk_to_cell(7, 7)
lab.run('DOWN', 30)
lab.run(None, 60)
check(lab.map == 17 and lab.ram('dentMap') == 255, 'the stairs lead back out to Hollywood Cemetery')

# Stage 6: the Belle Isle Quarry, Blasting Powder and Old Snapper.
lab.walk_to_cell(31, 10)
lab.run('RIGHT', 30)
lab.run(None, 40)
lab.walk_to_cell(19, 6)
lab.run('UP', 20)
lab.run(None, 70)
check(lab.map == lab.map_named('Quarry Floor'), 'east to Belle Isle and down into the quarry')
check(clear_room(), 'the Quarry Floor is cleared')
lab.walk_to_cell(12, 4)
lab.face('up')
lab.press('A', 60)
close_box()
lab.walk_to_cell(7, 1)
lab.run('UP', 10)
lab.run('UP', 40)
lab.run(None, 40)
check(lab.map == lab.map_named('Powder Store'), 'its key opens the way north to the Powder Store')
check(clear_room(), 'the Powder Store is cleared')
lab.walk_to_cell(3, 3)
lab.face('up')
lab.press('A', 60)
check(lab.ram('hasPowder') == 1 and lab.ram('powder') == 8 and lab.ram('eqItem') == 2, 'Blasting Powder, 8 kegs, ready on B')
lab.screenshot(f'{out}/13-blasting-powder.png')
close_box()
lab.walk_to_cell(13, 5)
lab.face('right')
lab.press('B', 2)
check(lab.ram('powder') == 7 and lab.ram('bombOn'), 'B sets down a keg')
lab.walk_to_cell(9, 5)
lab.run(None, 90)
check(all(lab.ram('colRam', 1, False, y * 32 + 15) == 0 for y in (4, 5, 6)), 'the keg blows the cracked wall open')
lab.walk_to_cell(14, 5)
lab.run('RIGHT', 30)
lab.run(None, 60)
check(lab.map == lab.map_named('Quarry Pond') and lab.ram('boxState'), 'Old Snapper stirs in the Quarry Pond')
close_box()
stunned = False
for _ in range(8000):
    if lab.ram('boxState') or not lab.ram('bossOn'):
        break
    if lab.ram('hp') <= 2:
        lab.poke('hp', lab.ram('hpMax'))
    bx, by = lab.ram('bossX', 2, True) >> 4, lab.ram('bossY', 2, True) >> 4
    st = lab.ram('bossState')
    x, y = lab.pos
    dx, dy = bx - x, (by - 6) - y
    if st == 2:            # stunned against the wall: paddle him
        stunned = True
        if abs(dx) + abs(dy) < 24 and (abs(dx) < 12 or abs(dy) < 12):
            lab.run(('RIGHT' if dx > 0 else 'LEFT') if abs(dx) >= abs(dy) else ('DOWN' if dy > 0 else 'UP'), 1)
            lab.press('A', 10)
        else:
            lab.run(('RIGHT' if dx > 0 else 'LEFT') if abs(dx) > abs(dy) else ('DOWN' if dy > 0 else 'UP'), 1)
    elif st in (1, 3):     # winding up or charging: step out of his line
        lab.run('UP' if y > 88 else 'DOWN', 1)
    else:                  # wait on his row with a wall behind you
        want_x = bx - 56 if bx > 128 else bx + 56
        if abs(y - by) > 3:
            lab.run('DOWN' if by > y else 'UP', 1)
        elif abs(x - want_x) > 4:
            lab.run('RIGHT' if want_x > x else 'LEFT', 1)
        else:
            lab.run(None, 1)
check(stunned, 'Old Snapper charges into the wall and stuns himself')
check(not lab.ram('bossOn') and lab.ram('roomFlags', 1, False, lab.map) & 2, 'Old Snapper is beaten')
lab.run(None, 30)
lab.screenshot(f'{out}/14-old-snapper-beaten.png')
close_box()
hpmax = lab.ram('hpMax')
for c in [(7, 4), (9, 4)]:
    try:
        lab.walk_to_cell(*c)
    except RuntimeError:
        pass
    close_box()
    lab.run('DOWN', 4)
    lab.run(None, 20)
    close_box()
check(lab.ram('keystones') == 1, 'the Granite Keystone (1 of 4)')
check(lab.ram('hpMax') == hpmax + 2, 'and a heart container')
lab.walk_to_cell(7, 7)
lab.run('DOWN', 30)
lab.run(None, 60)
check(lab.map == 18, 'the stairs lead back up to Belle Isle')
lab.walk_to_cell(0, 10)
lab.run('LEFT', 30)
lab.run(None, 40)
check(lab.map == 17, 'west again to Hollywood Cemetery')

# Stage 6: the Gas Lantern from the Archivist in Capitol Square.
def equip(item):
    for _ in range(4):
        if lab.ram('eqItem') == item:
            return True
        lab.press('SELECT', 2)
    return lab.ram('eqItem') == item

lab.walk_to_cell(15, 0)
lab.run('UP', 30)
lab.run(None, 40)
lab.walk_to_cell(31, 10)
lab.run('RIGHT', 30)
lab.run(None, 40)
check(lab.map == 12, 'north to the Fan, east to Capitol Square')
lab.walk_to_cell(7, 8)
lab.face('up')
lab.press('A', 60)
lab.screenshot(f'{out}/15-archivist.png')
close_box()
check(lab.ram('hasLantern') == 1 and lab.ram('eqItem') == 3, 'the Archivist hands over the Gas Lantern')
lab.walk_to_cell(0, 10)
lab.run('LEFT', 30)
lab.run(None, 40)
lab.walk_to_cell(15, 21)
lab.run('DOWN', 30)
lab.run(None, 40)
check(lab.map == 17, 'back to Hollywood Cemetery')

# Stage 6: blast the rubble off the pyramid door; the Pyramid and the Wraith.
check(equip(2), 'SELECT switches B to the powder')
lab.walk_to_cell(21, 7)
lab.face('up')
lab.press('B', 2)
lab.walk_to_cell(21, 10)
lab.run(None, 90)
check(lab.ram('colRam', 1, False, 5 * 32 + 21) == 2, 'a keg clears the rubble from the pyramid door')
lab.walk_to_cell(21, 6)
lab.run('UP', 20)
lab.run(None, 70)
check(lab.map == lab.map_named('Hall of Lamps'), 'into the pyramid: the Hall of Lamps')
equip(3)
for cx, cy, d in [(3, 3, 'up'), (12, 3, 'up'), (12, 6, 'down'), (3, 6, 'down')]:
    lab.walk_to_cell(cx, cy)
    lab.face(d)
    lab.press('B', 4)
    if lab.ram('hp') <= 2:
        lab.poke('hp', lab.ram('hpMax'))
check(lab.ram('roomBraz', 1, False, lab.map) == 15, 'the lantern lights all four braziers')
check(lab.ram('colRam', 1, False, 7) == 0, 'and the gate opens')
lab.screenshot(f'{out}/16-hall-of-lamps.png')
lab.walk_to_cell(7, 1)
lab.run('UP', 40)
lab.run(None, 40)
check(lab.map == lab.map_named('Burial Vault') and clear_room(), 'the Burial Vault is cleared')
lab.walk_to_cell(8, 3)
lab.face('up')
lab.press('A', 60)
close_box()
check(lab.ram('hasShield') == 1, 'a chest appears with the Cast-Iron Shield')
lab.walk_to_cell(14, 5)
lab.run('RIGHT', 40)
lab.run(None, 40)
check(lab.map == lab.map_named('Ossuary') and clear_room(), 'east, the Ossuary is cleared')
lab.walk_to_cell(12, 3)
lab.face('up')
lab.press('A', 60)
close_box()
lab.walk_to_cell(7, 9)
lab.run('DOWN', 10)
lab.run('DOWN', 40)
lab.run(None, 60)
check(lab.map == lab.map_named('Heart of the Pyramid') and lab.ram('boxState'), 'its key opens the Heart of the Pyramid')
close_box()
lab.walk_to_cell(8, 7)
vis_seen = refl_seen = False
for _ in range(9000):
    if lab.ram('boxState') or not lab.ram('bossOn'):
        break
    if lab.ram('hp') <= 2:
        lab.poke('hp', lab.ram('hpMax'))
    bx, by = lab.ram('bossX', 2, True) >> 4, lab.ram('bossY', 2, True) >> 4
    x, y = lab.pos
    if any(lab.ram('shotRefl', 1, False, k) and lab.ram('shotOn', 1, False, k) for k in range(10)):
        refl_seen = True
    if lab.ram('bossVis'):
        vis_seen = True
        dx, dy = bx - x, (by - 4) - y
        if abs(dx) < 10 and abs(dy) < 22:
            lab.run('UP' if dy < 0 else 'DOWN', 1)
            lab.press('A', 10)
        else:
            lab.run(('RIGHT' if dx > 0 else 'LEFT') if abs(dx) >= 10 else ('UP' if dy < 0 else 'DOWN'), 1)
    elif y < 120:
        lab.run('DOWN', 1)
    elif abs(x - 128) > 4:
        lab.run('RIGHT' if x < 128 else 'LEFT', 1)
    elif lab.ram('heroDir') != 1:
        lab.face('up')         # stand facing it: the shield turns its orbs back
    else:
        lab.run(None, 1)
check(refl_seen, 'the shield sends an orb back at the Wraith')
check(vis_seen, 'its own orb makes the Wraith solid')
check(not lab.ram('bossOn') and lab.ram('roomFlags', 1, False, lab.map) & 2, 'the Wraith is beaten')
lab.run(None, 30)
close_box()
hpmax = lab.ram('hpMax')
for c in [(7, 4), (9, 4)]:
    try:
        lab.walk_to_cell(*c)
    except RuntimeError:
        pass
    close_box()
    lab.run('DOWN', 4)
    lab.run(None, 20)
    close_box()
lab.screenshot(f'{out}/17-wraith-beaten.png')
check(lab.ram('keystones') == 3, 'the Iron Keystone (2 of 4)')
check(lab.ram('hpMax') == hpmax + 2, 'and a heart container')
lab.walk_to_cell(7, 7)
lab.run('DOWN', 30)
lab.run(None, 60)
check(lab.map == 17, 'the stairs lead back out to Hollywood Cemetery')

# Stage 3: let a ghost take the last half-heart; wake in Capitol Square.
lab.poke('hp', 1)
for _ in range(1500):
    if lab.ram('dying'):
        break
    lab.run(None, 1)
check(lab.ram('dying') == 1, 'a ghost takes the last heart')
lab.run(None, 60)
lab.screenshot(f'{out}/18-fog-takes-you.png')
for _ in range(10):
    if not lab.ram('boxState'):
        break
    lab.press('A', 6)
lab.run(None, 60)
check(lab.map == 12 and lab.pos == (120, 136), f'wakes by the fountain in Capitol Square (map {lab.map}, {lab.pos})')
check(lab.ram('hp') == lab.ram('hpMax'), 'with full hearts')
sys.exit(0 if ok else 1)
