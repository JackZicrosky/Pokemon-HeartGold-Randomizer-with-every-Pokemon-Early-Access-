# grid.py OUT NAME... : last frame of rec/def_px_NAME | rec/am_px_NAME, one row per name (2x scale)
import json, sys
from PIL import Image
out, names = sys.argv[1], sys.argv[2:]
rows = []
for n in names:
    ims = []
    for m in ('def', 'am'):
        d = 'rec/%s_px_%s' % (m, n)
        try:
            fr = json.load(open(d + '/index.json'))['frames']
            ims.append(Image.open(d + '/' + fr[-1]).convert('RGB'))
        except Exception:
            ims.append(Image.new('RGB', (256, 384), (255, 0, 255)))
    rows.append(ims)
W, H = 256, 384
g = Image.new('RGB', (W * 2 + 8, (H + 8) * len(rows)), (60, 60, 60))
for i, (a, b) in enumerate(rows):
    g.paste(a, (0, i * (H + 8))); g.paste(b, (W + 8, i * (H + 8)))
g.save(out)
