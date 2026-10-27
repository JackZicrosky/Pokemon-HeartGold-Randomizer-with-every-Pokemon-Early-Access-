# flashscan.py PREFIX : AMOLED recordings rec/am_PREFIX*: frames where the top screen has the light-blue HP-box fill,
# and frames where the bottom screen is almost all black while the frames around it are not (a one-frame blackout)
import json, glob, os, sys
from PIL import Image
B = (136, 208, 240)
for d in sorted(glob.glob('rec/am_%s*' % sys.argv[1])):
    fr = json.load(open(d + '/index.json'))['frames']
    blue, dark = {}, {}
    for f in set(fr):
        im = Image.open(d + '/' + f).convert('RGB')
        blue[f] = list(im.crop((0, 10, 256, 120)).getdata()).count(B) > 300
        bot = list(im.crop((0, 200, 256, 384)).getdata())
        dark[f] = sum(1 for p in bot if max(p) < 16) / len(bot)
    nb = [i for i in range(len(fr)) if blue[fr[i]]]
    flash = [i for i in range(1, len(fr) - 1) if dark[fr[i]] > 0.97 and dark[fr[i - 1]] < 0.9 and dark[fr[i + 1]] < 0.9]
    if nb or flash:
        print('%-36s blue HP boxes in %d frames %s, bottom blackout frames %s' % (os.path.basename(d)[3:], len(nb), nb[:8], flash[:8]))
print('scan done')
