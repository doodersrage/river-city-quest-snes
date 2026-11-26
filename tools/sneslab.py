"""Drive the ROM in the snes9x core for tests: read game variables from WRAM by symbol,
path-find over the collision grid, steer the hero, press buttons, take screenshots.

    lab = Lab('RiverCityQuest.sfc', 'RiverCityQuest.sym', repo_root)
    lab.run(None, 120)
    lab.walk_to_cell(7, 9); lab.face('down'); lab.press('A')
"""
import os
from collections import deque
import numpy as np
import stable_retro as retro
from PIL import Image

ORDER = ['B', 'Y', 'SELECT', 'START', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'X', 'L', 'R']
DIRS = {'down': 'DOWN', 'up': 'UP', 'left': 'LEFT', 'right': 'RIGHT'}
MAP_W, MAP_H, HALF = 32, 22, 5


class Lab:
    def __init__(self, rom, sym, root):
        self.emu = retro.RetroEmulator(rom)
        self.data = retro.data.GameData()
        self.emu.configure_data(self.data)
        self.root = root
        self.sym = {}
        for line in open(sym):
            parts = line.split()
            if len(parts) == 2 and not parts[0].startswith('['):
                try:
                    self.sym[parts[1]] = int(parts[0], 16)
                except ValueError:
                    pass

    # ---------- memory ----------
    def ram(self, name, size=1, signed=False, index=0):
        self.data.update_ram()
        addr = self.sym[name] + index * size
        wram = self.data.memory.blocks[0x7E0000]
        off = addr - 0x7E0000
        v = int(wram[off]) | (int(wram[off + 1]) << 8 if size == 2 else 0)
        if signed and size == 2 and v >= 0x8000:
            v -= 0x10000
        return v

    @property
    def pos(self):
        return self.ram('px', 2, True), self.ram('py', 2, True)

    @property
    def map(self):
        return self.ram('curMap')

    # ---------- input ----------
    def run(self, buttons, frames):
        mask = np.zeros(12, dtype=np.uint8)
        for b in (buttons or '').split('+'):
            if b:
                mask[ORDER.index(b.upper())] = 1
        for _ in range(frames):
            self.emu.set_button_mask(mask, 0)
            self.emu.step()

    def press(self, button, settle=2):
        self.run(button, 1)
        self.run(None, settle)

    def screenshot(self, path):
        Image.fromarray(self.emu.get_screen()).save(path)

    # ---------- movement ----------
    def map_named(self, name):
        """Map index by its name, from the comments in the generated maps.h."""
        import re
        for line in open(os.path.join(self.root, 'maps.h')):
            m = re.match(r'extern char m(\d+)_chr.*/\* (.*) \*/', line)
            if m and m.group(2) == name:
                return int(m.group(1))
        raise KeyError(name)

    def solid_grid(self):
        """The live collision grid (colRam), so opened doors and cut bushes count."""
        self.data.update_ram()
        wram = self.data.memory.blocks[0x7E0000]
        off = self.sym['colRam'] - 0x7E0000
        col = [int(wram[off + i]) for i in range(MAP_W * MAP_H)]
        w, h = self.ram('mapW'), self.ram('mapH')
        # doors and stairs (2, 3) count as walls for routing, so a route never wanders out of the map
        walls = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11} | (set() if self.ram('hasKayak') else {10})
        grid = [[1 if (x >= w or y >= h or col[y * MAP_W + x] in walls) else 0
                 for x in range(MAP_W)] for y in range(MAP_H)]
        for k in range(self.ram('blkN')):
            grid[self.ram('blkTY', 1, False, k)][self.ram('blkTX', 1, False, k)] = 1
        for k in range(self.ram('npcCount')):
            nx, ny = self.ram('npcX', 2, True, k), self.ram('npcY', 2, True, k)
            grid[ny // 16][nx // 16] = 1
        return grid

    def path(self, goal):
        grid = self.solid_grid()
        grid[goal[1]][goal[0]] = 0
        x, y = self.pos
        start = (x // 16, y // 16)
        prev, q = {start: None}, deque([start])
        while q:
            c = q.popleft()
            if c == goal:
                break
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (c[0] + dx, c[1] + dy)
                if 0 <= n[0] < MAP_W and 0 <= n[1] < MAP_H and n not in prev and not grid[n[1]][n[0]]:
                    prev[n] = c
                    q.append(n)
        if goal not in prev:
            raise RuntimeError(f'no path from {start} to {goal}')
        out, c = [], goal
        while c:
            out.append(c)
            c = prev[c]
        return out[::-1]

    def steer_to(self, tx, ty, limit=400):
        """Walk to pixel (tx, ty), one axis at a time, using the live position."""
        prefer_x = True
        for _ in range(limit):
            x, y = self.pos
            if abs(x - tx) <= 1 and abs(y - ty) <= 1:
                return True
            moves = []
            if abs(x - tx) > 1: moves.append('RIGHT' if tx > x else 'LEFT')
            if abs(y - ty) > 1: moves.append('DOWN' if ty > y else 'UP')
            if not prefer_x: moves.reverse()
            self.run(moves[0], 1)
            if self.pos == (x, y):          # that axis is blocked: try the other one next time
                prefer_x = not prefer_x
        return False

    def walk_to_cell(self, cx, cy):
        for c in self.path((cx, cy))[1:]:
            if not self.steer_to(c[0] * 16 + 8, c[1] * 16 + 8):
                raise RuntimeError(f'stuck near {self.pos} heading for cell {c}')
        self.run(None, 2)

    def face(self, d):
        """Turn without moving: tap the direction for one frame, then step back if it moved."""
        x, y = self.pos
        self.run(DIRS[d], 1)
        self.steer_to(x, y, 20)
        self.run(DIRS[d], 1)
        self.run(None, 1)

    # ---------- combat ----------
    def enemy(self, k):
        return {'x': self.ram('enX', 2, True, k) >> 4, 'y': self.ram('enY', 2, True, k) >> 4,
                'alive': self.ram('enAlive', 1, False, k), 'type': self.ram('enType', 1, False, k),
                'hp': self.ram('enHp', 1, False, k)}

    def poke(self, name, value, index=0):
        """Write one byte of a game variable (for tests that need a set-up state)."""
        self.data.memory.assign(self.sym[name] + index, '|u1', value)

    def _exit_ahead(self, d):
        """Would a step in direction d put the hero's feet on a door or an exit?"""
        x, y = self.pos
        dx, dy = {'UP': (0, -6), 'DOWN': (0, 6), 'LEFT': (-6, 0), 'RIGHT': (6, 0)}[d]
        if (x + dx * 2 < 0 or y + dy * 2 < 0 or x + dx * 2 >= self.ram('mapW') * 16 or y + dy * 2 >= self.ram('mapH') * 16):
            return True   # keep well clear of the room's open edges
        nx, ny = x + dx, y + dy
        w, h = self.ram('mapW'), self.ram('mapH')
        if nx < 0 or ny < 0 or nx >= w * 16 or ny >= h * 16:
            return True
        return self.ram('colRam', 1, False, (ny // 16) * MAP_W + nx // 16) in (2, 3)

    def run_safe(self, d, frames):
        """Run in direction d unless that walks out of the room."""
        for _ in range(frames):
            if self._exit_ahead(d):
                return
            self.run(d, 1)

    def fight(self, k, limit=1500):
        """Chase enemy k and swing at it until it is gone. Returns frames used, or None.
        Never steps onto a door, an exit or off the map."""
        for f in range(limit):
            e = self.enemy(k)
            if not e['alive']:
                return f
            x, y = self.pos
            dx, dy = e['x'] - x, e['y'] - y
            if abs(dx) + abs(dy) < 20 and (abs(dx) < 8 or abs(dy) < 8):
                d = ('RIGHT' if dx > 0 else 'LEFT') if abs(dx) >= abs(dy) else ('DOWN' if dy > 0 else 'UP')
                self.run_safe(d, 1)
                self.press('A', 15)
            else:
                if abs(dx) > abs(dy) and abs(dy) > 3:
                    order = ['DOWN' if dy > 0 else 'UP', 'RIGHT' if dx > 0 else 'LEFT']
                elif abs(dx) >= 8:
                    order = ['RIGHT' if dx > 0 else 'LEFT', 'DOWN' if dy > 0 else 'UP']
                else:
                    order = ['DOWN' if dy > 0 else 'UP', 'RIGHT' if dx > 0 else 'LEFT']
                self.run_safe(order[0], 1)
                if self.pos == (x, y):
                    # blocked: sidestep a whole half-cell, whichever way is open
                    side = [order[1]] + [d for d in ('UP', 'DOWN', 'LEFT', 'RIGHT') if d != order[1] and d != order[0]]
                    for d in side:
                        before = self.pos
                        self.run_safe(d, 8)
                        if self.pos != before:
                            break
        return None


# ---------- getting around the overworld ----------

def _neighbours(root):
    import re
    h = open(os.path.join(root, 'maps.h')).read()
    return {d: [int(v) for v in re.search(r'nb%s\[MAP_COUNT\] = \{([^}]*)\}' % d, h).group(1).split(',')] for d in 'NSEW'}


def _edge_open(root, m, d):
    """Whether map m has a walkable cell on its d edge (outdoor maps are 32x22)."""
    col = open(os.path.join(root, 'data', f'm{m}.col'), 'rb').read()
    cells = [col[(0 if d == 'N' else 21) * 32 + x] for x in range(32)] if d in 'NS' else \
            [col[y * 32 + (0 if d == 'W' else 31)] for y in range(22)]
    return any(c in (0, 10) for c in cells)


def route(lab, dest):
    """The edges to take (N, S, E, W) from the current map to dest, breadth first."""
    from collections import deque
    nb = _neighbours(lab.root)
    prev, q = {lab.map: None}, deque([lab.map])
    while q:
        m = q.popleft()
        for d in 'NSEW':
            n = nb[d][m]
            if n != 255 and n not in prev and _edge_open(lab.root, m, d):
                prev[n] = (m, d)
                q.append(n)
    out, m = [], dest
    while prev[m]:
        m, d = prev[m]
        out.append(d)
    return out[::-1]


def leave(lab, d, close_box):
    """Walk off the map's d edge through the open cell nearest the middle."""
    g, w, h = lab.solid_grid(), lab.ram('mapW'), lab.ram('mapH')
    if d in 'NS':
        y = 0 if d == 'N' else h - 1
        cells, key = [(x, y) for x in range(w) if not g[y][x]], 'UP' if d == 'N' else 'DOWN'
    else:
        x = 0 if d == 'W' else w - 1
        cells, key = [(x, y) for y in range(h) if not g[y][x]], 'LEFT' if d == 'W' else 'RIGHT'
    cells.sort(key=lambda c: abs(c[0] - w // 2) + abs(c[1] - h // 2))
    m0 = lab.map
    want = _neighbours(lab.root)[d][m0]
    for c in cells:
        try:
            lab.walk_to_cell(*c)
        except RuntimeError:
            pass
        if lab.map == m0:
            lab.run(key, 30)
            lab.run(None, 40)
        close_box()
        if lab.map == want:
            return True
        if lab.map != m0:
            # stumbled through a door on the way: walk back out the way we came
            for back in ('DOWN', 'UP', 'LEFT', 'RIGHT'):
                lab.run(back, 40)
                lab.run(None, 40)
                close_box()
                if lab.map == m0:
                    break
            if lab.map != m0:
                return False
    return False


def travel(lab, dest, close_box):
    for d in route(lab, dest):
        if not leave(lab, d, close_box):
            raise RuntimeError(f'stuck leaving map {lab.map} {d}')
