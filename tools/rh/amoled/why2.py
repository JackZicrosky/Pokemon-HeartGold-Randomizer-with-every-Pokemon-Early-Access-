# why2.py [PICTURE...] : for each owner picture (matches.json), every pixel its best AMOLED proof frame
# (rec/am_px_CAP, proof2.py) gets wrong, by cause (pixels where the Default capture shows the owner's moment only):
#   wrong   : the pixel's palette entry has one owner colour on this screen, the game shows another
#   shared  : the owner gave the entry several colours on this screen (needs a split: spare entry / row / tile copy)
#   unknown : not a palette file pixel the colour-id capture knows (3D, affine sprite, colours set by code)
# Per cause: the entries (narc, file, index) with the most pixels, owner colour -> shown colour.
import json, os, sys, glob, collections
from PIL import Image, ImageChops
A = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'
D = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
M = json.load(open('matches.json'))
def o1(v): return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))
def c5(p): return (p[0] >> 3) | (p[1] >> 3) << 5 | (p[2] >> 3) << 10
def q5(im): return im.point(lambda v: (v >> 3) << 3)
def wrongcount(a, b):
    r, g, bb = ImageChops.difference(a, b).split()
    return ImageChops.lighter(ImageChops.lighter(r, g), bb).point(lambda v: 255 if v else 0).histogram()[255]
names = sys.argv[1:] or sorted(M)
for name in names:
    cap = M.get(name)
    fa = sorted(glob.glob('rec/am_px_%s/f*.png' % cap)); fd = sorted(glob.glob('rec/def_px_%s/f*.png' % cap))
    if not fa or not os.path.exists('caps/id_%s.json' % cap):
        continue
    own = Image.open(A + name).convert('RGB').point(lambda v: o1(v) << 3); dref = q5(Image.open(D + name).convert('RGB'))
    g = min((q5(Image.open(f).convert('RGB')) for f in fa), key=lambda im: wrongcount(own, im))
    dc = min((q5(Image.open(f).convert('RGB')) for f in fd), key=lambda im: wrongcount(dref, im)) if fd else dref
    log = json.load(open('caps/id_%s.json' % cap)); idim = Image.open('caps/id_%s.png' % cap).convert('RGB')
    O, G, Dr, Dc, I = own.load(), g.load(), dref.load(), dc.load(), idim.load()
    want = collections.defaultdict(collections.Counter)      # entry -> owner colours on this screen
    px = []
    for y in range(384):
        for x in range(256):
            if Dr[x, y] != Dc[x, y]:
                continue
            ic = c5(I[x, y]); s = ic >> 8
            e = (log['narc'][s], log['file'][s] & 0xFFF, (log['file'][s] >> 12) * 256 + (ic & 255)) if s < min(64, log['count']) else None
            oc = tuple(v >> 3 for v in O[x, y]); gc = tuple(v >> 3 for v in G[x, y])
            if e: want[e][oc] += 1
            if oc != gc: px.append((e, oc, gc, x, y))
    out = collections.defaultdict(collections.Counter)
    for e, oc, gc, x, y in px:
        if e is None: cause = 'unknown'
        elif len(want[e]) > 1: cause = 'shared'
        else: cause = 'wrong'
        out[cause][(e, oc, gc)] += 1
    tot = {k: sum(v.values()) for k, v in out.items()}
    print('%s (%s): %s' % (name, cap, tot))
    for k, v in out.items():
        for (e, oc, gc), n in v.most_common(4):
            print('   %-7s %5d  entry %-16s owner %-12s shown %s' % (k, n, e, oc, gc))
