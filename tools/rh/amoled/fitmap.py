# fitmap.py CAP HALF : where dsrender disagrees with the colour-id capture (scratch: fit_CAP.png), by layer
import sys, collections
sys.path.insert(0, '.')
import dsrender as d
from PIL import Image
sc, half = sys.argv[1], int(sys.argv[2])
b = open('caps/id_%s.vram2' % sc, 'rb').read(); shot = Image.open('caps/id_%s.png' % sc).convert('RGB')
scr, fit = d.screens(b, shot)
bad = Image.new('RGB', (256, 192)); c = collections.Counter()
for y in range(192):
    for x in range(256):
        p = scr[half][y][x]
        if p is None: continue
        if d.shown(b, p) != tuple(v >> 3 for v in shot.getpixel((x, y + 192 * half))):
            bad.putpixel((x, y), (255, 0, 0)); c[(p['kind'], p.get('layer'), p['idx'] // 16, str(p['effect'])[:20])] += 1
print(fit, c.most_common(8))
for eng in (0, 1):
    io = d.O_IOM if eng == 0 else d.O_IOS
    print('eng', eng, 'disp %08x' % d.u32(b, io), 'bgcnt', ['%04x' % d.u16(b, io + 8 + 2 * k) for k in range(4)],
          'bld %04x %04x %d' % (d.u16(b, io + 0x50), d.u16(b, io + 0x52), d.u16(b, io + 0x54)), 'win %04x %04x' % (d.u16(b, io + 0x48), d.u16(b, io + 0x4A)))
o = Image.new('RGB', (512, 192)); o.paste(shot.crop((0, 192 * half, 256, 192 * half + 192)), (0, 0)); o.paste(bad, (256, 0))
o.resize((1024, 384)).save(sys.argv[3] if len(sys.argv) > 3 else 'fit_%s.png' % sc)
