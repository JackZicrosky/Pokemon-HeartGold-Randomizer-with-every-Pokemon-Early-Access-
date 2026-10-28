# proof.py [MATCHES.json] : for every owner picture (matches.json: picture -> capture name), the full-screen proof:
#   situation: pixels where the Default capture differs from the owner's Default screenshot (0 = the same situation
#              and frame; anything else means the capture is not the owner's moment yet)
#   AMOLED:    pixels where the AMOLED capture differs from the owner's AMOLED picture, all 98304 pixels, colours as
#              the DS has them (5 bits per channel); accuracy = 1 - wrong / 98304 (target >= 99.99%: <= 9 pixels)
# Writes proof/NAME.png for pictures not yet proven: owner picture | AMOLED capture | wrong pixels in red.
import json, os, sys
from PIL import Image, ImageChops
A = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'
D = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
M = json.load(open(sys.argv[1] if len(sys.argv) > 1 else 'matches.json'))
os.makedirs('proof', exist_ok=True)
def o1(v): return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))
def q5own(im): return im.point(lambda v: o1(v) << 3)
def q5(im): return im.point(lambda v: (v >> 3) << 3)
def ndiff(a, b):
    r, g, bb = ImageChops.difference(a, b).split()
    m = ImageChops.lighter(ImageChops.lighter(r, g), bb).point(lambda v: 255 if v else 0)
    return m.histogram()[255], m
rows = []
for name in sorted(M):
    cap = M[name]
    fd, fa = 'caps/def_%s.png' % cap, 'caps/am_%s.png' % cap
    if not (os.path.exists(fd) and os.path.exists(fa)):
        rows.append((name, cap, None, None)); continue
    sit, _ = ndiff(q5(Image.open(D + name).convert('RGB')), q5(Image.open(fd).convert('RGB')))
    own = q5own(Image.open(A + name).convert('RGB')); got = q5(Image.open(fa).convert('RGB'))
    wrong, m = ndiff(own, got)
    rows.append((name, cap, sit, wrong))
    if wrong > 9 or sit:
        im = Image.new('RGB', (768, 384)); im.paste(own, (0, 0)); im.paste(got, (256, 0))
        red = got.copy(); red.paste((255, 0, 0), mask=m); im.paste(red, (512, 0)); im.save('proof/%s' % name)
ok = 0
for name, cap, sit, wrong in rows:
    if sit is None:
        print('%-40s %-14s no capture' % (name, cap)); continue
    acc = 100 * (1 - wrong / 98304)
    good = sit == 0 and wrong <= 9
    ok += good
    print('%-40s %-14s situation %6d px  AMOLED wrong %6d px  %8.4f%%  %s' % (name, cap, sit, wrong, acc, 'PROVEN' if good else ''))
print('proven %d of %d' % (ok, len(rows)))
