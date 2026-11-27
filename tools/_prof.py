import sys
from sneslab import Lab
lab=Lab(sys.argv[1],sys.argv[2],'..')
lab.run(None,150); lab.press('A',60); lab.press('START',120)
for _ in range(40):
    if not lab.ram('boxState'): break
    lab.press('A',6)
lab.run(None,30)
lab.walk_to_cell(16,10)   # on the cross street
base = lab.emu.get_state()
def walk():
    lab.emu.set_state(base); lab.run(None,2)
    a=lab.ram('frameCount',2); 
    for _ in range(3): lab.run('LEFT',40); lab.run('RIGHT',40)
    b=lab.ram('frameCount',2); return (b-a)&0xFFFF
print('normal          ', walk(), '/ 240')
for name,pokes in [('no people',[('npcCount',0)]),('no wanderers',[('nWander',0)]),('no cars',[('carAny',0)]),('no enemies',[('enCount',0)])]:
    def w2():
        lab.emu.set_state(base); lab.run(None,1)
        for n,v in pokes: lab.poke(n,v)
        lab.run(None,1)
        a=lab.ram('frameCount',2)
        for _ in range(3): 
            for n,v in pokes: lab.poke(n,v)
            lab.run('LEFT',40); lab.run('RIGHT',40)
        return (lab.ram('frameCount',2)-a)&0xFFFF
    print(f'{name:16}', w2(), '/ 240')
lab.emu.set_state(base); lab.run(None,2)
a=lab.ram('frameCount',2); lab.run(None,240); print('standing', (lab.ram('frameCount',2)-a)&0xFFFF)
lab.emu.set_state(base); lab.run(None,2)
a=lab.ram('frameCount',2); lab.run('UP',30); lab.run('DOWN',30); lab.run('UP',30); lab.run('DOWN',30); print('updown /120', (lab.ram('frameCount',2)-a)&0xFFFF)
