# the game's ScreenSig (src/rh_theme.c) from a capture dump; mask=True: palette bits of map entries left out
import struct, glob, json, os
def sig(b, kind, mask):
    if kind == 'vram2':
        io = [b[0:0x70], b[0x70:0xE0]]; bgm = 0xE0 + 0x800 + 0x800; bgs = bgm + 0x80000
    else:   # .vram: IO main 0x70 | IO sub 0x70 | palette 0x800 | main BG 0x80000 | sub BG 0x20000
        io = [b[0:0x70], b[0x70:0xE0]]; bgm = 0xE0 + 0x800; bgs = bgm + 0x80000
    h = 2166136261
    for eng in (0, 1):
        disp = struct.unpack_from('<I', io[eng], 0)[0]
        for bg in range(4):
            if not disp & (0x100 << bg) or (bg == 0 and eng == 0 and disp & 8): continue   # (3D layer: no map)
            cnt = struct.unpack_from('<H', io[eng], 8 + 2 * bg)[0]
            base = (bgs if eng else bgm + ((disp >> 27) & 7) * 0x10000) + ((cnt >> 8) & 31) * 0x800
            h = ((h ^ cnt ^ bg << 16 ^ eng << 20) * 16777619) & 0xFFFFFFFF
            # each map's own hash (tile numbers as steps from the cell before's), then into the screen's
            hb, prev = 2166136261, 0
            for i in range(0x600 // 2):
                e = struct.unpack_from('<H', b, base + 2 * i)[0]
                t = e & 0x3FF
                if t:
                    e = (e & ~0x3FF) | ((t - prev) & 0x3FF); prev = t
                if mask: e &= 0x0FFF
                hb = ((hb ^ e) * 16777619) & 0xFFFFFFFF
            h = ((h ^ hb) * 16777619) & 0xFFFFFFFF
    return h

def fuzzy(b, kind):
    """the screen's layer setup (hash of every shown layer's engine, number, control register) and, per layer, the
    hashes of its map's 6 regions (rows 0-7 / 8-15 / 16-23 x columns 0-15 / 16-31): tile numbers as steps from the
    region's cell before, palette bits left out (16 bits each). The theme's FuzzyFind (rhtheme/) works the same way:
    a screen whose exact fingerprint has no page takes the page of the screen with the same setup whose regions
    match best (another number of moves, more items in the bag...)"""
    if kind == 'vram2':
        io = [b[0:0x70], b[0x70:0xE0]]; bgm = 0xE0 + 0x800 + 0x800; bgs = bgm + 0x80000
    else:
        io = [b[0:0x70], b[0x70:0xE0]]; bgm = 0xE0 + 0x800; bgs = bgm + 0x80000
    cfg = 2166136261; regs = []
    for eng in (0, 1):
        disp = struct.unpack_from('<I', io[eng], 0)[0]
        for bg in range(4):
            if not disp & (0x100 << bg) or (bg == 0 and eng == 0 and disp & 8): continue
            cnt = struct.unpack_from('<H', io[eng], 8 + 2 * bg)[0]
            base = (bgs if eng else bgm + ((disp >> 27) & 7) * 0x10000) + ((cnt >> 8) & 31) * 0x800
            cfg = ((cfg ^ cnt ^ bg << 16 ^ eng << 20) * 16777619) & 0xFFFFFFFF
            hr = [2166136261] * 6; prev = [0] * 6
            for i in range(0x600 // 2):
                r = (i >> 8) * 2 + ((i & 31) >= 16)
                e = struct.unpack_from('<H', b, base + 2 * i)[0] & 0x0FFF
                t = e & 0x3FF
                if t:
                    e = (e & ~0x3FF) | ((t - prev[r]) & 0x3FF); prev[r] = t
                hr[r] = ((hr[r] ^ e) * 16777619) & 0xFFFFFFFF
            regs += [(h ^ h >> 16) & 0xFFFF for h in hr]
    return cfg, (regs + [0] * 48)[:48]
if __name__ == '__main__':
    ok = bad = 0; new = {}
    for j in sorted(glob.glob('caps/id_*.json')):
        n = j[8:-5]; js = json.load(open(j))
        for kind in ('vram2', 'vram'):
            f = 'caps/id_%s.%s' % (n, kind)
            if os.path.exists(f):
                b = open(f, 'rb').read()
                s0 = sig(b, kind, False)
                if s0 == js.get('sig'): ok += 1
                else: bad += 1
                new[n] = (js.get('sig'), sig(b, kind, True)); break
    print('replica ok', ok, 'mismatch', bad)
    json.dump(new, open('sig_new.json', 'w'))
