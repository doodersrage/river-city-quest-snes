import sys
rom=bytearray(open(sys.argv[1],'rb').read()); sym={}
for l in open(sys.argv[2]):
    p=l.split()
    if len(p)==2 and not p[0].startswith('['):
        try: sym[p[1]]=int(p[0],16)
        except: pass
for name in sys.argv[4:]:
    a=sym[name]; off=((a>>16)&0x7F)*0x8000+(a&0x7FFF); rom[off]=0x6B
open(sys.argv[3],'wb').write(rom)
