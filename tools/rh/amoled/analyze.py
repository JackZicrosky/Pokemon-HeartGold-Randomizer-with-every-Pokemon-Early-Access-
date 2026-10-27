# analyze.py PREFIX REFS.json OUTDIR : every recording rec/am_PREFIX* (and its Default twin rec/def_*): its distinct
# frames (which frames each covers), the screen region that changed from the previous one, and every AMOLED frame
# against each of the owner's reference pictures listed in REFS.json (a list of picture names): the best-matching
# picture and its wrong pixel count. Compared: pixels where the Default frame shows what the Default screenshot shows;
# colours as the DS has them (5 bits).
import json, os, sys, glob
from PIL import Image, ImageChops
A = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'
D = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
prefix, refs, out = sys.argv[1], json.load(open(sys.argv[2])), sys.argv[3]
os.makedirs(out, exist_ok=True)
def o1(v): return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))
def q5(im): return im.point(lambda v: (v >> 3) << 3)
def anydiff(a, b):          # 255 where any channel differs
    r, g, b2 = ImageChops.difference(a, b).split()
    return ImageChops.lighter(ImageChops.lighter(r, g), b2).point(lambda v: 255 if v else 0)
REF = {}
for n in refs:
    REF[n] = (Image.open(A + n).convert('RGB').point(lambda v: o1(v) << 3), Image.open(D + n).convert('RGB'))
def runs(frames):
    r = []
    for i, f in enumerate(frames):
        if r and r[-1][2] == f: r[-1][1] = i
        else: r.append([i, i, f])
    return r
report = {}
for d in sorted(glob.glob('rec/am_%s*' % prefix)):
    name = os.path.basename(d)[3:]
    am = json.load(open(d + '/index.json'))['frames']
    dd = 'rec/def_' + name
    df = json.load(open(dd + '/index.json'))['frames'] if os.path.exists(dd + '/index.json') else None
    entry = {'distinct_am': len(set(am)), 'distinct_def': len(set(df)) if df else None, 'runs': []}
    prev = None
    for a, b, f in runs(am):
        im = Image.open(d + '/' + f).convert('RGB')
        rec = {'frames': [a, b], 'file': f}
        if prev is not None:
            rec['changed'] = ImageChops.difference(prev, im).getbbox()
        prev = im
        if df:
            dfi = Image.open(dd + '/' + df[a]).convert('RGB')
            best = None
            for n, (own, dref) in REF.items():
                same = anydiff(dref, dfi).point(lambda v: 255 - v)              # Default frame shows the screenshot
                cmpn = same.histogram()[255]
                if cmpn < 20000: continue
                wrong = ImageChops.multiply(anydiff(own, q5(im)), same)
                w = wrong.histogram()[255]
                if best is None or w / cmpn < best[1] / best[2]:
                    best = (n, w, cmpn, wrong.getbbox())
            if best:
                rec['ref'], rec['wrong'], rec['compared'], rec['wrong_box'] = best
        entry['runs'].append(rec)
    report[name] = entry
json.dump(report, open(os.path.join(out, 'report_%s.json' % prefix), 'w'), indent=1)
for k, v in report.items():
    rs = [r for r in v['runs'] if 'wrong' in r]
    worst = max(rs, key=lambda r: r['wrong']) if rs else None
    print('%-34s am %3d def %4s distinct | worst %s' % (k, v['distinct_am'], v['distinct_def'],
          '%s %d px (frames %d-%d)' % (worst['ref'], worst['wrong'], *worst['frames']) if worst else '-'))
