import sys
from sneslab import Lab
lab=Lab(sys.argv[1],sys.argv[2],'..')
lab.run(None,150); lab.press('A',60); lab.press('START',120)
for _ in range(40):
    if not lab.ram('boxState'): break
    lab.press('A',6)
lab.run(None,30)
lab.walk_to_cell(16,12)
ims=[]
for d in ['down','right','up','left']:
    lab.face(d); lab.run(None,4)
    lab.press('A',4); lab.run(None,0) if False else None
    lab.screenshot(f'/tmp/sw_{d}.png')
    lab.run(None,30)
