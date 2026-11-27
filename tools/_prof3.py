import sys
from sneslab import Lab
lab=Lab(sys.argv[1],sys.argv[2],'..')
lab.run(None,150); lab.press('A',60); lab.press('START',120)
for _ in range(40):
    if not lab.ram('boxState'): break
    lab.press('A',6)
lab.run(None,30)
lab.walk_to_cell(16,10)
lab.run(None,2)
rows=[]
for f in range(120):
    lab.run('LEFT' if f<60 else 'RIGHT',1)
    rows.append((lab.ram('frameCount',2), lab.ram('sL',2), lab.ram('camX',2), lab.ram('px',2)))
for i in range(1,len(rows)):
    print(i, rows[i][0]-rows[i-1][0], rows[i][1:])
