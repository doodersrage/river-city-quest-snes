import sys
from sneslab import Lab
lab=Lab(sys.argv[1],sys.argv[2],'..')
lab.run(None,150); lab.press('A',60); lab.press('START',120)
for _ in range(40):
    if not lab.ram('boxState'): break
    lab.press('A',6)
lab.run(None,30)
lab.walk_to_cell(16,10)
base = lab.emu.get_state()
def go(pokes, keys):
    lab.emu.set_state(base); lab.run(None,2)
    a=lab.ram('frameCount',2)
    for k in keys:
        for _ in range(40):
            for n,v in pokes: lab.poke(n,v)
            lab.run(k,1)
    return (lab.ram('frameCount',2)-a)&0xFFFF
K=['LEFT','RIGHT']*3
print('walk', go([],K))
print('walk sLive=0', go([('sLive',0)],K))
print('hold B (no move?)', go([],['B']*6))
lab.emu.set_state(base)
for _ in range(200): lab.run('UP',1)
print('pos after up', lab.pos)
s2=lab.emu.get_state()
a=lab.ram('frameCount',2); lab.run('UP',240); print('push wall', (lab.ram('frameCount',2)-a)&0xFFFF, lab.pos)
lab.emu.set_state(base)
g=lab.solid_grid(); x,y=lab.pos; cx,cy=x//16,y//16
cand=[(c,r) for r in range(1,len(g)) for c in range(len(g[0])) if not g[r][c] and g[r-1][c]]
c=min(cand,key=lambda t:abs(t[0]-cx)+abs(t[1]-cy))
lab.walk_to_cell(*c); lab.run('UP',30)
p0=lab.pos
a=lab.ram('frameCount',2); lab.run('UP',240); print('push wall', (lab.ram('frameCount',2)-a)&0xFFFF, p0, lab.pos)
