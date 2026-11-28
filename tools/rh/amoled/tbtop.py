import json, sys
from PIL import Image, ImageChops
for n in sys.argv[1:]:
    d = 'rec/%s_px_tb_%s/' % ('def' if n == 'd' else 'am', n); j = json.load(open(d + 'index.json')); fr = j['frames']; pal = j['pal']
    A = next(i for i, v in enumerate(pal) if v[0] == 0x5e00)
    ev = []; prev = None
    for i, f in enumerate(fr):
        if prev and f != prev and i > A + 150:
            a = Image.open(d + prev).convert('L').crop((0, 0, 256, 192)); b = Image.open(d + f).convert('L').crop((0, 0, 256, 192))
            if sum(1 for p in ImageChops.difference(a, b).getdata() if p > 40) > 1500: ev.append(i - A)
        prev = f
    print(n, ev[:40])
