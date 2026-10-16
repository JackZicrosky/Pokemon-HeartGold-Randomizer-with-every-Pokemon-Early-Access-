# every owner picture vs its AMOLED capture (pixels where the Default capture shows the Default screenshot's frame)
import json, os
from PIL import Image
A='D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'; D='D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
def o1(v): return v>>3 if v%8==0 else min(31,round(v*31/255))
M=json.load(open('matches.json')); tw=tc=0
for n in sorted(M):
    c=M[n]
    if not os.path.exists('caps/am_'+c+'.png'): print(n,'no am capture'); continue
    O=Image.open(A+n).convert('RGB').load(); Dd=Image.open(D+n).convert('RGB').load()
    C=Image.open('caps/def_'+c+'.png').convert('RGB').load(); Mm=Image.open('caps/am_'+c+'.png').convert('RGB').load()
    w=cmp=0
    for y in range(384):
        for x in range(256):
            if Dd[x,y]!=C[x,y]: continue
            cmp+=1
            if tuple(o1(v) for v in O[x,y])!=tuple(v>>3 for v in Mm[x,y]): w+=1
    tw+=w; tc+=cmp
    print('%-40s %-16s wrong %6d / %6d' % (n,c,w,cmp))
print('TOTAL wrong',tw,'of',tc, '%.3f%%' % (100*tw/max(1,tc)))
