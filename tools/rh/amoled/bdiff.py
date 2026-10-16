# bdiff.py OWNERNUM_name CAP [out.png]: pixels where def capture == Default screenshot; am capture vs owner (after DS 5-bit rounding)
import sys
from PIL import Image
A='D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'; D='D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
def o1(v): return v>>3 if v%8==0 else min(31,round(v*31/255))
o,c=sys.argv[1],sys.argv[2]
O=Image.open(A+o+'.png').convert('RGB'); Dd=Image.open(D+o+'.png').convert('RGB')
C=Image.open('caps/def_'+c+'.png').convert('RGB'); M=Image.open('caps/am_'+c+'.png').convert('RGB')
out=Image.new('RGB',(512,384)); bad=same=0; bbad=0
for y in range(384):
    for x in range(256):
        d,cc,m,ow=Dd.getpixel((x,y)),C.getpixel((x,y)),M.getpixel((x,y)),O.getpixel((x,y))
        out.putpixel((x,y),ow); 
        if d!=cc: out.putpixel((256+x,y),(0,0,90)); continue
        same+=1
        if tuple(o1(v) for v in ow)!=tuple(v>>3 for v in m): bad+=1; bbad+=y>=192; out.putpixel((256+x,y),(255,0,255))
        else: out.putpixel((256+x,y),m)
print(o,c,'compared',same,'wrong',bad,'bottom wrong',bbad)
if len(sys.argv)>3: out.save(sys.argv[3])
