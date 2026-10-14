# test only: every pixel where the AMOLED game differs from the owner's picture, by cause
#   frame    : the Default capture differs from the Default screenshot there (animation / different frame) - not counted
#   bug      : the pixel shows a palette entry, the owner gave that entry one colour on this screen, game shows another
#   shared   : the owner gave different colours to pixels of the same palette entry on this screen
#   other    : not a plain palette entry (blending / dimming, colours set by code, unhooked)
# writes strict/NAME.png: owner picture with bug=red, shared=magenta, other=cyan
import json, os, sys, collections
from PIL import Image
SP = os.environ.get('RH_AMOLED_WORK', os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, SP)
import narc
D = os.environ.get('RH_UI_DEFAULT', '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots') + '/'
A = os.environ.get('RH_UI_AMOLED', '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED') + '/'
C = SP + '/caps/'
os.makedirs(SP + '/strict', exist_ok=True)
M = json.load(open(SP + '/matches.json'))
LRN = json.load(open(SP + '/learned.json'))
_m = {}
def PAL(n, f):
    try:
        if n not in _m: _m[n] = narc.members(narc.ROOT + 'a/%d/%d/%d' % (n // 100, n // 10 % 10, n % 10))
        return narc.colors(_m[n][f]) or []
    except Exception:
        return []
def o1(v): return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))
def c5(p): return (p[0] >> 3) | (p[1] >> 3) << 5 | (p[2] >> 3) << 10
tot = collections.Counter()
for name in sorted(M):
    if open(D + name, 'rb').read() == open(A + name, 'rb').read():
        continue                                   # an unedited copy of the Default picture
    cap = M[name]
    if not os.path.exists(C + 'am_' + cap + '.png'): continue
    o = Image.open(A + name).convert('RGB'); d = Image.open(D + name).convert('RGB')
    dc = Image.open(C + 'def_' + cap + '.png').convert('RGB'); g = Image.open(C + 'am_' + cap + '.png').convert('RGB')
    ic = Image.open(C + 'id_' + cap + '.png').convert('RGB'); log = json.load(open(C + 'id_' + cap + '.json'))
    px = {}; per = collections.defaultdict(collections.Counter)
    for y in range(384):
        for x in range(256):
            op = o.getpixel((x, y)); oc = o1(op[0]) | o1(op[1]) << 5 | o1(op[2]) << 10
            e = None
            if d.getpixel((x, y)) == dc.getpixel((x, y)):
                c = c5(ic.getpixel((x, y))); s = c >> 8
                if s < min(64, log['count']) and log['narc'][s] < 320:
                    n, f = log['narc'][s], log['file'][s]; j = (f >> 12) * 256 + (c & 255); p = PAL(n, f & 0xFFF)
                    if j < len(p) and p[j] == c5(d.getpixel((x, y))):
                        e = (n, f & 0xFFF, j, log['ctx'][s])
                        per[e][oc] += 1
            px[(x, y)] = (oc, e, d.getpixel((x, y)) == dc.getpixel((x, y)))
    cnt = collections.Counter(); im = o.copy()
    for (x, y), (oc, e, same) in px.items():
        if oc == c5(g.getpixel((x, y))): continue
        if not same: cnt['frame'] += 1; continue
        if e is None:
            k = 'unthemed' if tuple(v >> 3 for v in d.getpixel((x, y))) == tuple(o1(v) for v in o.getpixel((x, y))) else 'other'
        elif len(per[e]) > 1 and per[e].most_common(1)[0][0] != oc: k = 'shared'
        else:
            v = LRN.get('%d,%d,%d@%d' % e, {})
            k = 'bug' if v and int(max(v.items(), key=lambda kv: kv[1])[0]) == oc else 'sameload'
        cnt[k] += 1
        im.putpixel((x, y), {'bug': (255, 0, 0), 'shared': (255, 0, 255), 'other': (0, 255, 255), 'sameload': (255, 255, 0), 'unthemed': (0, 128, 0)}[k])
    im.save(SP + '/strict/' + name)
    tot.update(cnt)
    print('%-38s bug %5d  sameload %5d  shared %5d  other %5d  (frame %5d, unthemed area %5d)' % (name, cnt['bug'], cnt['sameload'], cnt['shared'], cnt['other'], cnt['frame'], cnt['unthemed']))
print('TOTAL', dict(tot))
