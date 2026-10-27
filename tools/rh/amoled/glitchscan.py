# glitchscan.py PREFIX : AMOLED frames whose top screen has the light-blue HP-box glitch colour (136,208,240) in a
# block the Default twin frame doesn't have
import json, glob, os, sys
from PIL import Image
B = (136, 208, 240)
def cnt(p):
    im = Image.open(p).convert('RGB').crop((0, 0, 256, 192))
    return sum(1 for c in im.getdata() if c == B)
tot = 0
for d in sorted(glob.glob('rec/am_%s*' % sys.argv[1])):
    n = os.path.basename(d)[3:]; dd = 'rec/def_' + n
    if not os.path.exists(d + '/index.json') or not os.path.exists(dd + '/index.json'): continue
    a = json.load(open(d + '/index.json'))['frames']; df = json.load(open(dd + '/index.json'))['frames']
    ca = {f: cnt(d + '/' + f) for f in set(a)}; cd = {f: cnt(dd + '/' + f) for f in set(df)}
    bad = [i for i in range(len(a)) if ca[a[i]] - cd[df[i]] > 300]
    if bad:
        tot += 1; print('%-36s %3d glitch frames: %s' % (n, len(bad), bad[:12]))
print('recordings with the glitch:', tot)
