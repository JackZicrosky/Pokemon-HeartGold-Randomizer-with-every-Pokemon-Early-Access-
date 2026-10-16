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
            if not disp & (0x100 << bg): continue
            cnt = struct.unpack_from('<H', io[eng], 8 + 2 * bg)[0]
            base = (bgs if eng else bgm + ((disp >> 27) & 7) * 0x10000) + ((cnt >> 8) & 31) * 0x800
            h = ((h ^ cnt ^ bg << 16 ^ eng << 20) * 16777619) & 0xFFFFFFFF
            for i in range(0x600 // 4):
                w = struct.unpack_from('<I', b, base + 4 * i)[0]
                if mask: w &= 0x0FFF0FFF
                h = ((h ^ w) * 16777619) & 0xFFFFFFFF
    return h
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
