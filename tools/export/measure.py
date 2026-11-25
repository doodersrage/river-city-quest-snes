import json,sys
from PIL import Image
d=sys.argv[1];m=json.load(open(d+'/maps.json'));rows=[]
for e in m:
    im=Image.open(d+'/'+e['file']).convert('RGB');W,H=im.size
    t8=set()
    for ty in range(0,H,8):
        for tx in range(0,W,8):
            t8.add(im.crop((tx,ty,tx+8,ty+8)).tobytes())
    rows.append((len(t8),e['id']))
rows.sort(reverse=True);v=[r[0] for r in rows]
print('max',rows[:4],'median',sorted(v)[len(v)//2],'over1024',sum(x>1024 for x in v),'over768',sum(x>768 for x in v))
