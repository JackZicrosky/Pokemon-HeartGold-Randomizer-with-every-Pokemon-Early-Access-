# why.py OWNER CAP : wrong pixels grouped by palette entry (engine-relative): wanted owner colour vs shown
import sys, collections, dsrender as R
from PIL import Image
A='D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'; D='D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
def o1(v): return v>>3 if v%8==0 else min(31,round(v*31/255))
o,c=sys.argv[1],sys.argv[2]
O=Image.open(A+o+'.png').convert('RGB'); Dd=Image.open(D+o+'.png').convert('RGB'); C=Image.open('caps/def_'+c+'.png').convert('RGB')
shot=Image.open('caps/am_'+c+'.png').convert('RGB')
b=open('caps/am_'+c+'.vram2','rb').read(); sc,h=R.screens(b,shot)
g=collections.defaultdict(collections.Counter); ex={}
for y in range(int(sys.argv[3]) if len(sys.argv)>3 else 0,384):
  for x in range(256):
    if Dd.getpixel((x,y))!=C.getpixel((x,y)): continue
    w=tuple(o1(t) for t in O.getpixel((x,y))); s=tuple(t>>3 for t in shot.getpixel((x,y)))
    if w==s: continue
    p=sc[y//192][y%192][x]
    k=('none' if not p else '%s%d.%d.%d'%(p['kind'],p.get('layer',9),p['idx']//16,p['idx']%16))
    g[k][(w,s)]+=1; ex.setdefault(k,(x,y))
for k in sorted(g,key=lambda k:-sum(g[k].values())): print(k,sum(g[k].values()),ex[k],g[k].most_common(3))
