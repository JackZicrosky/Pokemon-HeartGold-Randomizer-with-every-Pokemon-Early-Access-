# proof3.py [PICTURE...] : the proof, per owner picture (matches.json: picture -> capture; rec/def_px_CAP and
# rec/am_px_CAP: 60 frames each at that screen). Every pixel, colours as the DS has them (5 bits):
#   - where the owner recoloured it (owner AMOLED picture != owner Default picture): the AMOLED frame must show the
#     owner's colour, and the Default frame must show the owner's Default colour there (same moment: "situation")
#   - where the owner left the game's colour: the AMOLED frame must show what the game shows at that moment (the
#     Default frame), so animations (Pokemon sprites...) can't count, and anything the theme recolours there does
#   - battles: the wild Pokemon's gender symbol may differ (it is rolled by the game; owner: gender varies)
# The AMOLED frame and the Default frame closest to it (+-8 frames, the theme can be frames behind) with the fewest
# wrong pixels; proven = situation 0 and at most 9 wrong (99.99%).
# The recordings of the neighbouring steps of a numbered sequence count too (see neighbours).
# Writes proof3/NAME.png when not proven: owner picture | AMOLED frame | wrong in red, situation in yellow.
import json, os, sys
from PIL import Image, ImageChops, ImageFilter
A = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'
D = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
M = json.load(open('matches.json'))
os.makedirs('proof3', exist_ok=True)
GENDER = {(31, 11, 5), (6, 12, 27)}
def o1(v): return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))
def q5(im): return im.point(lambda v: (v >> 3) << 3)
def neq(a, b):                                  # 255 where the pictures differ
    r, g, b2 = ImageChops.difference(a, b).split()
    return ImageChops.lighter(ImageChops.lighter(r, g), b2).point(lambda v: 255 if v else 0)
def count(m): return m.histogram()[255]
def load(cap, mode):
    d = 'rec/%s_px_%s' % (mode, cap)
    if not os.path.exists(d + '/index.json'): return None
    files = sorted(set(json.load(open(d + '/index.json'))['frames']))      # the distinct frames
    ims = [q5(Image.open(d + '/' + f).convert('RGB')) for f in files]
    return len(ims), lambda i: ims[i]
def gmask(*ims):
    g = Image.new('L', (256, 384), 0); gp = g.load()
    for im in ims:
        px = im.load()
        for y in range(192):
            for x in range(256):
                if tuple(v >> 3 for v in px[x, y]) in GENDER: gp[x, y] = 255
    return ImageChops.invert(g.filter(ImageFilter.MaxFilter(7)))
names = sys.argv[1:] or sorted(M)
def neighbours(cap):
    # the recordings of the steps before / after too (a numbered sequence, e.g. the naming screen's letters: the
    # recordings can run a key press ahead of the captures); the situation check still demands the owner's moment
    import re
    m = re.match(r'(.*?)(\d+)$', cap)
    out = [cap]
    if m:
        w, k = len(m.group(2)), int(m.group(2))
        out += ['%s%0*d' % (m.group(1), w, k + dk) for dk in (-1, 1) if k + dk >= 0]
    return out
def score_cap(name, cap, od, oa, changed, kept):
    dl, al = load(cap, 'def'), load(cap, 'am')
    if dl is None or al is None:
        return None
    battle = name[:2].isdigit() and 59 <= int(name[:2]) <= 72
    best = None
    gcache = {}
    nd, dget = dl; na, aget = al
    for i in range(na):
        am = aget(i)
        wa = ImageChops.multiply(neq(am, oa), changed)               # recoloured pixels: the owner's colour
        for j in range(nd):
            de = dget(j)
            sit = ImageChops.multiply(neq(de, od), changed)          # ... at the owner's moment
            wk = ImageChops.multiply(neq(am, de), kept)              # kept pixels: the game's own colour
            wrong = ImageChops.lighter(wa, wk)
            if battle:
                if j not in gcache: gcache[j] = gmask(od, de)
                gm = gcache[j]
                wrong = ImageChops.multiply(wrong, gm); sit = ImageChops.multiply(sit, gm)
            score = (count(sit) + count(wrong), count(sit), count(wrong))
            if best is None or score < best[0]:
                best = (score, sit, wrong, aget(i), cap)
    return best
ok = 0
for name in names:
    od = q5(Image.open(D + name).convert('RGB')); oa = Image.open(A + name).convert('RGB').point(lambda v: o1(v) << 3)
    changed = neq(od, oa); kept = ImageChops.invert(changed)
    res = [r for r in (score_cap(name, c, od, oa, changed, kept) for c in neighbours(M[name])) if r is not None]
    if not res:
        print('%-40s %-16s no recording' % (name, M[name])); continue
    (_, ns, nw), sit, wrong, am, cap = min(res, key=lambda r: r[0])
    good = ns == 0 and nw <= 9
    ok += good
    if not good:
        im = Image.new('RGB', (768, 384)); im.paste(oa, (0, 0)); im.paste(am, (256, 0))
        red = am.copy(); red.paste((255, 0, 0), mask=wrong); red.paste((255, 255, 0), mask=sit); im.paste(red, (512, 0))
        im.save('proof3/%s' % name)
    print('%-40s %-16s situation %6d  wrong %6d  %8.4f%%  %s' % (name, cap, ns, nw, 100 * (1 - nw / 98304), 'PROVEN' if good else ''), flush=True)
print('proven %d of %d' % (ok, len(names)))
