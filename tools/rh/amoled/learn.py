# test only: learn the owner's AMOLED colours per palette entry from colour-id captures
# Work folder (captures in WORK/caps, test saves in WORK): $RH_AMOLED_WORK, default: this folder.
# usage: learn.py OUT.json  (reads caps/def_*.png, caps/id_*.png, caps/id_*.json)
import glob, json, os, sys, collections
from PIL import Image, ImageChops
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import narc
D = os.environ.get('RH_UI_DEFAULT', '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots') + '/'
A = os.environ.get('RH_UI_AMOLED', '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED') + '/'
CAPS = os.environ.get('RH_AMOLED_WORK', os.path.dirname(os.path.abspath(__file__))) + '/caps/'
_files = {}
def palette(n, f):
    if n not in _files:
        _files[n] = narc.members(narc.ROOT + 'a/%d/%d/%d' % (n // 100, n // 10 % 10, n % 10))
    b = _files[n][f]
    return narc.colors(b) if b[:4] == b'RLCN' else []
def c5(p): return (p[0] >> 3) | (p[1] >> 3) << 5 | (p[2] >> 3) << 10
def o1(v): return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))   # screenshot values are x*8; the owner's own colours round
def own5(p): return o1(p[0]) | o1(p[1]) << 5 | o1(p[2]) << 10
defs = {os.path.basename(f)[4:-4]: Image.open(f).convert('RGB') for f in glob.glob(CAPS + 'def_*.png')
        if os.path.exists(CAPS + 'id_' + os.path.basename(f)[4:-4] + '.json')}     # only captures with an id twin
votes = collections.defaultdict(collections.Counter)       # (narc, file, idx) -> Counter(owner colour)
# screens shown by one overlay: their votes also go to that context (screen-specific palettes, see gen_rh_amoled.py)
CONTEXT = {'02_main_menu.png': 'ov74', '03_new_game_save_warning.png': 'ov74'}
ctx_votes = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
report = []
for f in sorted(glob.glob(A + '[0-9]*.png')):
    name = os.path.basename(f)
    if not os.path.exists(D + name): continue
    d = Image.open(D + name).convert('RGB'); o = Image.open(f).convert('RGB')
    dd = list(d.getdata())
    # the capture that looks most like the Default screenshot
    best, bn = None, 10**9
    for k, im in defs.items():
        diff = 98304 - ImageChops.difference(d, im).convert('L').histogram()[0]
        if diff < bn: best, bn = k, diff
    if best is None or bn > 70000 or not os.path.exists(CAPS + 'id_' + best + '.json'):
        continue
    log = json.load(open(CAPS + 'id_' + best + '.json'))
    ids = list(Image.open(CAPS + 'id_' + best + '.png').convert('RGB').getdata())
    dcap = list(defs[best].getdata())          # the Default capture: only pixels where it shows the same frame count
    od = list(o.getdata())
    changed = ok = 0
    for i, (dp, op, ip, cp) in enumerate(zip(dd, od, ids, dcap)):
        if cp != dp:
            continue
        c = c5(ip); slot, idx = c >> 8, c & 255
        e = None
        if slot < min(64, log["count"]):
            n, fl = log['narc'][slot], log['file'][slot]
            e = (n, fl & 0xFFF, (fl >> 12) * 256 + idx)
        if e:
            pal = palette(e[0], e[1])
            if e[2] < len(pal) and pal[e[2]] == c5(dp):
                votes[e][own5(op)] += 1
                if name in CONTEXT:
                    ctx_votes[CONTEXT[name]][e][own5(op)] += 1
                if dp != op: ok += 1
        if dp != op: changed += 1
    report.append('%-38s <- %-10s diff=%5d changed=%5d explained=%5d (%d%%)' % (name, best, bn, changed, ok, 100 * ok // max(1, changed)))
print('\n'.join(report))
out = {'%d,%d,%d' % e: dict(v) for e, v in votes.items()}

json.dump(out, open(sys.argv[1], 'w'))
print(len(out), 'entries')
