# proof2.py [MATCHES.json] : the strict proof over recordings: for every owner picture (matches.json: picture ->
# capture name), rec/def_px_NAME and rec/am_px_NAME hold 60 consecutive frames at that screen (proofcaps.sh).
#   situation: the fewest pixels any Default frame differs from the owner's Default screenshot (0 = that exact moment)
#   AMOLED:    the fewest pixels any AMOLED frame differs from the owner's AMOLED picture (all 98304 pixels, 5-bit
#              colours); proven when situation 0 and AMOLED <= 9 (99.99%)
# Writes proof/NAME.png for pictures not proven: owner picture | best AMOLED frame | its wrong pixels in red.
import json, os, sys, glob
from PIL import Image, ImageChops
A = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'
D = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
M = json.load(open(sys.argv[1] if len(sys.argv) > 1 else 'matches.json'))
os.makedirs('proof', exist_ok=True)
def o1(v): return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))
def q5(im): return im.point(lambda v: (v >> 3) << 3)
def ndiff(a, b):
    r, g, bb = ImageChops.difference(a, b).split()
    m = ImageChops.lighter(ImageChops.lighter(r, g), bb).point(lambda v: 255 if v else 0)
    return m.histogram()[255], m
def frames(d):
    return sorted(glob.glob(d + '/f*.png'))
ok = 0
for name in sorted(M):
    cap = M[name]
    dd, ad = 'rec/def_px_%s' % cap, 'rec/am_px_%s' % cap
    if not (frames(dd) and frames(ad)):
        print('%-40s %-14s no recording' % (name, cap)); continue
    dref = q5(Image.open(D + name).convert('RGB')); own = Image.open(A + name).convert('RGB').point(lambda v: o1(v) << 3)
    sit = min(ndiff(dref, q5(Image.open(f).convert('RGB')))[0] for f in frames(dd))
    best = None
    for f in frames(ad):
        w, m = ndiff(own, q5(Image.open(f).convert('RGB')))
        if best is None or w < best[0]:
            best = (w, m, f)
    wrong, m, f = best
    good = sit == 0 and wrong <= 9
    ok += good
    if not good:
        got = q5(Image.open(f).convert('RGB'))
        im = Image.new('RGB', (768, 384)); im.paste(own, (0, 0)); im.paste(got, (256, 0))
        red = got.copy(); red.paste((255, 0, 0), mask=m); im.paste(red, (512, 0)); im.save('proof/%s' % name)
    print('%-40s %-14s situation %6d px  AMOLED wrong %6d px  %8.4f%%  %s' % (name, cap, sit, wrong,
          100 * (1 - wrong / 98304), 'PROVEN' if good else ''))
print('proven %d of %d' % (ok, len(M)))
