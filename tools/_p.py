import sys
from sneslab import Lab
lab = Lab(sys.argv[1], sys.argv[2], '.')
lab.run(None, 150)
st = lab.emu.get_state()
def rate(btn=None, n=120):
    f0 = lab.ram('frameCount', 2); lab.run(btn, n); return (lab.ram('frameCount', 2) - f0) / n
def setc(v): lab.poke('dayClock', v & 255, 0); lab.poke('dayClock', v >> 8, 1)
for name, pokes in [('all', []), ('nocar', [('carAny',0),('carOn',0)]), ('nobird', [('birdSt',255)]), ('nowander', [('nWander',0)]),
                    ('none', [('carAny',0),('carOn',0),('birdSt',255),('nWander',0)])]:
    lab.emu.set_state(st)
    for n,v in pokes: lab.poke(n, v)
    a = rate(); b = rate('UP', 60); c = rate('DOWN', 60)
    setc(16*1500); lab.poke('wx', 1); lab.poke('wxK', 255); lab.poke('wxNext', 255); lab.run(None, 60)
    print(name, 'day %.2f %.2f %.2f' % (a, b, c), 'night %.2f %.2f %.2f' % (rate(), rate('UP', 60), rate('DOWN', 60)), lab.pos)
