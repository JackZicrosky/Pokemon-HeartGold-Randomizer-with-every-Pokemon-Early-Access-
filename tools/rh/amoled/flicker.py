# flicker.py [PREFIX] : flickers the AMOLED theme adds. A blip = a frame that changes and comes back to exactly the
# frame before it within 3 frames (rec/*/index.json: identical frames share a file). The game's own blips (a
# blinking cursor, an animation) show in the Default recording of the same screen too; reported are the AMOLED
# blips whose area the Default recording never blips in.
import json, glob, os, sys
from PIL import Image, ImageChops

def blips(d):
    fr = json.load(open(d + '/index.json'))['frames']
    out = []
    for i in range(1, len(fr) - 1):
        if fr[i] != fr[i - 1] and any(i + k < len(fr) and fr[i + k] == fr[i - 1] for k in (1, 2, 3)):
            a = Image.open(d + '/' + fr[i - 1]).convert('RGB'); b = Image.open(d + '/' + fr[i]).convert('RGB')
            box = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v else 0).getbbox()
            if box: out.append((i, box))
    return out

def overlap(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]

pre = sys.argv[1] if len(sys.argv) > 1 else ''
total = 0
for d in sorted(glob.glob('rec/am_px_%s*' % pre)):
    n = d.split('am_px_')[1]; dd = 'rec/def_px_' + n
    if not os.path.exists(d + '/index.json') or not os.path.exists(dd + '/index.json'):
        continue
    am, de = blips(d), blips(dd)
    extra = [(i, b) for i, b in am if not any(overlap(b, db) for _, db in de)]
    if extra:
        total += 1
        print('%-28s AMOLED-only blips: %d  first %s' % (n, len(extra), extra[:3]), flush=True)
print('recordings with AMOLED-only flicker:', total)
