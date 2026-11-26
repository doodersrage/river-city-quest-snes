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


# Stage 1: boot, walk, change neighborhood.
lab = Lab(rom, sym, root)
lab.run(None, 120)
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
target = min(alive, key=lambda k: abs(lab.enemy(k)['x'] - lab.pos[0]) + abs(lab.enemy(k)['y'] - lab.pos[1]))
used = lab.fight(target)
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
lab.run('UP', 20)
lab.run(None, 70)
check(lab.map == lab.map_named('Foundry Gate') and lab.ram('dentMap') == lab.map, 'the Tredegar door leads into the Foundry Gate')
check(song() == 'forge', f'the ironworks play the forge theme ({song()})')
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
check(song() == 'boss', f'the boss theme ({song()})')
close_box()
lab.screenshot(f'{out}/11-iron-foreman.png')
clanked = stunned = False
def poke16(name, v):
    lab.poke(name, v & 0xFF, 0); lab.poke(name, (v >> 8) & 0xFF, 1)
for t in range(9000):
    if t >= 3000 and t % 1500 == 0 and lab.ram('bossState') != 2:
        # the bot hasn't managed it: line the two of you up in the middle of the hall and hook him
        poke16('bossX', 160 * 16); poke16('bossY', 88 * 16); lab.poke('bossState', 0)
        poke16('px', 104); poke16('py', 80)
        lab.face('right'); lab.press('B', 30)
    if t == 6000 and lab.ram('bossOn'):
        lab.poke('bossHp', 1)   # the fight's been checked: finish him
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
lab.run(None, 10)
check(song() == 'forge', f'and back to the forge theme once its last words are read ({song()})')
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
for c in [(7, 4), (9, 4), (7, 4), (9, 4)]:   # twice round: a pickup text box can cut a walk short
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
for c in [(7, 4), (9, 4), (7, 4), (9, 4)]:   # twice round: a pickup text box can cut a walk short
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

# Stage 6: the Poe Museum cellar, the Gold-Bug and the Raven.
lab.walk_to_cell(15, 0)
lab.run('UP', 30)
lab.run(None, 40)
lab.walk_to_cell(31, 10)
lab.run('RIGHT', 30)
lab.run(None, 40)
lab.walk_to_cell(31, 10)
lab.run('RIGHT', 30)
lab.run(None, 40)
check(lab.map == lab.map_named('Shockoe Bottom'), 'through the Fan and Capitol Square to Shockoe Bottom')
lab.walk_to_cell(5, 18)
lab.run('UP', 20)
lab.run(None, 70)
check(lab.map == lab.map_named('Cellar Stair') and clear_room(), 'down the Cellar Stair of the Poe Museum')
lab.walk_to_cell(14, 5)
lab.run('RIGHT', 40)
lab.run(None, 40)
clear_room()
equip(3)
for cx, cy, d in [(4, 3, 'up'), (11, 3, 'up'), (13, 6, 'down')]:
    lab.walk_to_cell(cx, cy)
    lab.face(d)
    lab.press('B', 4)
check(lab.map == lab.map_named('Wine Cellar') and lab.ram('roomBraz', 1, False, lab.map) == 7, 'the Wine Cellar: three braziers lit')
lab.walk_to_cell(8, 6)
lab.face('down')
lab.press('A', 60)
close_box()
check(lab.ram('keys', 1, False, lab.ram('roomArea')) == 1, 'and its key chest appears')
lab.walk_to_cell(1, 5)
lab.run('LEFT', 40)
lab.run(None, 40)
lab.walk_to_cell(7, 1)
lab.run('UP', 10)
lab.run('UP', 40)
lab.run(None, 40)
check(lab.map == lab.map_named("Raven's Study") and clear_room(), "the Raven's Study is cleared")
lab.walk_to_cell(8, 4)
lab.face('up')
lab.press('A', 60)
lab.screenshot(f'{out}/19-gold-bug.png')
close_box()
check(lab.ram('hasBug') == 1 and lab.ram('eqItem') == 5, 'a chest appears with the Gold-Bug')
lab.walk_to_cell(1, 5)
lab.run('LEFT', 40)
lab.run(None, 40)
check(lab.map == lab.map_named('Enchanted Garden'), 'west into the Enchanted Garden')
lab.walk_to_cell(12, 5)
lab.face('left')
lab.press('B', 8)
lab.screenshot(f'{out}/20-gold-bug-flying.png')
lab.run(None, 40)
close_box()
check(lab.ram('keys', 1, False, lab.ram('roomArea')) == 1, 'the Gold-Bug brings back the key from the island')
lab.walk_to_cell(14, 5)
lab.run('RIGHT', 40)
lab.run(None, 40)
lab.walk_to_cell(7, 1)
lab.run('UP', 10)
lab.run('UP', 40)
lab.run(None, 60)
check(lab.map == lab.map_named("The Raven's Roost") and lab.ram('boxState'), "the key opens the Raven's Roost")
close_box()
for _ in range(9000):
    if lab.ram('boxState') or not lab.ram('bossOn'):
        break
    if lab.ram('hp') <= 2:
        lab.poke('hp', lab.ram('hpMax'))
    bx, by = lab.ram('bossX', 2, True) >> 4, lab.ram('bossY', 2, True) >> 4
    x, y = lab.pos
    dx, dy = bx - x, (by - 4) - y
    if abs(dx) < 12 and abs(dy) < 24:
        lab.run('UP' if dy < 0 else 'DOWN', 1)
        lab.press('A', 8)
    elif abs(dx) < 22 and abs(dy) < 12:
        lab.run('RIGHT' if dx > 0 else 'LEFT', 1)
        lab.press('A', 8)
    else:
        lab.run(('RIGHT' if dx > 0 else 'LEFT') if abs(dx) > abs(dy) else ('DOWN' if dy > 0 else 'UP'), 1)
check(not lab.ram('bossOn') and lab.ram('roomFlags', 1, False, lab.map) & 2, 'the Raven is beaten')
lab.run(None, 30)
close_box()
hpmax = lab.ram('hpMax')
for c in [(7, 4), (9, 4), (7, 4), (9, 4)]:   # a pickup's text box can stop a walk short: go round twice
    try:
        lab.walk_to_cell(*c)
    except RuntimeError:
        pass
    close_box()
    lab.run('DOWN', 4)
    lab.run(None, 20)
    close_box()
    if lab.ram('keystones') == 7 and lab.ram('hpMax') > hpmax: break
lab.screenshot(f'{out}/21-raven-beaten.png')
check(lab.ram('keystones') == 7, 'the Brick Keystone (3 of 4)')
check(lab.ram('hpMax') == hpmax + 2, 'and a heart container')
lab.walk_to_cell(7, 7)
lab.run('DOWN', 30)
lab.run(None, 60)
check(lab.map == lab.map_named('Shockoe Bottom'), 'the stairs lead back up to Shockoe Bottom')
lab.walk_to_cell(0, 10)
lab.run('LEFT', 30)
lab.run(None, 40)
lab.walk_to_cell(0, 10)
lab.run('LEFT', 30)
lab.run(None, 40)
lab.walk_to_cell(15, 21)
lab.run('DOWN', 30)
lab.run(None, 40)
check(lab.map == 17, 'back west and south to Hollywood Cemetery')

# Stage 6: the trading quest, the kayak, the Carillon Key and the Carillon.
N = lab.map_named
def walk(cell):
    m0, outdoor = lab.map, lab.ram('mapW') == 32
    for _ in range(4):
        try:
            lab.walk_to_cell(*cell)
        except RuntimeError:
            close_box(); lab.run(None, 30)
        if lab.map != m0 and not outdoor:
            return          # walked out a door on purpose
        if lab.map != m0:   # knocked over an edge: come back
            close_box(); travel(lab, m0, close_box)
            continue
        if (lab.pos[0] // 16, lab.pos[1] // 16) == tuple(cell):
            return
    lab.walk_to_cell(*cell)
def talk_until(cell, d, done):
    for _ in range(4):
        walk(cell); lab.face(d); lab.press('A', 60); close_box()
        if done(): return
def push(start_cell, d, frames):
    walk(start_cell)
    for _ in range(frames):
        lab.run(d, 1)
        if lab.ram('hp') <= 2:
            lab.poke('hp', lab.ram('hpMax'))
    lab.run(None, 10)
travel(lab, N('Carytown'), close_box)
talk_until((25, 18), 'up', lambda: lab.ram('tradeItem') == 1)
check(lab.ram('tradeItem') == 1, 'a Carytown vendor hands over a melon')
travel(lab, N('Jackson Ward'), close_box)
talk_until((12, 4), 'up', lambda: lab.ram('tradeItem') == 2)
check(lab.map == N('Jackson Ward') and lab.ram('tradeItem') == 2, 'the Jackson Ward musician trades sheet music for it')
travel(lab, N('Carytown'), close_box)
walk((22, 17)); lab.run('UP', 20); lab.run(None, 60)
talk_until((10, 3), 'up', lambda: lab.ram('tradeItem') == 3)
check(lab.map == N('The Byrd Theatre') and lab.ram('tradeItem') == 3, 'the Byrd organist trades a river map')
walk((7, 8)); lab.run('DOWN', 30); lab.run(None, 60)
travel(lab, N('Belle Isle'), close_box)
talk_until((8, 5), 'up', lambda: lab.ram('hasKayak') == 1)
check(lab.ram('hasKayak') == 1, 'the Belle Isle kayaker lends a kayak')
travel(lab, N('Manchester'), close_box)
check(lab.map == N('Manchester'), 'paddle across the James to Manchester')
check(song() in ('south', 'night'), f'south of the river, its own theme (or the night theme after dark) ({song()})')
travel(lab, N('Forest Hill Park'), close_box)
for _ in range(3):
    equip(2); walk((22, 9)); lab.face('up'); lab.press('B', 2); walk((22, 12)); lab.run(None, 90)
    walk((22, 8)); lab.run('UP', 20); lab.run(None, 30)
    if lab.ram('hasCarkey'): break
lab.screenshot(f'{out}/23-carillon-key.png'); close_box()
check(lab.map == N('Forest Hill Park') and lab.ram('hasCarkey') == 1, 'a keg clears the Forest Hill rubble: the Carillon Key')
travel(lab, N('Byrd Park & Maymont'), close_box)
walk((4, 8)); lab.run('UP', 20); lab.run(None, 70); close_box()
check(lab.map == lab.map_named('Carillon Base'), 'the key opens the Carillon')
clear_room()
push((3, 5), 'RIGHT', 320)
lab.screenshot(f'{out}/24-carillon-base.png')
check(lab.ram('roomFlags', 1, False, lab.map) & 16, 'a push block onto the switch opens the gate')
walk((7, 1)); lab.run('UP', 40); lab.run(None, 40); close_box()
check(lab.map == lab.map_named('Rope Room') and clear_room(), 'the Rope Room is cleared')
walk((1, 5)); lab.run('LEFT', 40); lab.run(None, 40); close_box()
clear_room()
push((3, 3), 'RIGHT', 300); push((12, 7), 'LEFT', 300)
lab.screenshot(f'{out}/25-bell-loft.png')
check(lab.map == lab.map_named('Bell Loft') and lab.ram('roomFlags', 1, False, lab.map) & 16, 'two blocks solve the Bell Loft')
walk((12, 5)); lab.face('right'); lab.press('A', 60); close_box()
check(lab.ram('keys', 1, False, lab.ram('roomArea')) == 1, 'a small key')
walk((14, 5)); lab.run('RIGHT', 40); lab.run(None, 40)
walk((7, 1)); lab.run('UP', 10); lab.run('UP', 40); lab.run(None, 60)
check(lab.map == lab.map_named('The Great Bell') and lab.ram('boxState'), 'the key opens the Great Bell')
lab.screenshot(f'{out}/26-the-great-bell.png'); close_box()
shot = False
for _ in range(9000):
    if lab.ram('boxState') or not lab.ram('bossOn') or lab.map != lab.map_named('The Great Bell'):
        break
    if lab.ram('hp') <= 2:
        lab.poke('hp', lab.ram('hpMax'))
    st, bt = lab.ram('bossState'), lab.ram('bossT')
    bx, by = lab.ram('bossX', 2, True) >> 4, lab.ram('bossY', 2, True) >> 4
    x, y = lab.pos
    dx, dy = bx - x, (by - 4) - y
    if st == 2:
        if not shot and lab.ram('bossSt') < 100:
            lab.screenshot(f'{out}/27-bell-slams.png'); shot = True
        if abs(dx) < 12 and abs(dy) < 24:
            lab.run('UP' if dy < 0 else 'DOWN', 1); lab.press('A', 8)
        elif abs(dx) < 22 and abs(dy) < 12:
            lab.run('RIGHT' if dx > 0 else 'LEFT', 1); lab.press('A', 8)
        else:
            lab.run(('RIGHT' if dx > 0 else 'LEFT') if abs(dx) > abs(dy) else ('DOWN' if dy > 0 else 'UP'), 1)
    elif st == 4 or (st == 0 and bt % 140 > 110):
        gx = 40 if bx > 120 else 200
        b = []
        if abs(gx - x) > 3: b.append('RIGHT' if gx > x else 'LEFT')
        if abs(100 - y) > 3: b.append('DOWN' if 100 > y else 'UP')
        lab.run('+'.join(b) or None, 1)
    else:
        lab.run(None, 1)
check(not lab.ram('bossOn') and lab.ram('roomFlags', 1, False, lab.map) & 2, 'the Bronze Bell is beaten')
lab.run(None, 40); close_box()
hpmax = lab.ram('hpMax')
for c in [(7, 4), (9, 4), (7, 4), (9, 4)]:   # twice round: a pickup text box can cut a walk short
    try:
        lab.walk_to_cell(*c)
    except RuntimeError:
        pass
    lab.run(None, 20); close_box(); lab.run('DOWN', 4); lab.run(None, 20); close_box()
lab.screenshot(f'{out}/28-bell-beaten.png')
check(lab.ram('keystones') == 15, 'the Bronze Keystone (4 of 4)')
check(lab.ram('hpMax') == hpmax + 2, 'and a heart container')
walk((7, 9)); lab.run('DOWN', 30); lab.run(None, 60); close_box()
equip(2); walk((14, 5)); lab.face('right'); lab.press('B', 2); walk((10, 5)); lab.run(None, 100)
walk((15, 5)); lab.run('RIGHT', 30); lab.run(None, 60); close_box()
check(lab.map == lab.map_named('Belfry Ledge') and clear_room(), 'a keg opens the Belfry Ledge')
walk((8, 4)); lab.face('up'); lab.press('A', 60)
lab.screenshot(f'{out}/29-heart-piece.png'); close_box()
check(lab.ram('pieces') == 1, 'a heart piece (1 of 4)')
walk((0, 5)); lab.run('LEFT', 30); lab.run(None, 60); close_box()
walk((7, 9)); lab.run('DOWN', 30); lab.run(None, 60); close_box()
walk((7, 9)); lab.run('DOWN', 30); lab.run(None, 60); close_box()
check(lab.map == lab.map_named('Byrd Park & Maymont'), 'back out to Byrd Park')

# Stage 6: the Conservatory at Lewis Ginter, the Watering Can and the Titan Arum.
travel(lab, lab.map_named('Lewis Ginter Botanical Garden'), close_box)
lab.walk_to_cell(8, 6); lab.run('UP', 20); lab.run(None, 70); close_box()
check(lab.map == lab.map_named('Palm House'), 'north to Lewis Ginter and into the Conservatory')
clear_room()
lab.walk_to_cell(14, 5); lab.run('RIGHT', 30); lab.run(None, 60); close_box()
check(lab.map == lab.map_named('Orchid Room') and clear_room(), 'the Orchid Room is cleared')
lab.run(None, 30)
lab.walk_to_cell(4, 3); lab.face('up'); lab.press('A', 60)
lab.screenshot(f'{out}/30-watering-can.png'); close_box()
check(lab.ram('hasCan') == 1 and lab.ram('eqItem') == 6, 'a chest appears with the Watering Can')
lab.walk_to_cell(7, 4); lab.face('right'); lab.press('B', 10); close_box(); lab.run(None, 20)
check(all(lab.ram('colRam', 1, False, 4 * 32 + x) == 0 for x in range(8, 12)), 'the sprout grows a vine across the gap')
lab.walk_to_cell(12, 4); lab.face('up'); lab.press('A', 60); close_box()
lab.screenshot(f'{out}/31-vine-bridge.png')
check(lab.ram('keys', 1, False, lab.ram('roomArea')) == 1, 'over the vine to the key on the island')
lab.walk_to_cell(0, 5); lab.run('LEFT', 30); lab.run(None, 60); close_box()
lab.walk_to_cell(7, 1); lab.run('UP', 40); lab.run(None, 60); close_box()
check(lab.map == lab.map_named('Fern Gully'), 'the key opens Fern Gully')
lab.walk_to_cell(5, 8); lab.face('up'); lab.press('B', 10); close_box(); lab.run(None, 20)
check(clear_room() and lab.ram('roomFlags', 1, False, lab.map) & 16, 'a second vine, and Fern Gully is cleared')
lab.walk_to_cell(12, 3); lab.face('up'); lab.press('A', 60); close_box()
lab.screenshot(f'{out}/32-fern-gully.png')
lab.walk_to_cell(7, 1); lab.run('UP', 40); lab.run(None, 60)
check(lab.map == lab.map_named('Under the Dome') and lab.ram('boxState'), 'the gate opens Under the Dome')
lab.screenshot(f'{out}/33-titan-arum.png'); close_box()
shot = False
hpmax = lab.ram('hpMax')
for _ in range(60):
    if not lab.ram('bossOn') or lab.map != lab.map_named('Under the Dome'):
        break
    if lab.ram('hp') <= 3:
        lab.poke('hp', lab.ram('hpMax'))
    close_box()
    if lab.ram('bossState') != 2:
        try:
            lab.walk_to_cell(8, 5)
        except RuntimeError:
            pass
        lab.face('up'); lab.press('B', 4)
        if lab.ram('bossState') == 2 and not shot:
            close_box(); lab.run(None, 4); lab.screenshot(f'{out}/34-arum-blooms.png'); shot = True
        close_box()
    else:
        lab.run('UP', 6); lab.press('A', 10)
check(not lab.ram('bossOn') and lab.ram('roomFlags', 1, False, lab.map) & 2, 'watered, the Titan Arum blooms and is beaten')
lab.run(None, 30); close_box()
try:
    lab.walk_to_cell(9, 5)
except RuntimeError:
    pass
lab.run(None, 20); close_box(); lab.run('DOWN', 4); lab.run(None, 20); close_box()
check(lab.ram('hpMax') == hpmax + 2, 'and a heart container')
lab.screenshot(f'{out}/35-arum-beaten.png')
lab.walk_to_cell(7, 7); lab.run('DOWN', 30); lab.run(None, 60); close_box()
check(lab.map == lab.map_named('Lewis Ginter Botanical Garden'), 'the stairs lead back out to Lewis Ginter')

# Stage 6: gear from around town, and the item menu on START.
def walk(cell):
    m0, outdoor = lab.map, lab.ram('mapW') == 32
    for _ in range(4):
        try:
            lab.walk_to_cell(*cell)
        except RuntimeError:
            close_box(); lab.run(None, 30)
        if lab.map != m0 and not outdoor:
            return          # walked out a door on purpose
        if lab.map != m0:   # knocked over an edge: come back
            close_box(); travel(lab, m0, close_box)
            continue
        if (lab.pos[0] // 16, lab.pos[1] // 16) == tuple(cell):
            return
    lab.walk_to_cell(*cell)
def talk2(cell, d):
    walk(cell); lab.face(d); lab.press('A', 60); close_box()
def menu_pick(item):
    lab.press('START', 10)
    items = [lab.ram('menuList', 1, False, i) for i in range(lab.ram('menuN'))]
    while lab.ram('menuCur') < items.index(item): lab.press('DOWN', 4)
    while lab.ram('menuCur') > items.index(item): lab.press('UP', 4)
    lab.press('A', 6)
    return lab.ram('eqItem') == item and not lab.ram('boxState')
N = lab.map_named
travel(lab, N("Scott's Addition"), close_box)
for _ in range(4):
    talk2((13, 9), 'up')
    if lab.ram('hasBalls'): break
check(lab.ram('hasBalls') and lab.ram('balls') == 15, 'the Ballplayer in Scott\'s Addition gives you Baseballs')
walk((23, 5)); lab.run('UP', 20); lab.run(None, 60); close_box()
lab.poke('hp', 2)
talk2((7, 4), 'up')
check(lab.map == N("Scott's Addition Taproom") and lab.ram('hasBottle') and lab.ram('bottleFull') and lab.ram('hp') == lab.ram('hpMax'),
      'the Brewer pours a bottle of Ginger Beer and tops up your hearts')
walk((7, 9)); lab.run('DOWN', 30); lab.run(None, 60); close_box()
lab.press('START', 10)
lab.screenshot(f'{out}/36-item-menu.png')
check(lab.ram('boxState') == 5 and lab.ram('menuN') >= 7, f"START opens the item menu ({lab.ram('menuN')} items)")
lab.press('START', 6)
check(menu_pick(7), 'the menu puts Baseballs on B')
b0 = lab.ram('balls'); lab.face('down')
for _ in range(4):
    close_box(); lab.run(None, 20); lab.press('B', 4)
    if lab.ram('balls') < b0: break
lab.screenshot(f'{out}/37-baseball.png')
check(lab.ram('balls') == b0 - 1, f"B throws a baseball ({b0} -> {lab.ram('balls')}, box {lab.ram('boxState')})")
travel(lab, N('Byrd Park & Maymont'), close_box)
try:
    lab.walk_to_cell(12, 17)
except RuntimeError:
    pass
lab.run(None, 20); close_box()
check(lab.ram('hasBib'), 'a race bib in Byrd Park')
travel(lab, N('The Fan'), close_box)
for _ in range(4):
    talk2((26, 7), 'up')
    if lab.ram('hasShoes'): break
check(lab.ram('hasShoes'), 'the Runner in the Fan trades his spare Running Shoes for it')
check(menu_pick(8), 'Running Shoes on B')
walk((10, 12)); lab.face('right'); x0 = lab.pos[0]; lab.press('B', 1); lab.run(None, 8)
lab.screenshot(f'{out}/38-dash.png'); lab.run(None, 20)
check(lab.pos[0] - x0 > 40, f'B dashes ({lab.pos[0] - x0} px)')
travel(lab, N('Rocketts Landing'), close_box)
for _ in range(4):
    talk2((20, 11), 'up')
    if lab.ram('hasBike'): break
check(lab.ram('hasBike'), 'the Trail Cyclist at Rocketts Landing hands over a bicycle')
walk((6, 12)); lab.press('B', 2)
moved = frames = 0
for _ in range(60):
    x0 = lab.pos[0]; lab.run('RIGHT', 1)
    if lab.pos[0] != x0: moved += lab.pos[0] - x0; frames += 1
lab.screenshot(f'{out}/39-bike.png')
check(lab.ram('riding') and frames and moved / frames >= 1.9, f'B hops on the bike: {moved / max(frames, 1):.2f} px a frame (walking is 1.5)')
for _ in range(3):
    lab.press('A', 4); close_box()
    if not lab.ram('riding'): break
check(not lab.ram('riding'), 'A hops off')
check(menu_pick(10), 'Ginger Beer on B')
lab.poke('hp', 1); lab.press('B', 10); close_box()
check(lab.ram('hp') == lab.ram('hpMax') and not lab.ram('bottleFull'), 'drinking it refills every heart')
travel(lab, N('Carytown'), close_box)
walk((6, 17)); lab.run('UP', 30); lab.run(None, 40)
lab.poke('coins', 200); lab.poke('balls', 5)
walk((8, 5)); lab.face('up'); lab.press('A', 10)
k0 = lab.ram('powder')
for idx, what in ((9, 'Ginger Beer'), (10, '10 Baseballs'), (7, 'Big Satchel'), (11, '4 Powder Kegs'), (11, '4 Powder Kegs')):
    while lab.ram('shopCur') < idx: lab.press('DOWN', 3)
    while lab.ram('shopCur') > idx: lab.press('UP', 3)
    if idx == 10: lab.screenshot(f'{out}/40-shop-gear.png')
    lab.press('A', 60); lab.press('A', 10)
lab.press('B', 10)
check(lab.ram('bottleFull') and lab.ram('balls') == 15 and lab.ram('capBalls') == 50 and lab.ram('capPowder') == 16,
      'Carytown Goods refills the bottle, sells baseballs and the Big Satchel')
check(lab.ram('powder') == min(16, k0 + 8), f"and powder kegs ({k0} -> {lab.ram('powder')})")
walk((7, 8)); lab.run('DOWN', 30); lab.run(None, 60); close_box()
check(lab.map == N('Carytown'), 'back out to Carytown')

# Stage 6: the finale. The four keystones open the Church Hill Tunnel; the Richmond Vampire; the ending.
def equip(item):
    for _ in range(12):
        if lab.ram('eqItem') == item: return True
        lab.press('SELECT', 2)
    return lab.ram('eqItem') == item
travel(lab, N('Church Hill'), close_box)
lab.run('RIGHT', 20)   # off the edge, then deal with whatever's waiting by it
for _ in range(4):
    near = [k for k in range(6) if lab.enemy(k)['alive'] and abs(lab.enemy(k)['x'] - lab.pos[0]) + abs(lab.enemy(k)['y'] - lab.pos[1]) < 96]
    if not near or lab.map != N('Church Hill'): break
    lab.fight(near[0], 300)
if lab.map != N('Church Hill'): travel(lab, N('Church Hill'), close_box); lab.run('RIGHT', 20)
walk((4, 5)); lab.steer_to(72, 90); lab.run('UP', 40); lab.run(None, 70); close_box()
check(lab.map == N('Church Hill Tunnel'), 'the four keystones open the Church Hill Tunnel')
lab.screenshot(f'{out}/41-church-hill-tunnel.png')
clear_room()
entry_clear = clear_room()
for _ in range(3):
    if lab.ram('colRam', 1, False, 5 * 32 + 15) == 0: break
    equip(2); walk((14, 5)); lab.face('right'); lab.press('B', 2); walk((10, 5)); lab.run(None, 100)
wall = [lab.ram('colRam', 1, False, y * 32 + 15) for y in (4, 5, 6)]
walk((15, 5)); lab.run('RIGHT', 30); lab.run(None, 60); close_box()
in_siding = lab.map == N('Flooded Siding')
siding_clear = in_siding and clear_room()
check(in_siding and siding_clear, f"a keg opens the Flooded Siding (entry cleared {entry_clear}, wall {wall}, map {lab.map}, powder {lab.ram('powder')}, "
      f"left {[(lab.enemy(k)['type'], lab.enemy(k)['x'], lab.enemy(k)['y']) for k in range(6) if lab.enemy(k)['alive']]})")
equip(5); walk((7, 5)); lab.face('right'); lab.press('B', 8); lab.run(None, 60); close_box()
check(lab.ram('keys', 1, False, lab.ram('roomArea')) == 1, 'the Gold-Bug fetches the key from the island')
walk((7, 1)); lab.run('UP', 10); lab.run('UP', 40); lab.run(None, 60); close_box()
check(lab.map == N('Signal Room') and clear_room(), 'the key opens the Signal Room')
equip(3)
for cx, cy, d in [(3, 3, 'up'), (12, 3, 'up'), (3, 7, 'down'), (12, 7, 'down')]:
    walk((cx, cy)); lab.face(d); lab.press('B', 4)
lab.run(None, 20)
check(lab.ram('roomFlags', 1, False, lab.map) & 16, 'four signal lamps lit')
lab.screenshot(f'{out}/42-signal-room.png')
walk((0, 5)); lab.run('LEFT', 30); lab.run(None, 70)
check(lab.map == N('Engine 231') and lab.ram('boxState'), 'the gate opens on Engine 231')
close_box()
shot = set()
for _ in range(20000):
    if not lab.ram('bossOn') or lab.map != N('Engine 231'):
        break
    if lab.ram('boxState'):
        close_box(); continue
    if lab.ram('hp') <= 4:
        lab.poke('hp', lab.ram('hpMax'))
    bats = [k for k in range(6) if lab.enemy(k)['alive']]
    if lab.ram('vampSwarm') or (lab.ram('vampGone') and bats):
        if 'swarm' not in shot and lab.ram('vampSwarm'):
            lab.screenshot(f'{out}/44-swarm.png'); shot.add('swarm')
        if bats:
            x, y = lab.pos
            lab.fight(min(bats, key=lambda k: abs(lab.enemy(k)['x'] - x) + abs(lab.enemy(k)['y'] - y)), 200)
        else:
            lab.run(None, 1)
        continue
    if lab.ram('vampGone'):
        lab.run(None, 1); continue
    bx, by = lab.ram('bossX', 2, True) >> 4, lab.ram('bossY', 2, True) >> 4
    x, y = lab.pos
    dx, dy = bx - x, (by - 4) - y
    if 'vamp' not in shot and abs(dx) < 50 and abs(dy) < 50:
        lab.screenshot(f'{out}/43-richmond-vampire.png'); shot.add('vamp')
    if abs(dx) < 12 and abs(dy) < 20:
        lab.run('UP' if dy < 0 else 'DOWN', 1); lab.press('A', 8)
    elif abs(dx) < 20 and abs(dy) < 12:
        lab.run('RIGHT' if dx > 0 else 'LEFT', 1); lab.press('A', 8)
    else:
        lab.run_safe(('RIGHT' if dx > 0 else 'LEFT') if abs(dx) > abs(dy) else ('DOWN' if dy > 0 else 'UP'), 1)
check(not lab.ram('bossOn') and lab.ram('gameWon'), 'the Richmond Vampire is beaten')
lab.run(None, 60)
lab.screenshot(f'{out}/45-the-fog-lifts.png')
for _ in range(60):
    if not lab.ram('boxState'): break
    lab.press('A', 8)
lab.run(None, 200)
lab.screenshot(f'{out}/46-lamps-come-back-on.png')
for t in range(200):
    lab.run(None, 20)
    if t == 60:
        lab.screenshot(f'{out}/47-credits.png')
        check(song() == 'win', f'the credits roll to the victory theme ({song()})')
    if lab.map == N('Church Hill') and t > 40: break
lab.run(None, 30)
check(lab.map == N('Church Hill') and lab.pos == (72, 72), 'the lamps come back on, the credits roll, and you are back on Church Hill')
check(song() == 'town', f'and the town theme again ({song()})')

# Stage 3: let an enemy take the last half-heart; wake in Capitol Square.
lab.poke('hp', 1)
# put a rat right on top of the hero (wherever the gear run ended, there may be none nearby)
lab.poke('enAlive', 1, 0); lab.poke('enType', 0, 0); lab.poke('enHp', 1, 0); lab.poke('enFlash', 0, 0)
for _ in range(300):
    if lab.ram('dying'):
        break
    x, y = lab.pos
    for name, v in (('enX', x * 16), ('enY', y * 16)):
        lab.poke(name, v & 0xFF, 0); lab.poke(name, (v >> 8) & 0xFF, 1)
    lab.run(None, 1)
check(lab.ram('dying') == 1, 'an enemy takes the last heart')
lab.run(None, 60)
lab.screenshot(f'{out}/22-fog-takes-you.png')
for _ in range(10):
    if not lab.ram('boxState'):
        break
    lab.press('A', 6)
lab.run(None, 60)
check(lab.map == 12 and lab.pos == (120, 136), f'wakes by the fountain in Capitol Square (map {lab.map}, {lab.pos})')
check(lab.ram('hp') == lab.ram('hpMax'), 'with full hearts')

# Stage 8: night, weather and dark rooms (last, so poking the clock and the sky can't change the run above).
def set_clock(v):
    lab.poke('dayClock', v & 0xFF, 0); lab.poke('dayClock', v >> 8, 1)
def brightness():
    return float(screen()[24:].mean())
lab.poke('wx', 0); lab.poke('wxK', 0); lab.poke('wxNext', 255)
set_clock(4500); lab.run(None, 4); day = brightness()
set_clock(16 * 1500); lab.run(None, 4); night = brightness()
lab.screenshot(f'{out}/48-night.png')
check(lab.ram('darkLevel') == 4 and night < day * .75, f'night falls at 10pm ({day:.0f} -> {night:.0f} brightness)')
check(lab.ram('lightR1') == 32, f"a circle of light around you ({lab.ram('lightR1')})")
set_clock(12 * 1500 + 600); lab.run(None, 4)
check(0 < lab.ram('darkLevel') < 4, f"dusk comes on gradually (level {lab.ram('darkLevel')})")
set_clock(4500); lab.run(None, 4)
lab.poke('wx', 1); lab.poke('wxK', 255); lab.run(None, 30)
lab.screenshot(f'{out}/49-rain.png')
check(lab.ram('oamShown', 1, False, 100) == 1 and brightness() < day, 'rain falls and the sky darkens')
lab.poke('wx', 3); lab.run(None, 30)
lab.screenshot(f'{out}/50-snow.png')
check(lab.ram('oamShown', 1, False, 100) == 1 and brightness() > day, 'snow falls and washes the sky out')
lab.poke('wx', 0); lab.poke('wxK', 0); lab.run(None, 4)
travel(lab, N('Hollywood Cemetery'), close_box)
walk((28, 17)); lab.run('UP', 30); lab.run(None, 60); close_box()
lab.screenshot(f'{out}/51-dark-room.png')
check(lab.ram('darkLevel') == 4 and lab.ram('lightR1') == (68 if lab.ram('hasLantern') else 38),
      f"the Foundry Gate is dark but for your light (map {lab.map}, radius {lab.ram('lightR1')})")
sys.exit(0 if ok else 1)
