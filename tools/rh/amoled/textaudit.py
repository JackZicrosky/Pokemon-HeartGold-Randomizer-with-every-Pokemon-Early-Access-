# textaudit.py MODE CAP...  : text (tiles drawn at runtime: not in any ROM graphics file) shown in a dark colour.
# For every capture (caps/MODE_CAP.vram2 + the colour-id twin caps/id_CAP.json/png for the palette file of each pixel):
# runtime-drawn 4bpp tiles of UI palettes; in each tile the most common colour is its background, the rest is glyph
# (text, shadow, numbers). Glyph pixels darker than the owner's shadow grey (5-bit max channel < 10) are counted, per
# palette file and entry. Writes audit/MODE_CAP.png: the screen with those pixels in magenta.
import sys, os, json, collections, pickle
from PIL import Image
import dsrender as R
sys.argv.pop(0); mode = sys.argv.pop(0)
STATIC = pickle.load(open('static_tiles.pkl', 'rb'))
def c5(p): return (p[0] >> 3) | (p[1] >> 3) << 5 | (p[2] >> 3) << 10
def tile_of(dump, addr):
    for base, off, size in ((0x06000000, R.O_BGM, 0x80000), (0x06200000, R.O_BGS, 0x20000),
                            (0x06400000, R.O_OBJM, 0x40000), (0x06600000, R.O_OBJS, 0x20000)):
        if base <= addr < base + size:
            o = off + ((addr - base) & ~31); return dump[o:o + 32]
    return None
os.makedirs('audit', exist_ok=True)
total = collections.Counter()
for cap in sys.argv:
    vf = 'caps/%s_%s.vram2' % (mode, cap)
    if not os.path.exists(vf) or not os.path.exists('caps/id_%s.json' % cap): continue
    b = open(vf, 'rb').read(); shot = Image.open('caps/%s_%s.png' % (mode, cap)).convert('RGB')
    scr, fit = R.screens(b, shot)
    log = json.load(open('caps/id_%s.json' % cap)); idimg = Image.open('caps/id_%s.png' % cap).convert('RGB')
    tiles = collections.defaultdict(list)          # tile address -> [(x, y, colour)]
    for y in range(384):
        for x in range(256):
            if fit[y // 192] < 0.85: continue
            p = scr[y // 192][y % 192][x]
            if p is None or p.get('half') is None or p['effect']: continue
            ic = c5(idimg.getpixel((x, y))); slot = ic >> 8
            if slot >= min(64, log['count']): continue
            n, f = log['narc'][slot], log['file'][slot] & 0xFFF
            if n in (4, 6) or n >= 320 or (n == 7 and f < 246): continue          # Pokemon / trainer art, backgrounds
            t = tile_of(b, p['vram'])
            if t is None or t in STATIC: continue
            col = tuple(v >> 3 for v in shot.getpixel((x, y)))
            tiles[(p['vram'] & ~31, y // 8, x // 8)].append((x, y, col, (n, f, p['idx'] % 16)))
    # text = in Default (same frame), dark pixels in a light cell (the game's text: dark on light boxes); in AMOLED
    # those pixels must be light (white text / grey shadow): still dark = black text
    dshot = Image.open('caps/def_%s.png' % cap).convert('RGB')
    out = shot.copy(); hits = collections.Counter()
    for k, px in tiles.items():
        dc = collections.Counter(tuple(v >> 3 for v in dshot.getpixel((x, y))) for x, y, _, _ in px)
        if len(dc) < 2: continue                    # a blank (one colour) cell: no glyph
        bg = dc.most_common(1)[0][0]
        if max(bg) < 20: continue                   # not a light box in Default
        for x, y, c, e in px:
            d = tuple(v >> 3 for v in dshot.getpixel((x, y)))
            if max(d) <= 14 and max(c) < 20:
                hits[e] += 1; out.putpixel((x, y), (255, 0, 255))
    if hits:
        out.save('audit/%s_%s.png' % (mode, cap))
    n = sum(hits.values()); total[cap] = n
    print('%-18s dark glyph px %5d  %s' % (cap, n, ', '.join('%d/%d#%d:%d' % (*e, v) for e, v in hits.most_common(6))))
