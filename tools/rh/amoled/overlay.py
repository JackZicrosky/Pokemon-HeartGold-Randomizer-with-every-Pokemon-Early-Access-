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
    '33_trainer_card_back.png': [(0, 40, 256, 128)],
    # the battle command menu's arcs under FIGHT (the owner's colours on a blended layer the theme can't reach)
    '66_battle_command_menu.png': [(64, 306, 192, 336)],
    '67_battle_fight_pressed.png': [(64, 306, 192, 336)],
    '68_battle_bag_pressed.png': [(64, 306, 192, 336)],
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
    out.pop(pic, None)
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
    if not best:
        continue
    vb = open(CAPS + 'am_' + cap + '.vram2', 'rb').read()
    half = best[0][1] // 192
    if any(y // 192 != half for _, y, _ in best):
        print(pic, 'boxes span both screens: skipped'); continue
    eng = dsrender.ENGINES[0][half]
    io = vb[0x70:0xE0] if eng else vb[0:0x70]
    sh = 5 + (int.from_bytes(io[0:4], 'little') >> 20 & 3)
    oam = vb[dsrender.O_OAM + 0x400 * eng:dsrender.O_OAM + 0x400 * eng + 0x400]
    sz = {(0, 0): 8, (0, 1): 16, (0, 2): 32, (0, 3): 64}
    used_rows, ranges = set(), []
    for k in range(128):
        a0, a1, a2 = (int.from_bytes(oam[k * 8 + 2 * w:k * 8 + 2 * w + 2], 'little') for w in range(3))
        if (a0 & 0x300) == 0x200 or (a0 & 255) == 192:
            continue
        used_rows.add(a2 >> 12)
        st = (a2 & 1023) << sh
        ranges.append((st, st + 64 * 64 // 2))           # (generous: the largest sprite)
    cols = sorted({w for _, _, w in best})
    if len(cols) > 15:
        print(pic, 'more than 15 colours:', len(cols)); continue
    row = next(r for r in range(15, 0, -1) if r not in used_rows)
    ent = {c: i + 1 for i, c in enumerate(cols)}
    cells = {}
    for x, y, w in best:
        cells.setdefault((x // 8, (y % 192) // 8), {})[(x % 8, y % 8)] = ent[w]
    ooff = dsrender.O_OBJS if eng else dsrender.O_OBJM
    t = (max([e for _, e in ranges] or [0]) + (1 << sh) - 1) >> sh
    entries, ip = [], []
    base0 = 768 if eng else 256
    for c, i in ent.items():
        ip.append((base0 + row * 16 + i, c[0] | c[1] << 5 | c[2] << 10))
    for (cx, cy), px in sorted(cells.items()):
        while t < 1024 and any(vb[ooff + (t << sh):ooff + (t << sh) + 32]):
            t += 1
        if t >= 1024:
            print(pic, 'no free OBJ VRAM'); break
        data = bytearray(32)
        for (x, y), v in px.items():
            o = y * 4 + x // 2
            data[o] |= v << (4 * (x & 1))
        for wd in range(8):
            val = int.from_bytes(data[wd * 4:wd * 4 + 4], 'little')
            if val:
                entries.append((1 << 15 | eng | ((t << sh) // 4 + wd) << 16, val))
        entries.append((1 << 14 | eng | 0 << 1 | 0 << 4 | 0 << 6 | (cy * 8) << 16, cx * 8 | t << 9 | row << 19))
        t += 1
    out[pic] = {'objcopy': entries, 'ipatch': ip}
    print(pic, 'overlay pixels', len(best), 'sprites', len(cells), 'entries', len(entries), 'row', row, 'colours', len(cols))
json.dump(out, open('overlays.json', 'w'))
