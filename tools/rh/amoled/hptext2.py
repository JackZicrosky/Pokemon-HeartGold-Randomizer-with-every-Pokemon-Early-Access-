# hptext2.py CAP... : the HP-box text rule against the box art (narc 8 gauge NCGRs): in each HP-box sprite, a pixel is
# text (name, gender, level, HP numbers) when it shows entry 14 or 2 and the art there has neither. Each sprite is
# matched to the art half (file, 0/2048) with the most equal tiles. Writes caps/hptext2_CAP.png (text white/grey,
# other 14 red, other 2 blue) and prints the matched art.
import struct, sys, narc, hptext
from PIL import Image
GAUGES = [188, 191, 194, 197, 200, 203, 253]
ms = narc.members(narc.ROOT + 'a/0/0/8')
ART = {}
for f in GAUGES:
    b = ms[f]
    ART[(f, 0)] = b[48:48 + 2048]; ART[(f, 1)] = b[48 + 2048:48 + 4096]
def nib(buf, x, y):
    t = (y // 8) * 8 + x // 8; p = (y % 8) * 8 + x % 8; v = buf[t * 32 + p // 2]
    return (v >> 4) if p & 1 else v & 15
for cap in sys.argv[1:]:
    v = open('caps/%s.vram2' % cap, 'rb').read()
    disp = struct.unpack_from('<I', v, 0)[0]; sh = 5 + (disp >> 20 & 3)
    bx = hptext.boxes(v)
    im = Image.new('RGB', (64 * max(1, len(bx)), 64))
    for k, (i, px) in enumerate(bx):
        a2 = struct.unpack_from('<H', v, hptext.O_OAM + i * 8 + 4)[0]; raw = v[hptext.O_OBJM + ((a2 & 1023) << sh):][:2048]
        key = max(ART, key=lambda a: sum(1 for t in range(64) if raw[t * 32:t * 32 + 32] == ART[a][t * 32:t * 32 + 32]))
        same = sum(1 for t in range(64) if raw[t * 32:t * 32 + 32] == ART[key][t * 32:t * 32 + 32])
        print(cap, 'oam', i, 'art', key, 'equal tiles', same)
        for y in range(64):
            for x in range(64):
                n = px[y][x]; a = nib(ART[key], x, y)
                c = (40, 40, 40) if n == 15 else (0, 0, 0) if n == 0 else (110, 90, 30)
                if n in (14, 2):
                    text = a not in (14, 2)
                    c = ((255, 255, 255) if n == 14 else (150, 150, 150)) if text else ((255, 0, 0) if n == 14 else (0, 0, 255))
                im.putpixel((k * 64 + x, y), c)
    im.resize((im.width * 4, 256), Image.NEAREST).save('caps/hptext2_%s.png' % cap)
