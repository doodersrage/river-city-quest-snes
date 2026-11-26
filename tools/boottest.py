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
hpmax_before_foreman = lab.ram('hpMax')
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
hpmax = hpmax_before_foreman   # (the fight may already have walked over the container)
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
travel(lab, lab.map_named('Belle Isle'), close_box)
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
if lab.ram('tradeItem') != 2:   # say why
    print('  musician:', [(lab.ram('npcX', 2, True, k), lab.ram('npcY', 2, True, k)) for k in range(lab.ram('npcCount'))],
          'dir', lab.ram('heroDir'), 'atk', lab.ram('atk'), 'spin', lab.ram('spin'), 'kt', lab.ram('kt'), 'hook', lab.ram('hookSt'),
          'dash', lab.ram('dash'), 'charging', lab.ram('charging'), 'dying', lab.ram('dying'), 'melonTaken', lab.ram('melonTaken'), 'kayak', lab.ram('hasKayak'))
    lab.face('up'); f0 = lab.ram('frameCount', 2); lab.press('A', 2)
    print('  pressed A: box', lab.ram('boxState'), 'frames', lab.ram('frameCount', 2) - f0, 'dir', lab.ram('heroDir'), 'atk', lab.ram('atk'))
check(lab.map == N('Jackson Ward') and lab.ram('tradeItem') == 2, f"the Jackson Ward musician trades sheet music for it (map {lab.map}, trade {lab.ram('tradeItem')}, challenge {lab.ram('chalOn')}, at {lab.pos}, box {lab.ram('boxState')})")
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
check(lab.ram('lightR1') == (54 if lab.ram('hasLantern') else 32), f"a circle of light around you, wider with the lantern ({lab.ram('lightR1')})")
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

# Stage 9: town life. Cars and the bus, townspeople out for a walk, pigeons, the bateau.
def poke16k(name, k, v):
    lab.poke(name, v & 0xFF, 2 * k); lab.poke(name, (v >> 8) & 0xFF, 2 * k + 1)
set_clock(4500)
for _ in range(3):   # back out of the Foundry Gate the way we came
    if lab.map == N('Hollywood Cemetery'): break
    lab.run('DOWN', 40); lab.run(None, 40); close_box()
travel(lab, N('Broad Street'), close_box)
lab.run(None, 10)
cars = [(lab.ram('carOn', 1, False, k), lab.ram('carBus', 1, False, k)) for k in range(2)]
check(cars == [(1, 1), (1, 0)], f'Broad Street has the Pulse bus and a car ({cars})')
x0 = lab.ram('carX', 2, True, 1)
lab.run(None, 30)
check(lab.ram('carX', 2, True, 1) != x0, 'the car drives along')
# wait for the bus to come onto the screen, then take a picture
for _ in range(600):
    bx = lab.ram('carX', 2, True, 0) >> 4
    if lab.ram('camX', 2, True) + 40 < bx < lab.ram('camX', 2, True) + 200:
        break
    lab.run(None, 1)
lab.screenshot(f'{out}/52-broad-street-bus.png')
check(lab.ram('oamShown', 1, False, 112) == 2, 'the bus is drawn')
# step into the car's lane just ahead of it: it stops for you and waits
cx, cy = lab.ram('carX', 2, True, 1) >> 4, lab.ram('carY', 2, True, 1)
lab.poke('inv', 255)
poke16('px', max(24, min(488, cx - 24))); poke16('py', cy)
lab.run(None, 30)
xs = lab.ram('carX', 2, True, 1); lab.run(None, 60)
check(lab.ram('carStop', 1, False, 1) == 1 and lab.ram('carX', 2, True, 1) == xs, 'a car stops for you')
lab.screenshot(f'{out}/53-car-stops.png')
lab.run('RIGHT', 30)
edge = (lab.ram('carX', 2, True, 1) >> 4) - 13
check(lab.pos[0] < edge, f"and you can't walk through it (you {lab.pos[0]}, its back bumper {edge})")
lab.run('DOWN', 30)
for i in range(12):
    if lab.ram('carX', 2, True, 1) != xs: break
    lab.run('UP' if i >= 4 else 'DOWN', 12); lab.run(None, 20)   # out of its lane, whichever way is open
check(lab.ram('carX', 2, True, 1) != xs, 'it drives on once you are out of the way')
# the Local on Broad Street wanders near home
k = 1
home = (lab.ram('npcHx', 2, True, k), lab.ram('npcHy', 2, True, k))
moved = False
for _ in range(40):
    lab.run(None, 30)
    p_ = (lab.ram('npcX', 2, True, k), lab.ram('npcY', 2, True, k))
    moved |= p_ != home
    if abs(p_[0] - home[0]) >= 40 or abs(p_[1] - home[1]) >= 40: moved = None; break
check(moved is True, f'a townsperson walks about, staying near home ({home} -> {p_})')
# pigeons take off when you come close
bird = (lab.ram('birdX', 2, True, 0) >> 4, lab.ram('birdY', 2, True, 0) >> 4)
check(lab.ram('birdSt', 1, False, 0) == 0, f'a pigeon sits on Broad Street {bird}')
walk((bird[0] // 16, bird[1] // 16 - 2))
for _ in range(30):
    if lab.ram('birdSt', 1, False, 0): break
    lab.run('DOWN', 2)
lab.run(None, 12)
lab.screenshot(f'{out}/54-pigeon.png')
check(lab.ram('birdSt', 1, False, 0) == 1, 'and flies off as you come near')
# the bateau: three parts for the river historian at Ancarrow's Landing
def take_pickup(kind):
    for k in range(6):
        if lab.ram('pkKind', 1, False, k) == kind:
            back = lab.pos   # the sandbar is out in the water: hop over and back
            poke16('px', lab.ram('pkX', 2, True, k)); poke16('py', lab.ram('pkY', 2, True, k))
            lab.run(None, 10)
            poke16('px', back[0]); poke16('py', back[1])
            return True
    return False
travel(lab, N('Pony Pasture'), close_box)
lab.run(None, 10)
got = take_pickup(14)
check(got and lab.ram('parts') == 1 and lab.ram('boxState'), f"a bateau part at Pony Pasture ({lab.ram('parts')} of 3)")
lab.screenshot(f'{out}/55-bateau-oar.png')
close_box(); take_pickup(14); close_box()
check(lab.ram('parts') == 2, 'and another')
travel(lab, N("Ancarrow's Landing"), close_box)
lab.run(None, 10)
take_pickup(14); close_box()
check(lab.ram('parts') == 3, 'the third on the island off the landing')
coins, hpm = lab.ram('coins', 2), lab.ram('hpMax')
for _ in range(4):
    walk((16, 12)); lab.face('up'); lab.press('A', 40)
    if lab.ram('bateauDone'): break
    close_box()
lab.screenshot(f'{out}/56-the-bateau.png')
close_box()
check(lab.ram('bateauDone') and lab.ram('coins', 2) == min(999, coins + 50) and lab.ram('hpMax') == min(20, hpm + 2),
      f"the river historian rebuilds the bateau: 50 coins and a heart charm ({coins} -> {lab.ram('coins', 2)}, {hpm} -> {lab.ram('hpMax')}; map {lab.map} {lab.pos})")

# Stage 10: the neighbourhood challenges.
import re
hdr = open(os.path.join(root, 'maps.h')).read()
def carr(name):
    return [int(v) for v in re.search(r'%s\[\d+\] = \{([^}]*)\}' % name, hdr).group(1).split(',')]
CH = ['flies', 'bus', 'taps', 'lamps', 'melons', 'tenk', 'quiz', 'rats', 'lures', 'ghosts', 'cleanup', 'crabs', 'trolley', 'murals']
chal_off, chal_xy = carr('chalOff'), carr('chalXY')
def chal_cells(c):
    return [(chal_xy[2 * i], chal_xy[2 * i + 1]) for i in range(chal_off[c], chal_off[c + 1])]
def talk_to(k):
    """Walk up to person k on this map and press A (from below, else the other sides)."""
    for _ in range(4):
        nx, ny = lab.ram('npcX', 2, True, k) // 16, lab.ram('npcY', 2, True, k) // 16
        g = lab.solid_grid()
        for (cx, cy), d in (((nx, ny + 1), 'up'), ((nx - 1, ny), 'right'), ((nx + 1, ny), 'left'), ((nx, ny - 1), 'down')):
            if 0 <= cx < 32 and 0 <= cy < 22 and not g[cy][cx]:
                walk((cx, cy)); lab.face(d); lab.press('A', 30)
                if lab.ram('boxState'): return True
                break
        lab.run(None, 30)
    return False
def accept(k, want=None):
    """Talk to them until they offer their challenge (or minigame: want=3), then say yes."""
    for _ in range(4):
        r = talk_to(k)
        if os.environ.get('DEBUG'):
            print('  accept', k, 'talked', r, 'box', lab.ram('boxState'), 'ask', lab.ram('askKind'), 'at', lab.pos, 'dir', lab.ram('heroDir'),
                  'npc', (lab.ram('npcX', 2, True, k), lab.ram('npcY', 2, True, k)))
        for _ in range(30):
            if lab.ram('boxState') in (0, 6): break
            lab.press('A', 6)
        if lab.ram('boxState') == 6 and want and lab.ram('askKind') != want:
            lab.press('B', 10); close_box(); continue   # someone else's question (the person next to them)
        if lab.ram('boxState') == 6:
            for _ in range(3):
                if not lab.ram('askCur'): break
                lab.press('UP', 4)
            lab.press('A', 10)
            return True
        close_box()
    return False
def poke16(name, v, k=0):
    lab.poke(name, v & 0xFF, 2 * k); lab.poke(name, (v >> 8) & 0xFF, 2 * k + 1)
def hop(x, y):
    back = lab.pos
    poke16('px', x); poke16('py', y); lab.run(None, 6)
    poke16('px', back[0]); poke16('py', back[1]); lab.run(None, 2)
lab.poke('inv', 0)
set_clock(4500)
# Melon Roundup in Carytown: eight melons
travel(lab, N('Carytown'), close_box)
lab.run(None, 4)
seen = False
for _ in range(40):
    seen |= lab.ram('oamShown', 1, False, 127) == 2
    lab.run(None, 1)
check(seen, 'a "!" over someone with a challenge')
coins = lab.ram('coins', 2)
check(accept(2) and lab.ram('chalOn') == CH.index('melons') + 1, f"the Melon Vendor's challenge starts (chalOn {lab.ram('chalOn')})")
close_box()
lab.run(None, 30)
lab.screenshot(f'{out}/57-melon-roundup.png')
toks = [(lab.ram('pkX', 2, True, k), lab.ram('pkY', 2, True, k)) for k in range(10) if lab.ram('pkKind', 1, False, k) == 15]
check(len(toks) == len(chal_cells(CH.index('melons'))) and lab.ram('chalNeed') == len(toks), f'melons all over Cary Street ({len(toks)})')
check(lab.ram('oamShown', 1, False, 120) == 1, 'a countdown at the top of the screen')
for x, y in toks:
    hop(x, y)
    if lab.ram('boxState'): break
lab.run(None, 40); lab.screenshot(f'{out}/58-city-pin.png')
check(not lab.ram('chalOn') and lab.ram('pins', 2) & (1 << CH.index('melons')) and lab.ram('coins', 2) == min(999, coins + 25),
      f"gather them all: a city pin and 25 coins ({coins} -> {lab.ram('coins', 2)})")
close_box()
# Catch the Bus on Broad Street: take too long and it's over
travel(lab, N('Broad Street'), close_box)
lab.run(None, 4)
check(accept(0) and lab.ram('chalOn') == CH.index('bus') + 1, 'the Commuter dares you to beat the bus')
close_box()
for _ in range(1500):   # 8 seconds of game time (a busy street runs a little slow)
    if lab.ram('boxState'): break
    lab.run(None, 1)
lab.screenshot(f'{out}/59-times-up.png')
check(not lab.ram('chalOn') and lab.ram('boxState') and not any(lab.ram('pkKind', 1, False, k) == 16 for k in range(10)), "time's up, the flag goes away")
close_box()
accept(0); close_box()
fl = [k for k in range(10) if lab.ram('pkKind', 1, False, k) == 16]
check(len(fl) == 1, 'second try: the flag at the bus shelter')
if fl: hop(lab.ram('pkX', 2, True, fl[0]), lab.ram('pkY', 2, True, fl[0]))
check(lab.ram('pins', 2) & (1 << CH.index('bus')), 'reach it in time: another pin')
close_box()
# Market Rats in Shockoe Bottom: eight rats, six at a time
travel(lab, N('Shockoe Bottom'), close_box)
lab.run(None, 4)
accept(1); close_box()
c = CH.index('rats')
spawned = sum(lab.ram('enChal', 1, False, k) for k in range(6))
check(lab.ram('chalOn') == c + 1 and spawned >= 2, f'Market Rats: rats come out ({spawned} at once, more as you beat them)')
lab.poke('inv', 255)
for _ in range(40):
    if not lab.ram('chalOn'): break
    poke16('chalT', 3000)
    left = [k for k in range(6) if lab.ram('enChal', 1, False, k) and lab.ram('enAlive', 1, False, k)]
    if not left: lab.run(None, 10); continue
    lab.poke('enHp', 1, left[0])
    lab.fight(left[0], 400)
check(lab.ram('pins', 2) & (1 << c), f"clear out eight: a pin ({lab.ram('chalGot')} of {lab.ram('chalNeed')})")
close_box()
lab.poke('inv', 0)
# Light the Gas Lamps on Church Hill with the lantern
travel(lab, N('Church Hill'), close_box)
lab.run(None, 4)
c = CH.index('lamps')
accept(1); close_box()
check(lab.ram('chalOn') == c + 1, 'the Lamplighter hands you the job')
equip(3)
g = lab.solid_grid()
for tx, ty in chal_cells(c):
    for (cx, cy), d in (((tx, ty + 1), 'up'), ((tx, ty - 1), 'down'), ((tx - 1, ty), 'right'), ((tx + 1, ty), 'left')):
        if 0 <= cy < 22 and 0 <= cx < 32 and not g[cy][cx]:
            poke16('px', cx * 16 + 8); poke16('py', cy * 16 + 8); lab.run(None, 2)
            lab.face(d); lab.press('B', 6)
            break
    poke16('chalT', 3000)
    if not lab.ram('chalOn'): break
lab.run(None, 40); lab.screenshot(f'{out}/60-gas-lamps.png')
check(lab.ram('pins', 2) & (1 << c), f"every lamp lit: a pin ({lab.ram('chalGot')} of {lab.ram('chalNeed')})")
close_box()
# The history quiz in Capitol Square: three stories from your journal
travel(lab, N('Capitol Square'), close_box)
lab.run(None, 4)
n_read = sum(bin(lab.ram('loreRead', 1, False, i)).count('1') for i in range(9))
check(n_read >= 1, f'markers read along the way go in the journal ({n_read} so far)')
accept(1)
check(lab.ram('boxState') and lab.ram('askKind') == 0, 'the Tour Guide wants three stories first')
close_box()
for i in range(9): lab.poke('loreRead', 0xFF, i)   # (as if you'd read every marker in town)
accept(1)
allbut = ((1 << len(CH)) - 1) & ~(1 << CH.index('quiz'))   # every other pin, to see the gold paddle
lab.poke('pins', allbut & 0xFF, 0); lab.poke('pins', allbut >> 8, 1)
for q in range(3):
    for _ in range(40):
        if lab.ram('boxState') == 6: break
        lab.press('A', 6)
    if lab.ram('boxState') != 6: break
    if q == 0: lab.screenshot(f'{out}/61-quiz.png')
    for _ in range(3):
        if lab.ram('askCur') >= lab.ram('quizAns'): break
        lab.press('DOWN', 4)
    lab.press('A', 10)
lab.run(None, 20)
lab.screenshot(f'{out}/62-gold-paddle.png')
check(lab.ram('pins', 2) == (1 << len(CH)) - 1 and lab.ram('goldPaddle'), 'three right answers: the last pin, and the paddle turns gold')
close_box()

# Stage 10, part 2: minigames.
def play_mini(k, step, limit=4000):
    """Accept person k's minigame and run step() each frame until it's over; True if it started."""
    if not accept(k, 3): return False
    for _ in range(30):
        if lab.ram('mgKind') and lab.ram('miniOn'): break
        lab.run(None, 1)
    if not lab.ram('miniOn'): return False
    for f in range(limit):
        if not lab.ram('miniOn'): break
        step(f)
    lab.run(None, 40)
    return True
# Batting practice: the Ballplayer, now that his pin is won. Swing on time.
travel(lab, N("Scott's Addition"), close_box)
coins = lab.ram('coins', 2)
shot = set()
def bat(f):
    if lab.ram('mgBallOn') and not lab.ram('mgRes') and lab.ram('mgAge') + 1 == lab.ram('mgArr'):
        lab.press('A', 1)
        if 'swing' not in shot: lab.run(None, 6); lab.screenshot(f'{out}/63-batting.png'); shot.add('swing')
    else: lab.run(None, 1)
check(play_mini(1, bat) and lab.ram('mgKind') == 1, 'the Ballplayer pitches: batting practice')
check(lab.ram('mgHits') == 10 and lab.ram('mgHr') == 10, f"swing on time: {lab.ram('mgHits')} hits, {lab.ram('mgHr')} home runs")
check(lab.map == N("Scott's Addition") and lab.ram('boxState') and lab.ram('coins', 2) == min(999, coins + 70) and lab.ram('miniBest', 2, False, 1) == 20,
      f"back on the street, 70 coins richer ({coins} -> {lab.ram('coins', 2)})")
lab.screenshot(f'{out}/64-batting-result.png')
close_box()
# Tennis at the Ashe Center: follow the ball, swing as it arrives; then let three go by.
travel(lab, N('Arthur Ashe Boulevard'), close_box)
coins = lab.ram('coins', 2)
def tennis(f):
    if lab.ram('mgRet') >= 5 or not lab.ram('mgBallOn') or lab.ram('mgBallUp'):
        lab.run(None, 1); return
    bx, by, x = lab.ram('mgBx', 2, True) >> 6, lab.ram('mgBy', 2, True) >> 6, lab.ram('mgHx', 2, True) >> 4
    if abs(by - 148) < 6 and abs(bx - x) < 14:
        lab.press('A', 1)
        if 'tennis' not in shot: lab.run(None, 4); lab.screenshot(f'{out}/65-tennis.png'); shot.add('tennis')
    elif bx > x + 4: lab.run('RIGHT', 1)
    elif bx < x - 4: lab.run('LEFT', 1)
    else: lab.run(None, 1)
check(play_mini(2, tennis), 'the Tennis Coach starts the ball machine')
check(lab.ram('mgRet') == 5 and lab.ram('mgMiss') == 3, f"five returns, then three misses ({lab.ram('mgRet')}, {lab.ram('mgMiss')})")
check(lab.ram('coins', 2) == min(999, coins + 5), f"five coins ({coins} -> {lab.ram('coins', 2)})")
close_box()
# Rams free throws in the Siegel Center: shoot when the needle is in the middle.
travel(lab, N('VCU & Court End'), close_box)
walk((7, 7)); lab.run('UP', 30); lab.run(None, 60); close_box()
check(lab.map == N('The Siegel Center'), f'into the Siegel Center (map {lab.map})')
coins = lab.ram('coins', 2)
def hoops(f):
    i = (lab.ram('mgPh', 2) >> 8) & 63
    if not lab.ram('mgBallOn') and i in (0, 32):
        lab.press('A', 1)
        if 'hoops' not in shot: lab.run(None, 20); lab.screenshot(f'{out}/66-free-throw.png'); shot.add('hoops')
    else: lab.run(None, 1)
check(play_mini(0, hoops), 'the Rams Coach hands you the ball')
check(lab.ram('mgMade') >= 8, f"{lab.ram('mgMade')} of 10 from the line")
check(lab.ram('coins', 2) == min(999, coins + 2 * lab.ram('mgMade')), f"two coins a basket ({coins} -> {lab.ram('coins', 2)})")
close_box()
walk((7, 8)); lab.run('DOWN', 30); lab.run(None, 60); close_box()   # back out to VCU

# The scrolling-road races: steer around what's ahead.
def racer(f):
    if f % 2:
        lab.run(None, 1); return
    y, x, n = lab.ram('rcY', 2), lab.ram('rcX', 2, True) >> 4, lab.ram('rcN')
    target = 128
    for i in range(n):
        sy = lab.ram('obsY', 2, False, i) - y
        if 100 < sy < 220 and not lab.ram('obsHit', 1, False, i):
            ox = lab.ram('obsX', 1, False, i)
            target = ox + 34 if ox < 128 else ox - 34
            break
    lab.run('RIGHT' if target > x + 2 else 'LEFT' if target < x - 2 else None, 2)
MEDAL = ['gold', 'silver', 'bronze']
for name, place, k, shotname in (('Derby Kid', 'Church Hill', 2, '67-soapbox-derby'), ('Trail Cyclist', 'Rocketts Landing', 2, '68-capital-trail'),
                                 ('Race Official', 'Richmond Raceway', 0, '69-raceway')):
    travel(lab, N(place), close_box)
    coins = lab.ram('coins', 2)
    shot.discard('race')
    def step(f):
        if f == 240: lab.screenshot(f'{out}/{shotname}.png')
        racer(f)
    started = play_mini(k, step, 6000)
    medal = lab.ram('rcMedal')
    won = {0: 20, 1: 10, 2: 5}.get(medal, 0)
    check(started and lab.ram('rcY', 2) >= lab.ram('rcLen', 2), f"{name}: down the course in {lab.ram('rcTime', 2) / 60:.1f} s, a {MEDAL[medal] if medal < 3 else '?'} medal")
    check(lab.ram('coins', 2) == min(999, coins + won) and lab.ram('miniBest', 2, False, lab.ram('mgKind')) == lab.ram('rcTime', 2),
          f"{won} coins and a best time ({coins} -> {lab.ram('coins', 2)})")
    close_box()

# Stone House pinball: one coin a game. Flip when the ball comes down to a flipper.
travel(lab, N('Forest Hill Park'), close_box)
walk((7, 6)); lab.run('UP', 30); lab.run(None, 60); close_box()
check(lab.map == N('Stone House Arcade'), f'into the Stone House Arcade (map {lab.map})')
coins = lab.ram('coins', 2)
walk((2, 3)); lab.face('up'); lab.press('A', 20)
for _ in range(30):
    if lab.ram('boxState') in (0, 6): break
    lab.press('A', 6)
check(lab.ram('boxState') == 6 and lab.ram('askKind') == 3, 'a pinball machine: one coin a game')
lab.press('A', 10)
for _ in range(30):
    if lab.ram('miniOn') and lab.ram('mgKind') == 7: break
    lab.run(None, 1)
check(lab.ram('miniOn') and lab.ram('coins', 2) == coins - 1, f"in goes the coin ({coins} -> {lab.ram('coins', 2)})")
flips = 0
for f in range(8000):
    if not lab.ram('miniOn'): break
    if f == 400: lab.screenshot(f'{out}/70-pinball.png')
    x, y, vy = lab.ram('pbX', 2, True) >> 6, lab.ram('pbY', 2, True) >> 6, lab.ram('pbVY', 2, True)
    if lab.ram('pbOn') and 160 < y < 184 and vy > 0 and 80 < x < 176:
        lab.run('B' if x < 128 else 'A', 6); flips += 1
    else: lab.run(None, 1)
score = lab.ram('pbScore', 2)
lab.run(None, 60)
check(not lab.ram('miniOn') and lab.ram('pbBalls') == 0 and score > 0, f'three balls: {score} points, {flips} flips')
check(lab.ram('coins', 2) == coins - 1 + score // 500 and lab.ram('boxState'), f"the machine pays a coin per 500 ({lab.ram('coins', 2)} coins)")
close_box()

# Stage 10, part 3: landmark cards, fishing, frogs.
walk((7, 8)); lab.run('DOWN', 30); lab.run(None, 60); close_box()   # out of the arcade
def ncards(): return bin(lab.ram('cards', 2)).count('1')
check(ncards() == 5, f'five landmark cards from the five bosses ({ncards()})')
travel(lab, N('Carytown'), close_box)
walk((6, 17)); lab.run('UP', 30); lab.run(None, 40); close_box()
lab.poke('coins', 200, 0); lab.poke('coins', 0, 1)
walk((8, 5)); lab.face('up'); lab.press('A', 10)
for idx in (5, 6):   # the Fishing Rod, a Landmark Card Pack
    while lab.ram('shopCur') < idx: lab.press('DOWN', 3)
    while lab.ram('shopCur') > idx: lab.press('UP', 3)
    lab.press('A', 60); lab.press('A', 10)
lab.press('B', 10); close_box()
check(lab.ram('hasRod') and ncards() == 6, f'Carytown Goods sells a Fishing Rod and landmark card packs ({ncards()} cards)')
walk((7, 8)); lab.run('DOWN', 30); lab.run(None, 60); close_box()
# the cards in the START menu
lab.press('START', 10)
items = [lab.ram('menuList', 1, False, i) for i in range(lab.ram('menuN'))]
for _ in range(12):
    if lab.ram('menuCur') >= len(items) - 1: break
    lab.press('DOWN', 4)
lab.press('A', 6)
check(lab.ram('boxState') == 8, 'START, then Landmark Cards: the collection')
for _ in range(12):
    if lab.ram('cards', 2) & (1 << lab.ram('cardCur')): break
    lab.press('DOWN', 4)
lab.press('A', 30)
lab.screenshot(f'{out}/71-landmark-card.png')
check(lab.ram('boxState') not in (0, 8), 'A reads the card')
close_box(); lab.press('B', 6); close_box()
# Fishing in Byrd Park: find the water's edge, cast, wait for the bite, then reel without snapping the line
travel(lab, N('Byrd Park & Maymont'), close_box)
menu_pick(11)
g = lab.solid_grid()
wram = None
def cell(x, y):
    return lab.ram('colRam', 1, False, y * 32 + x)
spots = [(x, y, d) for y in range(1, 21) for x in range(1, 31) for d, (dx, dy) in (('up', (0, -1)), ('down', (0, 1)), ('left', (-1, 0)), ('right', (1, 0)))
         if not g[y][x] and cell(x + dx, y + dy) == 12]
px0, py0 = lab.pos
spots.sort(key=lambda q: abs(q[0] * 16 + 8 - px0) + abs(q[1] * 16 + 8 - py0))
caught = 0
for x, y, d in spots[:6]:
    try: walk((x, y))
    except RuntimeError: continue
    lab.face(d)
    for _ in range(3):   # a few casts: clear the bait-stealers
        close_box()
        lab.press('B', 4)
        if lab.ram('boxState') != 7: break
        for f in range(900):
            ph = lab.ram('fishPh')
            if lab.ram('boxState') != 7: break
            if ph == 1: lab.press('A', 1)
            elif ph == 2 and lab.ram('fishTen', 2) < 60 * 256: lab.press('A', 2)
            else: lab.run(None, 1)
            if f == 300: lab.screenshot(f'{out}/72-fishing.png')
        caught = sum(lab.ram('fishN', 1, False, k) for k in range(5))
        if caught: break
    if caught: break
lab.screenshot(f'{out}/73-a-catch.png')
check(caught >= 1, f"cast into the lake and reel one in ({[lab.ram('fishN', 1, False, k) for k in range(5)]})")
close_box()
# Frogs: out near the water when it rains
lab.poke('wx', 1); lab.poke('wxK', 255); lab.poke('wxNext', 255)
got = 0
for t in range(60):
    lab.run(None, 30)
    for k in range(10):
        if lab.ram('pkKind', 1, False, k) == 17:
            back = lab.pos
            poke16('px', lab.ram('pkX', 2, True, k)); poke16('py', lab.ram('pkY', 2, True, k)); lab.run(None, 6)
            poke16('px', back[0]); poke16('py', back[1]); lab.run(None, 2)
            if lab.ram('boxState'):
                if lab.ram('frogs') == 1: lab.screenshot(f'{out}/74-a-frog.png')
                close_box()
    if lab.ram('frogs') >= 5: break
check(lab.ram('frogs') >= 5, f"frogs come out in the rain: {lab.ram('frogs')} caught")
coins, before = lab.ram('coins', 2), ncards()
for _ in range(3):
    talk_to(2)
    for _ in range(20):
        if not lab.ram('boxState'): break
        lab.press('A', 6)
    if lab.ram('frogsDone'): break
check(lab.ram('frogsDone') and lab.ram('coins', 2) == min(999, coins + 40) and ncards() == before + 1,
      f"the naturalist counts them: 40 coins and the Maymont card ({ncards()} cards)")
lab.poke('wx', 0); lab.poke('wxK', 0)
sys.exit(0 if ok else 1)
