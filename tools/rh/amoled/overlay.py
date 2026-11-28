# overlay.py : overlay sprites (rhtheme/ ObjCopies, w0 bits 14 / 15) for pixels no palette row can give.
# For each picture in OVERLAY: the AMOLED recording's frame (with the theme's pages) closest to the owner's picture
# inside the listed boxes; every pixel there that differs from the owner's becomes a pixel of an 8x8 overlay sprite in
# the owner's colour (a free OBJ palette row of the screen's engine, free OBJ VRAM above the game's sprites). Writes
# overlays.json, which learn.py merges into the screens' sprite copies and index patches.
# Only for screens whose content in those boxes is static (the trainer card's leader portraits).
import json, os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dsrender

A = '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED/'
CAPS = os.path.dirname(os.path.abspath(__file__)) + '/caps/'
REC = os.path.dirname(os.path.abspath(__file__)) + '/rec/'
M = json.load(open('matches.json'))
# picture -> boxes (screen coordinates, both screens stacked: top 0-191)
OVERLAY = {
    '32_trainer_card_front.png': [(0, 40, 256, 128)],
    '33_trainer_card_back.png': [(0, 40, 256, 128), (32, 0, 100, 24)],
    # the save prompt's band: its first line takes the game's colour only when nothing delays the game's own work
    # for it in the vertical blank (timing): drawn by the theme's sprites instead, the same every frame
    '34_save_prompt.png': [(0, 138, 256, 146)],
    # the battle command menu's arcs under FIGHT (the owner's colours on a blended layer the theme can't reach)
    # the options menu's FRAME value box: its top line and side columns, the owner's gradient colour where the box's
    # entry has its own (one entry per line: the scanline colours can't give two on a line)
    '35_options_menu.png': [(144, 338, 216, 348)],
    '66_battle_command_menu.png': [(64, 306, 192, 336)],
    '67_battle_fight_pressed.png': [(64, 306, 192, 336), (0, 196, 256, 212), (0, 340, 256, 384)],
    '68_battle_bag_pressed.png': [(64, 306, 192, 336), (30, 238, 36, 280), (220, 238, 226, 280), (30, 308, 36, 314),
                                  (220, 308, 226, 314), (170, 364, 256, 368)],
}
# (only the pictures named on the command line are made again: the others' overlays are kept - a recording that
# already shows an overlay would make it empty)
ONLY = set(sys.argv[1:])


def o1(v):
    return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))


out = json.load(open('overlays.json')) if os.path.exists('overlays.json') else {}
for pic, boxes in OVERLAY.items():
    if ONLY and pic not in ONLY:
        continue
    # (the pixels of the overlay made before: kept - the recording may show them already, then they aren't a
    # difference any more)
    old_px = {}
    prev = out.pop(pic, None)
    if prev:
        vb0 = open(CAPS + 'am_' + M[pic] + '.vram2', 'rb').read()
        dsrender.screens(vb0, Image.open(CAPS + 'am_' + M[pic] + '.png').convert('RGB'))   # (which engine is where)
        cols0 = {i % 256: c for i, c in prev['ipatch']}         # (row * 16 + entry within the engine's OBJ palette)
        words, half0 = {}, None
        for a, b in prev['objcopy']:
            if a >> 15 & 1:
                words[a >> 16] = b
            elif a >> 14 & 1:
                eng0 = a & 1
                io0 = vb0[0x70:0xE0] if eng0 else vb0[0:0x70]
                sh0 = 5 + (int.from_bytes(io0[0:4], 'little') >> 20 & 3)
                t0, x0, y0 = b >> 9 & 1023, b & 511, a >> 16 & 255
                half0 = [h_ for h_ in (0, 1) if dsrender.ENGINES[0][h_] == eng0][0]
                for yy in range(8):
                    for xx in range(8):
                        w = words.get(((t0 << sh0) + yy * 4 + xx // 8 * 0) // 4 + 0, 0) if False else words.get((t0 << sh0) // 4 + yy, 0)
                        v = w >> (4 * xx) & 15
                        if v and ((b >> 19 & 15) * 16 + v) in cols0:
                            c = cols0[(b >> 19 & 15) * 16 + v]
                            old_px[(x0 + xx, half0 * 192 + y0 + yy)] = (c & 31, c >> 5 & 31, c >> 10 & 31)
    cap = M[pic]
    own = Image.open(A + pic).convert('RGB')
    d = REC + 'am_px_' + cap + '/'
    frames = sorted(set(json.load(open(d + 'index.json'))['frames']))
    best = None
    for f in frames:
        im = Image.open(d + f).convert('RGB')
        diff = []
        for (x0, y0, x1, y1) in boxes:
            for y in range(y0, y1):
                for x in range(x0, x1):
                    w = tuple(o1(v) for v in own.getpixel((x, y)))
                    g = tuple(v >> 3 for v in im.getpixel((x, y)))
                    if w != g:
                        diff.append((x, y, w))
        if best is None or len(diff) < len(best):
            best = diff
    best = best or []
    # only the pixels the proof counts wrong (proof3/NAME.png, third panel pure red): where the owner kept the game's
    # colour the AMOLED frame must match the Default frame, not the owner's picture of another moment
    pf = 'proof3/' + pic
    if os.path.exists(pf):
        pim = Image.open(pf).convert('RGB')
        red = {(x, y) for (x0, y0, x1, y1) in boxes for y in range(y0, y1) for x in range(x0, x1)
               if pim.getpixel((512 + x, y)) == (255, 0, 0)}
        best = [(x, y, w) for x, y, w in best if (x, y) in red]
    else:
        best = []                                   # (proven: nothing new)
    have = {(x, y) for x, y, _ in best}
    best += [(x, y, w) for (x, y), w in old_px.items() if (x, y) not in have]
    if not best:
        continue
    vb = open(CAPS + 'am_' + cap + '.vram2', 'rb').read()
    # (which engine shows on which screen: the options menu has the main engine on the bottom one)
    dsrender.screens(vb, Image.open(CAPS + 'am_' + cap + '.png').convert('RGB'))
    half = best[0][1] // 192
    if any(y // 192 != half for _, y, _ in best):
        print(pic, 'boxes span both screens: skipped'); continue
    eng = dsrender.ENGINES[0][half]
    io = vb[0x70:0xE0] if eng else vb[0:0x70]
    sh = 5 + (int.from_bytes(io[0:4], 'little') >> 20 & 3)
    oam = vb[dsrender.O_OAM + 0x400 * eng:dsrender.O_OAM + 0x400 * eng + 0x400]
    SZ = {(0, 0): (8, 8), (0, 1): (16, 16), (0, 2): (32, 32), (0, 3): (64, 64), (1, 0): (16, 8), (1, 1): (32, 8),
          (1, 2): (32, 16), (1, 3): (64, 32), (2, 0): (8, 16), (2, 1): (8, 32), (2, 2): (16, 32), (2, 3): (32, 64)}
    used_rows, ranges = set(), []
    for k in range(128):
        a0, a1, a2 = (int.from_bytes(oam[k * 8 + 2 * w:k * 8 + 2 * w + 2], 'little') for w in range(3))
        if (a0 & 0x300) == 0x200 or (a0 & 255) == 192:
            continue
        used_rows.add(a2 >> 12)
        st = (a2 & 1023) << sh
        sw, shh = SZ.get((a0 >> 14, a1 >> 14), (64, 64))
        ranges.append((st, st + sw * shh // 2))
    # (the screen's own sprite tile copies - learn.py: a label's pixel copies - are taken too)
    scr_js = json.load(open('screens.json')).get(pic, {})
    for a_, b_ in scr_js.get('objcopy', []):
        if not (a_ >> 12 & 1) and not (a_ >> 13 & 1) and not (a_ >> 14 & 3) and (b_ >> 16 & 255) and (a_ & 1) == eng:
            ranges.append(((b_ & 1023) << sh, ((b_ & 1023) << sh) + (b_ >> 16 & 255) * 32))
    import collections as _c
    cnt = _c.Counter(w for _, _, w in best)
    cols = [c for c, _ in cnt.most_common()]
    free_rows = [r for r in range(15, 0, -1) if r not in used_rows]
    groups = [cols[i:i + 15] for i in range(0, len(cols), 15)]       # (more than 15 colours: another row)
    if len(groups) > len(free_rows):
        print(pic, 'not enough free palette rows for', len(cols), 'colours'); groups = groups[:len(free_rows)]
    ent = {}
    rows_ = []
    for g_, grp in enumerate(groups):
        rows_.append(free_rows[g_])
        for i, c in enumerate(grp):
            ent[c] = (g_, i + 1)
    row = rows_[0]
    cells = {}
    for x, y, w in best:
        if w in ent:
            g_, v_ = ent[w]
            cells.setdefault((x // 8, (y % 192) // 8, g_), {})[(x % 8, y % 8)] = v_
    ooff = dsrender.O_OBJS if eng else dsrender.O_OBJM
    # from the top of the first 16 KB of OBJ VRAM (always mapped when sprites show) down: the game allocates its
    # sprites' tiles from the bottom up, and loads more later (66: tiles free in the capture were the game's again)
    t = (0x4000 >> sh) - 1
    entries, ip = [], []
    base0 = 768 if eng else 256
    for c, (g_, i) in ent.items():
        ip.append((base0 + rows_[g_] * 16 + i, c[0] | c[1] << 5 | c[2] << 10))
    for (cx, cy, g_), px in sorted(cells.items()):
        while t > 0 and (any(vb[ooff + (t << sh):ooff + (t << sh) + 32])
                         or any(a_ < (t << sh) + 32 and (t << sh) < b_ for a_, b_ in ranges)):
            t -= 1
        if t <= 0:
            print(pic, 'no free OBJ VRAM'); break
        data = bytearray(32)
        for (x, y), v in px.items():
            o = y * 4 + x // 2
            data[o] |= v << (4 * (x & 1))
        for wd in range(8):
            val = int.from_bytes(data[wd * 4:wd * 4 + 4], 'little')
            if val:
                entries.append((1 << 15 | eng | ((t << sh) // 4 + wd) << 16, val))
        entries.append((1 << 14 | eng | 0 << 1 | 0 << 4 | 0 << 6 | (cy * 8) << 16, cx * 8 | t << 9 | rows_[g_] << 19))
        t -= 1
    out[pic] = {'objcopy': entries, 'ipatch': ip}
    print(pic, 'overlay pixels', len(best), 'sprites', len(cells), 'entries', len(entries), 'rows', rows_, 'colours', len(cols))
json.dump(out, open('overlays.json', 'w'))
