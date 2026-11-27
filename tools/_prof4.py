import sys
from sneslab import Lab
lab=Lab(sys.argv[1],sys.argv[2],'..')
lab.run(None,150); lab.press('A',60); lab.press('START',120)
for _ in range(40):
    if not lab.ram('boxState'): break
    lab.press('A',6)
lab.run(None,30)
base=lab.emu.get_state()
for k in ['LEFT','RIGHT','UP','DOWN']:
    lab.emu.set_state(base)
    a=lab.ram('frameCount',2); lab.run(k,60); print(k, (lab.ram('frameCount',2)-a)&0xFFFF, lab.pos)
a=lab.ram('frameCount',2); lab.run(None,60); print('idle', (lab.ram('frameCount',2)-a)&0xFFFF)
