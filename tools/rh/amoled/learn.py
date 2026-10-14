# test only: learn the owner's AMOLED colours per palette entry from colour-id captures
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
cvotes = collections.defaultdict(collections.Counter)      # (narc, file, idx, screen context) -> Counter(owner colour)
# screens shown by one overlay: their votes also go to that context (screen-specific palettes, see gen_rh_amoled.py)
CONTEXT = {'02_main_menu.png': 'ov74', '03_new_game_save_warning.png': 'ov74'}
ctx_votes = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
pix = collections.defaultdict(lambda: collections.defaultdict(lambda: collections.defaultdict(list)))   # screen -> entry -> colour -> pixels
report = []
MATCH = {}
for f in sorted(glob.glob(A + '[0-9]*.png')):
    name = os.path.basename(f)
    if not os.path.exists(D + name): continue
    if open(D + name, 'rb').read() == open(f, 'rb').read():
        continue                                   # an unedited copy of the Default picture: not a design (yet)
    d = Image.open(D + name).convert('RGB'); o = Image.open(f).convert('RGB')
    dd = list(d.getdata())
    # the capture that looks most like the Default screenshot
    best, bn = None, 10**9
    for k, im in defs.items():
        diff = 98304 - ImageChops.difference(d, im).convert('L').histogram()[0]
        if diff < bn: best, bn = k, diff
    if best is None or bn > 70000 or not os.path.exists(CAPS + 'id_' + best + '.json'):
        continue
    MATCH[name] = best
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
            ctx = log['ctx'][slot]
            if n >= 320: e = None
        if e:
            try:
                pal = palette(e[0], e[1])
            except (OSError, IndexError):
                pal = []
            if e[2] < len(pal) and pal[e[2]] == c5(dp):
                votes[e][own5(op)] += 1
                cvotes[e + (ctx,)][own5(op)] += 1
                ctx_votes[name][e + (ctx,)][own5(op)] += 1
                pix[name][e + (ctx,)][own5(op)].append(i)
                if dp != op: ok += 1
        if dp != op: changed += 1
    report.append('%-38s <- %-10s diff=%5d changed=%5d explained=%5d (%d%%)' % (name, best, bn, changed, ok, 100 * ok // max(1, changed)))
print('\n'.join(report))
out = {'%d,%d,%d@%d' % e: dict(v) for e, v in cvotes.items()}

json.dump(out, open(sys.argv[1], 'w'))
json.dump({sc: {'%d,%d,%d@%d' % e: dict(v) for e, v in cv.items()} for sc, cv in ctx_votes.items()}, open('by_screen.json', 'w'))
# per screen: its fingerprint and the owner's colour (most pixels) for every palette entry it shows
screens = {}
for sc, cv in ctx_votes.items():
    sig = json.load(open(CAPS + 'id_' + MATCH[sc] + '.json')).get('sig')
    if sig is None: continue
    screens[sc] = {'sig': sig, 'cols': {'%d,%d,%d@%d' % e: int(max(v.items(), key=lambda kv: kv[1])[0]) for e, v in cv.items()}}
# tile patches: on a screen, pixels of one palette entry the owner gave a less common colour get a spare entry of the
# same palette row (never shown on any captured screen) coloured that way; their tile data points there
import dsrender
used = collections.defaultdict(set)                  # (narc, file) -> entries shown somewhere
for (n, f, i, ctx) in cvotes:
    used[(n, f)].add(i)
spare = {}                                           # (narc, file, row, colour) -> spare index in the row
taken = collections.defaultdict(set)                 # (narc, file, row) -> spare indices handed out
nopatch = collections.Counter()
spare_ip = collections.defaultdict(dict)             # screen -> {palette RAM index: colour} for the spare entries
for sc, ents in pix.items():
    if sc not in screens: continue
    vf = CAPS + 'id_' + MATCH[sc] + '.vram2'
    if not os.path.exists(vf): continue
    bid = open(vf, 'rb').read(); idshot = Image.open(CAPS + 'id_' + MATCH[sc] + '.png').convert('RGB')
    scr, fit = dsrender.screens(bid, idshot)
    if max(fit) < 0.85:
        nopatch['screen not rendered reliably (3D, affine...)'] += sum(len(p) for c in ents.values() for p in c.values())
        continue
    patches = set()
    for (n, f, i, ctx), cols in ents.items():
        if len(cols) < 2: continue
        major = max(cols.items(), key=lambda kv: len(kv[1]))[0]
        row = i // 16
        for col, pts in cols.items():
            if col == major: continue
            key = (n, f, row, col)
            if key not in spare:
                free = [j for j in range(1, 16) if row * 16 + j not in used[(n, f)] and j not in taken[(n, f, row)]]
                if not free:
                    nopatch['no spare entry'] += len(pts); continue
                spare[key] = free[0]; taken[(n, f, row)].add(free[0])
            j = spare[key]
            got = 0
            for k in pts:
                x, y = k % 256, k // 256
                r = scr[y // 192][y % 192][x] if fit[y // 192] >= 0.85 else None     # (only on a reliably rendered screen)
                if r is None or r['kind'] not in ('bg', 'obj') or r.get('half') is None or r['idx'] % 16 != i % 16:
                    continue                          # not a 4bpp background / sprite pixel of this entry (3D, 8bpp, ...)
                if dsrender.shown(bid, r) != tuple(v >> 3 for v in idshot.getpixel((x, y))):
                    continue                          # the render doesn't match the game at this pixel: don't touch it
                patches.add((r['vram'], r['half'], i % 16, j)); got += 1
                spare_ip[sc][(r['idx'] & ~15) | j] = col          # the spare entry's colour, straight into palette RAM
            nopatch['not a 4bpp tile pixel'] += len(pts) - got
            if got:
                screens[sc]['cols']['%d,%d,%d@%d' % (n, f, row * 16 + j, ctx)] = int(''.join(map(str, [col])) if isinstance(col, int) else col)
    screens[sc]['vpatch'] = sorted(patches)
print('tile patches:', sum(len(s_['vpatch']) for s_ in screens.values() if 'vpatch' in s_), 'not patchable px:', dict(nopatch))

# palette index patches: with the AMOLED build's own capture of the screen (caps/am_*), every pixel's top layer and
# palette RAM entry and its colour effect; the colour that entry needs for the owner's pixel (worked back through
# dimming / blending); where the AMOLED build doesn't have it, a patch (most pixels of an entry decide)
ipstat = collections.Counter()
for sc in screens:
    cap = MATCH[sc]
    vf = CAPS + 'am_' + cap + '.vram2'
    if not os.path.exists(vf) or not os.path.exists(CAPS + 'am_' + cap + '.png'): continue
    b = open(vf, 'rb').read()
    shot = Image.open(CAPS + 'am_' + cap + '.png').convert('RGB')
    scr, fit = dsrender.screens(b, shot)
    if max(fit) < 0.85:
        ipstat['screens not rendered reliably'] += 1
        continue
    o = Image.open(A + sc).convert('RGB'); d = Image.open(D + sc).convert('RGB'); dc = defs.get(cap)
    want = collections.defaultdict(collections.Counter)
    # the colour effects as the game sets them: from the colour-id capture (no register patches there; the AMOLED
    # build switches some off)
    engines_am = dsrender.ENGINES[0]
    vi = CAPS + 'id_' + cap + '.vram2'
    scr_id = None
    if os.path.exists(vi):
        scr_id, fit_id = dsrender.screens(open(vi, 'rb').read(), Image.open(CAPS + 'id_' + cap + '.png').convert('RGB'))
        if dsrender.ENGINES[0] != engines_am:
            scr_id = None
        dsrender.ENGINES[0] = engines_am
    def game_effect(px, x, y):
        if scr_id is None: return px
        q = scr_id[y // 192][y % 192][x]
        if q is None or q['idx'] != px['idx']: return px
        px = dict(px); px['effect'] = q['effect']; px['below'] = q['below']
        return px
    # on a screen half where the colour effects (dimming, blending) can't give the owner's colours, the effect is
    # switched off while the screen shows (BLDCNT = 0) and every entry gets the owner's colour as it is
    engines = dsrender.ENGINES[0]
    unreach = [0, 0]
    for y in range(0, 384, 2):
        for x in range(0, 256, 2):
            if fit[y // 192] < 0.85: continue
            px = scr[y // 192][y % 192][x]
            if px is None: continue
            px = game_effect(px, x, y)
            if not px['effect']: continue
            op = o.getpixel((x, y))
            if dsrender.needed(b, px, (o1(op[0]), o1(op[1]), o1(op[2]))) is None:
                unreach[y // 192] += 1
    off = [u > 8 for u in unreach]
    screens[sc]['regs'] = [(0x50 if engines[h] == 0 else 0x1050, 0) for h in (0, 1) if off[h]]
    for y in range(384):
        for x in range(256):
            if dc is not None and d.getpixel((x, y)) != dc.getpixel((x, y)):
                continue                              # a different frame there
            px = scr[y // 192][y % 192][x] if fit[y // 192] >= 0.85 else None
            if px is None: continue
            g = shot.getpixel((x, y))
            if dsrender.shown(b, px) != tuple(v >> 3 for v in g):
                ipstat['render differs (3D, affine sprite...)'] += 1; continue
            op = o.getpixel((x, y)); ow = (o1(op[0]), o1(op[1]), o1(op[2]))
            px = game_effect(px, x, y)
            if off[y // 192]:
                px = dict(px); px['effect'] = 0
            t = dsrender.needed(b, px, ow)
            if t is None:
                ipstat['blend can not reach it'] += 1; continue
            want[px['idx']][t[0] | t[1] << 5 | t[2] << 10] += 1
    ip = []
    for idx, cnt in want.items():              # every entry the screen shows (undone when the screen changes)
        ip.append((idx, cnt.most_common(1)[0][0]))
    done = {i for i, _ in ip}
    ip += [(i, c) for i, c in spare_ip.get(sc, {}).items() if i not in done]
    screens[sc]['ipatch'] = ip
    ipstat['patches'] += len(ip)
print('palette index patches:', dict(ipstat))
json.dump(screens, open('screens.json', 'w'))
json.dump(screens, open('screens.json', 'w'))
json.dump({name: cap for name, cap in MATCH.items()}, open('matches.json', 'w'))
print(len(out), 'entries')
