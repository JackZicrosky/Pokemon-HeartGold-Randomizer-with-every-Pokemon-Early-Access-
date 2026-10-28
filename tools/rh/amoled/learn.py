# test only: learn the owner's AMOLED colours per palette entry from colour-id captures
# usage: learn.py OUT.json  (reads caps/def_*.png, caps/id_*.png, caps/id_*.json)
import glob, json, os, sys, collections
from PIL import Image, ImageChops
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import narc
D = '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
A = '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'
CAPS = os.path.dirname(os.path.abspath(__file__)) + '/caps/'
_files = {}
def palette(n, f):
    if n not in _files:
        _files[n] = narc.members(narc.ROOT + 'a/%d/%d/%d' % (n // 100, n // 10 % 10, n % 10))
    b = _files[n][f]
    return narc.colors(b) if b[:4] == b'RLCN' else []
def c5(p): return (p[0] >> 3) | (p[1] >> 3) << 5 | (p[2] >> 3) << 10
import pickle
STATIC = pickle.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static_tiles.pkl'), 'rb'))
def static_tile(dump, addr):
    """the 4bpp tile at VRAM address addr (in a vram2 dump) is graphics from a ROM file, not drawn at runtime (text,
    numbers, Pokemon sprites), and not one colour (blank text-window tiles get drawn into)"""
    import dsrender as R
    for base, off, size in ((0x06000000, R.O_BGM, 0x80000), (0x06200000, R.O_BGS, 0x20000),
                            (0x06400000, R.O_OBJM, 0x40000), (0x06600000, R.O_OBJS, 0x20000)):
        if base <= addr < base + size:
            o = off + ((addr - base) & ~31); t = dump[o:o + 32]
            return t in STATIC and len({v & 15 for v in t} | {v >> 4 for v in t}) > 1
    return False
ART_ARCHIVES = {4, 114, 194, 195, 18, 20, 69, 46, 174, 264, 81, 8, 120, 6}
def ui_file(n, f, learned_files):
    """palette files the AMOLED theme may set per screen: UI files; art (Pokemon, trainers, battle backgrounds) only
    where the owner recoloured it"""
    if n == 7 and f < 246: return False                   # battle backgrounds / platforms, per terrain and time
    if n in ART_ARCHIVES: return (n, f) in learned_files
    return True
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
# screens whose tile maps the theme code itself repoints per button (src/rh_theme.c BattleMoves): no map patches,
# and tile patches even where the owner coloured a shared tile pixel differently per button (type colours)
NOMAP = {'72_battle_check_moves.png'}
# screens whose best-looking capture is the wrong state (an unpressed frame looks like the pressed one...)
# other states of a screen (captures) that use its page
ALIAS = {'61_battle_moves.png': ['f_moves'], '72_battle_check_moves.png': ['f_checkmoves']}
FORCE = {'61_battle_moves.png': 'e_moves', '66_battle_command_menu.png': 'e_cmd', '67_battle_fight_pressed.png': 'e_fp0', '68_battle_bag_pressed.png': 'e_bp0'}
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
    if name in FORCE:
        best = FORCE[name]
        bn = 98304 - ImageChops.difference(d, defs[best]).convert('L').histogram()[0]
    # (a capture more than 20% different is another screen, not this one in another frame: its fingerprint would
    # carry the picture's colours to the wrong screen)
    if best is None or bn > 98304 // 5 or not os.path.exists(CAPS + 'id_' + best + '.json'):
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
            if n >= 320 or n in (4, 6) or (n == 7 and fl & 0xFFF < 246): e = None    # Pokemon / trainer art, battle backgrounds
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
# the battle HP boxes (narc 8 file 71): entries 2 and 14 also colour the box text, which is now an overlay with its own
# colours (src/battle/rh_hptext.c, white / (11,11,11)): the text's votes go, the box art's colours stay
HP_TEXT = (31 | 31 << 5 | 31 << 10, 11 | 11 << 5 | 11 << 10)
for votes_ in [cvotes] + list(ctx_votes.values()):
    for e in list(votes_):
        if e[0] == 8 and e[1] == 71 and e[2] % 16 in (2, 14):
            for c in HP_TEXT:
                votes_[e].pop(c, None)
for sc_ in pix:
    for e in list(pix[sc_]):
        if e[0] == 8 and e[1] == 71 and e[2] % 16 in (2, 14):
            for c in HP_TEXT:
                pix[sc_][e].pop(c, None)
out = {'%d,%d,%d@%d' % e: dict(v) for e, v in cvotes.items()}

json.dump(out, open(sys.argv[1], 'w'))
json.dump({sc: {'%d,%d,%d@%d' % e: dict(v) for e, v in cv.items()} for sc, cv in ctx_votes.items()}, open('by_screen.json', 'w'))
# per screen: its fingerprint and the owner's colour (most pixels) for every palette entry it shows
screens = {}
for sc, cv in ctx_votes.items():
    sig = json.load(open(CAPS + 'id_' + MATCH[sc] + '.json')).get('sig')
    if sig is None: continue
    screens[sc] = {'sig': sig, 'cols': {'%d,%d,%d@%d' % e: int(max(v.items(), key=lambda kv: kv[1])[0]) for e, v in cv.items()}}
    al = [json.load(open(CAPS + 'id_' + a + '.json')).get('sig') for a in ALIAS.get(sc, []) if os.path.exists(CAPS + 'id_' + a + '.json')]
    if al: screens[sc]['alias'] = [a for a in al if a is not None]
# tile patches: on a screen, pixels of one palette entry the owner gave a less common colour get a spare entry of the
# same palette row (never shown on any captured screen) coloured that way; their tile data points there
import dsrender
used = collections.defaultdict(set)                  # (narc, file) -> entries shown somewhere
for (n, f, i, ctx) in cvotes:
    used[(n, f)].add(i)
spare = {}                                           # (narc, file, row, colour) -> spare index in the row
spare8 = {}                                          # (screen, palette base, colour) -> spare index (256-colour BGs)
taken8 = collections.defaultdict(set)
taken = collections.defaultdict(set)                 # (narc, file, row) -> spare indices handed out
nopatch = collections.Counter()
RUNTIME_MAJOR = {}                                  # entry -> colour of its runtime-drawn pixels (text)
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
    shown_idx = {q['idx'] for half in scr for line in half for q in line if q}    # entries this screen shows at all
    # what the owner wants at every use of a tile pixel (a tile can be drawn in many places): patch only where most agree
    o_img = Image.open(A + sc).convert('RGB')
    tile_want = collections.defaultdict(collections.Counter)
    for yy in range(384):
        for xx in range(256):
            q = scr[yy // 192][yy % 192][xx]
            if q is not None and q['kind'] in ('bg', 'obj') and q.get('vram') is not None:
                op = o_img.getpixel((xx, yy)); tile_want[(q['vram'], q['half'])][(o1(op[0]), o1(op[1]), o1(op[2]))] += 1
    for (n, f, i, ctx), cols in ents.items():
        if len(cols) < 2: continue
        # the colour the entry keeps: the one drawn on runtime tiles (text, numbers: tile patches can't move those)
        # if any, else the one most pixels have; the other colours' pixels (on ROM graphics) move to spare entries
        def runtime_px(pts):
            n_ = 0
            for k in pts:
                q = scr[k // 256 // 192][k // 256 % 192][k % 256]
                if q is not None and q.get('half') is not None and not static_tile(bid, q['vram']): n_ += 1
            return n_
        rt = {col: runtime_px(pts) for col, pts in cols.items()}
        # (the battle HP boxes, narc 8 file 71: their text is an overlay with colours of its own, src/battle/rh_hptext.c;
        # the entries keep the box art's colours)
        if (n, f) == (8, 71) and i % 16 in (2, 14):
            rt = {col: 0 for col in cols}
        if max(rt.values()) > 0:
            major = max(cols, key=lambda c: (rt[c], len(cols[c])))
            screens[sc]['cols']['%d,%d,%d@%d' % (n, f, i, ctx)] = major
            RUNTIME_MAJOR['%d,%d,%d@%d' % (n, f, i, ctx)] = major
        else:
            major = max(cols.items(), key=lambda kv: len(kv[1]))[0]
        row = i // 16
        for col, pts in cols.items():
            if col == major: continue
            key = (sc, n, f, row, col)            # (spares are per screen: each screen sets its own row versions)
            base = None
            for k in pts:
                q = scr[k // 256 // 192][k // 256 % 192][k % 256]
                if q is not None and q['idx'] % 16 == i % 16:
                    base = q['idx'] & ~15; break
            if key in spare and base is not None and (base | spare[key]) in shown_idx:
                del spare[key]                        # (that spare is drawn by something else on this screen)
            if key not in spare:
                free = [j for j in range(1, 16) if j not in taken[(sc, n, f, row)]
                        and (base | j) not in shown_idx] if base is not None else []
                if free:
                    spare[key] = free[0]; taken[(sc, n, f, row)].add(free[0])
            j = spare.get(key)                        # (None: no spare in the 16-colour row; 256-colour pixels still can)
            got = 0
            for k in pts:
                x, y = k % 256, k // 256
                r = scr[y // 192][y % 192][x] if fit[y // 192] >= 0.85 else None     # (only on a reliably rendered screen)
                if r is not None and r['kind'] == 'bg' and r.get('half') is None and r.get('vram') is not None:
                    # a 256-colour background pixel: its byte points at a spare index of the 256 colours (one this
                    # screen doesn't show), both nibbles patched
                    if dsrender.shown(bid, r) != tuple(v >> 3 for v in idshot.getpixel((x, y))):
                        continue
                    tw = tile_want[(r['vram'], None)]
                    if tw and (len(tw) > 1 or tw.most_common(1)[0][0] != (col & 31, col >> 5 & 31, col >> 10 & 31)):
                        nopatch['tile drawn elsewhere in another colour'] += 1; continue
                    if not static_tile(bid, r['vram']):
                        nopatch['runtime-drawn tile (text, numbers, sprites)'] += 1; continue
                    pb = r['idx'] & ~255; key8 = (sc, pb, col)
                    if key8 not in spare8:
                        free8 = [v for v in range(255, 0, -1) if (pb | v) not in shown_idx and (pb | v) not in taken8[(sc, pb)]]
                        if not free8:
                            nopatch['no spare entry'] += 1; continue
                        spare8[key8] = free8[0]; taken8[(sc, pb)].add(free8[0])
                    v8, j8 = r['idx'] & 255, spare8[key8]
                    patches.add((r['vram'], 0, v8 & 15, j8 & 15)); patches.add((r['vram'], 1, v8 >> 4, j8 >> 4))
                    spare_ip[sc][pb | j8] = col
                    got += 1
                    continue
                if r is None or r['kind'] not in ('bg', 'obj') or r.get('half') is None or r['idx'] % 16 != i % 16:
                    continue                          # not a 4bpp background / sprite pixel of this entry (3D, 8bpp sprite, ...)
                if j is None:
                    nopatch['no spare entry'] += 1; continue
                if dsrender.shown(bid, r) != tuple(v >> 3 for v in idshot.getpixel((x, y))):
                    continue                          # the render doesn't match the game at this pixel: don't touch it
                tw = tile_want[(r['vram'], r['half'])]
                # (every place the tile pixel is drawn must want that colour: a tile drawn elsewhere in the owner's other
                # colour, e.g. mirrored halves of a picture, gets map patches per cell instead)
                if sc not in NOMAP and tw and (len(tw) > 1 or tw.most_common(1)[0][0] != (col & 31, col >> 5 & 31, col >> 10 & 31)):
                    nopatch['tile drawn elsewhere in another colour'] += 1; continue
                if not static_tile(bid, r['vram']):
                    nopatch['runtime-drawn tile (text, numbers, sprites)'] += 1; continue
                patches.add((r['vram'], r['half'], i % 16, j)); got += 1
                spare_ip[sc][(r['idx'] & ~15) | j] = col          # the spare entry's colour, straight into palette RAM
            nopatch['not a 4bpp tile pixel'] += len(pts) - got
            if got and j is not None:
                screens[sc]['cols']['%d,%d,%d@%d' % (n, f, row * 16 + j, ctx)] = int(''.join(map(str, [col])) if isinstance(col, int) else col)
    screens[sc]['vpatch'] = sorted(patches)
print('tile patches:', sum(len(s_['vpatch']) for s_ in screens.values() if 'vpatch' in s_), 'not patchable px:', dict(nopatch))

# palette index patches: with the AMOLED build's own capture of the screen (caps/am_*), every pixel's top layer and
# palette RAM entry and its colour effect; the colour that entry needs for the owner's pixel (worked back through
# dimming / blending); where the AMOLED build doesn't have it, a patch (most pixels of an entry decide)
ipstat = collections.Counter()
learned_files = {(n, f) for (n, f, i, ctx) in cvotes}
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
    tpatch = {(a_, h_): (f_, t_) for a_, h_, f_, t_ in screens[sc].get('vpatch', [])}
    cellpx = collections.defaultdict(list)       # map entry -> [(tile byte, half, palette index, colour wanted)]
    kept = collections.Counter()                 # idx -> pixels where the owner kept the game's colour (and AMOLED has it)
    cellw = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))   # map entry -> idx -> colours
    cellv = {}
    idlog = json.load(open(CAPS + 'id_' + cap + '.json')) if os.path.exists(CAPS + 'id_' + cap + '.json') else None
    idshot_ip = Image.open(CAPS + 'id_' + cap + '.png').convert('RGB') if idlog is not None else None
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
    # every palette row the colour-id capture shows, and the palette file it holds (rows of art files are never patched)
    rowfile = {}
    if scr_id is not None and os.path.exists(CAPS + 'id_' + cap + '.json'):
        _log = json.load(open(CAPS + 'id_' + cap + '.json')); _im = Image.open(CAPS + 'id_' + cap + '.png').convert('RGB')
        for yy in range(384):
            for xx in range(256):
                q = scr_id[yy // 192][yy % 192][xx]
                if q is None: continue
                sl = c5(_im.getpixel((xx, yy))) >> 8
                if sl < min(64, _log['count']):
                    rowfile[q['idx'] // 16] = (_log['narc'][sl], _log['file'][sl] & 0xFFF)
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
            # (the capture shows the game's tiles; this screen's tile patches move some pixels to spare entries)
            if px.get('vram') is not None and tpatch:
                if px.get('half') is not None:
                    tp = tpatch.get((px['vram'], px['half']))
                    if tp is not None and px['idx'] % 16 == tp[0]:
                        px = dict(px); px['idx'] = (px['idx'] & ~15) | tp[1]
                else:
                    lo, hi = tpatch.get((px['vram'], 0)), tpatch.get((px['vram'], 1))
                    if lo is not None and hi is not None and px['idx'] % 256 == lo[0] | hi[0] << 4:
                        px = dict(px); px['idx'] = (px['idx'] & ~255) | lo[1] | hi[1] << 4
            op = o.getpixel((x, y)); ow = (o1(op[0]), o1(op[1]), o1(op[2]))
            px = game_effect(px, x, y)
            if off[y // 192]:
                px = dict(px); px['effect'] = 0
            t = dsrender.needed(b, px, ow)
            if t is None:
                ipstat['blend can not reach it'] += 1; continue
            if idlog is not None:
                ic = c5(idshot_ip.getpixel((x, y))); slot = ic >> 8
                if slot < min(64, idlog['count']) and not ui_file(idlog['narc'][slot], idlog['file'][slot] & 0xFFF, learned_files):
                    ipstat['not a UI palette (art, battle background)'] += 1; continue
            if rowfile.get(px['idx'] // 16) == (8, 71) and px['idx'] % 16 in (2, 14) and (t[0] | t[1] << 5 | t[2] << 10) in HP_TEXT:
                continue                              # (HP box text: the overlay's, see above)
            want[px['idx']][t[0] | t[1] << 5 | t[2] << 10] += 1
            # (the owner kept the game's colour here, and the AMOLED build shows it: a colour the game may change by
            # itself, e.g. the battle background by time of day, is left to the game)
            dp = d.getpixel((x, y))
            if ow == tuple(v >> 3 for v in dp) and tuple(v >> 3 for v in g) == ow:
                kept[px['idx']] += 1
            # (the AMOLED captures are made without the screen pages, NOPAGES=1: cells and tiles as the game drew them)
            qc = px
            if qc is not None and qc.get('map') is not None:
                cellv[qc['map']] = qc.get('vram')
                cellw[qc['map']][qc['idx']][t[0] | t[1] << 5 | t[2] << 10] += 1
                if qc.get('half') is not None:
                    cellpx[qc['map']].append((qc['vram'], qc['half'], qc['idx'], t[0] | t[1] << 5 | t[2] << 10))
    ip = []
    for idx, cnt in want.items():              # every entry the screen shows (undone when the screen changes)
        # (only rows no themed palette file fills: the theme's own palette colours need their patches)
        if kept[idx] == sum(cnt.values()) and idx // 16 not in rowfile:
            ipstat['kept by the owner: left to the game'] += 1; continue
        ip.append((idx, cnt.most_common(1)[0][0]))
    # map patches: a background cell (one tile map entry) whose pixels want, for one of its entries, another colour
    # than the screen gives that entry (a tile drawn in several places, coloured differently by the owner) is pointed
    # at a palette row of its own: one this screen doesn't show, set to the row's colours with the cell's differences
    major = {i: c.most_common(1)[0][0] for i, c in want.items()}
    shown_rows = {q['idx'] // 16 for half in scr for line in half for q in line if q}
    groups, taken_rows = {}, set()
    clone_cells, clone_spare, clone_taken, used_tiles, mapped = set(), {}, set(), {}, set()
    for maddr, ents in (sorted(cellw.items()) if sc not in NOMAP else []):
        ov = {}
        for i, cnt in ents.items():
            c, k = cnt.most_common(1)[0]
            if c != major.get(i) and k >= 2 and 2 * k > sum(cnt.values()):
                ov[i % 16] = c
        if not ov: continue
        # (runtime-drawn tiles too: a map patch only points the cell at another palette row, its pixels stay what the
        # game drew; the screen's fingerprint includes the tile maps, so the cell is that same button / label)
        if cellv.get(maddr) is None:
            ipstat['map patch: no tile'] += 1; continue
        base = next(iter(ents)) & ~15; eb = base & ~255
        key = (base, tuple(sorted(ov.items())))
        if key not in groups:
            free = [r for r in range(1, 16) if (eb // 16 + r) not in shown_rows and (eb // 16 + r) not in taken_rows
                    and (eb // 16 + r) not in rowfile]
            if not free:
                clone_cells.add(maddr)
                ipstat['map patch: no free row (tile copy instead)'] += 1; continue
            r = eb // 16 + free[0]; taken_rows.add(r); groups[key] = r
            for j in range(16):
                ip.append((r * 16 + j, ov.get(j, major.get(base + j, dsrender.colour(b, base + j)))))
        screens[sc].setdefault('vpatch', []).append((maddr + 1, 1, (base - eb) // 16, groups[key] - eb // 16))
        mapped.add(maddr)
        ipstat['map patches'] += 1
    ip = [(i, c) for i, c in ip if i // 16 not in rowfile or ui_file(*rowfile[i // 16], learned_files)]
    done = {i for i, _ in ip}
    ip += [(i, c) for i, c in spare_ip.get(sc, {}).items() if i not in done]
    # tile copies: a cell (of the game's own graphics) whose pixels want colours its tile can't give where it is
    # (no free palette row for a map patch, or one entry wanted in two colours inside the cell: a gradient drawn over
    # a flat area) gets a copy of its tile in a tile slot no map uses, its pixels pointed at the entries of the row
    # that have the colours wanted (or a spare entry), and the cell pointed at the copy. All as tile patches (the copy
    # is written into a blank tile; the cell's map entry gets the copy's tile number).
    if sc not in NOMAP:
        vid = b
        def rd(addr, n):
            o = (dsrender.O_BGM + addr - 0x06000000) if addr < 0x06200000 else (dsrender.O_BGS + addr - 0x06200000)
            return vid[o:o + n]
        shown_idx_am = {q['idx'] for half_ in scr for line in half_ for q in line if q}
        # the colour every entry will show on this screen: its index patch, else what the game loads (this capture)
        final = {i: dsrender.colour(b, i) & 0x7FFF for i in range(1024)}
        final.update(dict(ip)); final_set = {i for i, _ in ip}
        for maddr in sorted(cellpx):
            pts = cellpx[maddr]
            ents = cellw[maddr]
            mixed = any(len(cnt) > 1 for cnt in ents.values())
            if maddr not in clone_cells and not (mixed and any(c != final.get(i) for i, cnt in ents.items() for c in cnt)):
                continue
            if maddr in mapped:
                continue                              # (pointed at a palette row of its own already)
            if not all(static_tile(vid, v) for v, _, _, _ in pts):
                ipstat['tile copy: runtime-drawn tile'] += 1; continue
            entry = int.from_bytes(rd(maddr, 2), 'little'); tile = entry & 0x3FF
            tbase = pts[0][0] & ~31
            cbase = tbase - tile * 32
            row = pts[0][2] & ~15
            # each pixel: the entry of the row that has the colour wanted
            new = bytearray(rd(tbase, 32)); changed = False; ok = True
            for k in range(64):                       # (the tile's own patches go with the copy)
                tp = tpatch.get((tbase + k // 2, k & 1))
                if tp is not None and (new[k // 2] >> (4 * (k & 1)) & 15) == tp[0]:
                    new[k // 2] = (new[k // 2] & (0xF0 if k & 1 == 0 else 0x0F)) | tp[1] << (4 * (k & 1))
            for v, h, idx, w in pts:
                if final.get(idx) == w:
                    continue
                e = next((j for j in range(1, 16) if final.get(row | j) == w), None)
                if e is None:
                    e = clone_spare.get((row, w))
                    if e is None:
                        fr = [j for j in range(15, 0, -1) if (row | j) not in shown_idx_am and (row | j) not in clone_taken
                              and (row | j) not in final_set]
                        if not fr:
                            ok = False; break
                        e = fr[0]; clone_taken.add(row | e); clone_spare[(row, w)] = e; ip.append((row | e, w)); final[row | e] = w
                off = v - tbase
                new[off] = (new[off] & 0xF0) | e if h == 0 else (new[off] & 0x0F) | e << 4
                changed = True
            if not ok:
                ipstat['tile copy: no spare entry'] += 1; continue
            if not changed:
                continue
            # a blank tile slot no map of this engine's backgrounds points at
            if cbase not in used_tiles:
                eng = 0 if maddr < 0x06200000 else 1
                ref = set()
                io = vid[0:0x70] if eng == 0 else vid[0x70:0xE0]
                disp = int.from_bytes(io[0:4], 'little')
                for bg in range(4):
                    if not disp & (0x100 << bg): continue
                    cnt = int.from_bytes(io[8 + 2 * bg:10 + 2 * bg], 'little')
                    mb = (0x06000000 + ((disp >> 27) & 7) * 0x10000 if eng == 0 else 0x06200000) + ((cnt >> 8) & 31) * 0x800
                    cb = (0x06000000 + ((disp >> 24) & 7) * 0x10000 if eng == 0 else 0x06200000) + ((cnt >> 2) & 15) * 0x4000
                    size = {0: 0x800, 1: 0x1000, 2: 0x1000, 3: 0x2000}[cnt >> 14]
                    m = rd(mb, size)
                    for q in range(0, len(m), 2):
                        ref.add(cb + (int.from_bytes(m[q:q + 2], 'little') & 0x3FF) * 32)
                top = 0x06080000 if eng == 0 else 0x06220000
                # (never inside a tile map: maps share the VRAM with the tile graphics, and an empty part of a map
                # looks like a blank tile. Every background's map, shown or not, whole size)
                maps = []
                for bg in range(4):
                    cnt = int.from_bytes(io[8 + 2 * bg:10 + 2 * bg], 'little')
                    mb = (0x06000000 + ((disp >> 27) & 7) * 0x10000 if eng == 0 else 0x06200000) + ((cnt >> 8) & 31) * 0x800
                    maps.append((mb, mb + {0: 0x800, 1: 0x1000, 2: 0x1000, 3: 0x2000}[cnt >> 14]))
                used_tiles[cbase] = [cbase + t * 32 for t in range(1023, 0, -1)
                                     if cbase + t * 32 + 32 <= top and cbase + t * 32 not in ref and not any(rd(cbase + t * 32, 32))
                                     and not any(a_ <= cbase + t * 32 < b_ for a_, b_ in maps)]
            if not used_tiles[cbase]:
                ipstat['tile copy: no blank tile'] += 1; continue
            naddr = used_tiles[cbase].pop(0); ntile = (naddr - cbase) // 32
            vp = screens[sc].setdefault('vpatch', [])
            if len(vp) + 70 > 7500:                   # (the theme loads at most 8192 patches per screen)
                ipstat['tile copy: screen has too many patches'] += 1; continue
            for k in range(64):
                val = new[k // 2] >> (4 * (k & 1)) & 15
                if val:
                    vp.append((naddr + k // 2, k & 1, 0, val))
            nentry = (entry & ~0x3FF) | ntile
            for k in range(4):
                a_, b_ = entry >> (4 * k) & 15, nentry >> (4 * k) & 15
                if a_ != b_:
                    vp.append((maddr + k // 2, k & 1, a_, b_))
            ipstat['tile copies'] += 1
    screens[sc]['ipatch'] = ip
    ipstat['patches'] += len(ip)
# colour links: pictures of one screen (one fingerprint) that want an entry in different colours (the bag's pockets:
# each pocket's colour) get it linked to the entry that holds that colour in each of them (the pocket's own palette
# row, loaded by the game): the theme copies that entry's colour every frame (src/rh_theme.c IdxWant)
by_sig = collections.defaultdict(list)
for sc_ in screens:
    by_sig[screens[sc_]['sig']].append(sc_)
for sig_, names in by_sig.items():
    if len(names) < 2:
        continue
    pals = {}
    for n_ in names:
        f_ = CAPS + 'am_' + MATCH[n_] + '.vram2'
        if os.path.exists(f_):
            vb = open(f_, 'rb').read()
            pals[n_] = [dsrender.colour(vb, i_) & 0x7FFF for i_ in range(1024)]
    if len(pals) < len(names):
        continue
    wants = collections.defaultdict(dict)
    for n_ in names:
        for i_, c_ in screens[n_].get('ipatch', []):
            wants[i_][n_] = c_
    for i_, byn in wants.items():
        if len(byn) < len(names) or len(set(byn.values())) < 2:
            continue
        src = next((s_ for s_ in range(1024) if s_ != i_ and s_ // 256 == i_ // 256 and all(pals[n_][s_] == byn[n_] for n_ in names)), None)
        if src is None:
            ipstat['link: no entry has the colours'] += 1; continue
        for n_ in names:
            screens[n_].setdefault('link', []).append((i_, src))
        ipstat['colour links'] += 1
print('palette index patches:', dict(ipstat))
json.dump(screens, open('screens.json', 'w'))
# entries whose text pixels the owner coloured differently from other pixels: every screen (file level) gets the text's
for k, c in RUNTIME_MAJOR.items():
    out[k] = {str(c): 10 ** 6}
json.dump(out, open(sys.argv[1], 'w'))
json.dump(screens, open('screens.json', 'w'))
json.dump({name: cap for name, cap in MATCH.items()}, open('matches.json', 'w'))
print(len(out), 'entries')
