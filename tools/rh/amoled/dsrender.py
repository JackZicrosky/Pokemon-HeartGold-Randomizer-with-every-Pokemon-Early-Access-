# test only: re-render a DS frame (both engines) from a caps/*.vram2 dump: text BGs + regular sprites, priorities,
# colour effects. For every pixel: the top layer (kind 'bg'/'obj', bg number / oam index), its palette RAM index
# (0-1023, both engines' BG and OBJ palettes: main BG 0-255, main OBJ 256-511, sub BG 512-767, sub OBJ 768-1023),
# its VRAM address and nibble half (4bpp BG only, for tile patches), the colour effect on it and the layer below
# (for alpha blending).
# dump layout (cap.py vram2): IO main 0x70 | IO sub 0x70 | palette 0x800 | OAM 0x800 | main BG VRAM 0x80000 |
# sub BG 0x20000 | main OBJ 0x40000 | sub OBJ 0x20000
import os, struct
O_IOM, O_IOS, O_PAL, O_OAM = 0, 0x70, 0xE0, 0xE0 + 0x800
O_BGM = O_OAM + 0x800
O_BGS = O_BGM + 0x80000
O_OBJM = O_BGS + 0x20000
O_OBJS = O_OBJM + 0x40000
SIZES = {(0, 0): (8, 8), (0, 1): (16, 16), (0, 2): (32, 32), (0, 3): (64, 64),
         (1, 0): (16, 8), (1, 1): (32, 8), (1, 2): (32, 16), (1, 3): (64, 32),
         (2, 0): (8, 16), (2, 1): (8, 32), (2, 2): (16, 32), (2, 3): (32, 64)}


def u16(b, o): return struct.unpack_from('<H', b, o)[0]
def u32(b, o): return struct.unpack_from('<I', b, o)[0]


def colour(b, idx): return u16(b, O_PAL + idx * 2)


def render(b):
    """-> list of 2 engines, each 192 rows x 256 of None or dict"""
    res = []
    for eng in (0, 1):
        io = O_IOM if eng == 0 else O_IOS
        disp = u32(b, io)
        mode = disp & 7
        vbg = O_BGM if eng == 0 else O_BGS
        vobj = O_OBJM if eng == 0 else O_OBJS
        vbgcpu = 0x06000000 if eng == 0 else 0x06200000
        palbg = 0 if eng == 0 else 512
        palobj = 256 if eng == 0 else 768
        bldcnt = u16(b, io + 0x50); bldalpha = u16(b, io + 0x52); bldy = u16(b, io + 0x54) & 31
        layers = []
        for bg in range(4):
            if not disp & (0x100 << bg):
                continue
            if bg == 0 and eng == 0 and disp & 8:
                layers.append((u16(b, io + 8) & 3, bg, None, 0, 0))     # 3D
                continue
            if not (bg < 2 or (bg == 2 and mode in (0, 1)) or (bg == 3 and mode == 0)):
                continue
            cnt = u16(b, io + 8 + 2 * bg)
            layers.append((cnt & 3, bg, cnt, u16(b, io + 0x10 + 4 * bg) & 0x1FF, u16(b, io + 0x12 + 4 * bg) & 0x1FF))
        # sprites, front (low OAM index) first
        sprites = []
        if disp & 0x1000:
            oam = O_OAM + (0 if eng == 0 else 0x400)
            for k in range(128):
                a0, a1, a2 = u16(b, oam + 8 * k), u16(b, oam + 8 * k + 2), u16(b, oam + 8 * k + 4)
                if a0 & 0x100 or (a0 & 0x200):     # affine (unsupported) or hidden
                    continue
                if (a0 >> 10) & 3 in (2, 3):        # window / bitmap sprites
                    continue
                shape, size = a0 >> 14, a1 >> 14
                if shape == 3: continue
                w, h = SIZES[(shape, size)]
                y0, x0 = a0 & 0xFF, a1 & 0x1FF
                if y0 >= 192: y0 -= 256
                if x0 >= 256: x0 -= 512
                sprites.append((k, x0, y0, w, h, a0, a1, a2))
        scr = [[None] * 256 for _ in range(192)]
        for y in range(192):
            for x in range(256):
                cands = []
                for prio, bg, cnt, hofs, vofs in layers:
                    if cnt is None:
                        continue                    # 3D: unknown pixels (counted as opaque 'other' below)
                    size = cnt >> 14
                    w = 512 if size in (1, 3) else 256
                    hh = 512 if size in (2, 3) else 256
                    sx, sy = (x + hofs) % w, (y + vofs) % hh
                    block = (sx // 256) + (sy // 256) * (2 if w == 512 else 1)
                    sbase = ((cnt >> 8) & 31) * 0x800 + (((disp >> 27) & 7) * 0x10000 if eng == 0 else 0)
                    cbase = ((cnt >> 2) & 15) * 0x4000 + (((disp >> 24) & 7) * 0x10000 if eng == 0 else 0)
                    e = u16(b, vbg + sbase + block * 0x800 + (((sy % 256) // 8) * 32 + (sx % 256) // 8) * 2)
                    tile, hf, vf, pl = e & 0x3FF, e >> 10 & 1, e >> 11 & 1, e >> 12
                    px, py = sx % 8, sy % 8
                    if hf: px = 7 - px
                    if vf: py = 7 - py
                    if cnt & 0x80:
                        a = cbase + tile * 64 + py * 8 + px; v = b[vbg + a]
                        if v == 0: continue
                        cands.append((prio, 1, bg, {'kind': 'bg', 'layer': bg, 'idx': palbg + v, 'vram': vbgcpu + a, 'half': None}))
                    else:
                        a = cbase + tile * 32 + py * 4 + px // 2; v = (b[vbg + a] >> (4 * (px & 1))) & 15
                        if v == 0: continue
                        cands.append((prio, 1, bg, {'kind': 'bg', 'layer': bg, 'idx': palbg + pl * 16 + v, 'vram': vbgcpu + a, 'half': px & 1}))
                for (k, x0, y0, w, h, a0, a1, a2) in sprites:
                    if not (x0 <= x < x0 + w and y0 <= y < y0 + h):
                        continue
                    px, py = x - x0, y - y0
                    if a1 & 0x1000: px = w - 1 - px
                    if a1 & 0x2000: py = h - 1 - py
                    tile = a2 & 0x3FF; bpp8 = a0 & 0x2000
                    if disp & 0x10:                 # 1D mapping
                        bound = 32 << ((disp >> 20) & 3)
                        tw = w // 8
                        t = (py // 8) * tw + px // 8
                        base = tile * bound + t * (64 if bpp8 else 32)
                    else:                            # 2D
                        t = tile + (py // 8) * 32 + (px // 8) * (2 if bpp8 else 1)
                        base = t * 32
                    if bpp8:
                        v = b[vobj + (base + (py % 8) * 8 + px % 8) % 0x40000]
                        if v == 0: continue
                        idx = palobj + v
                    else:
                        a = (base + (py % 8) * 4 + (px % 8) // 2) % 0x40000
                        v = (b[vobj + a] >> (4 * (px % 2))) & 15
                        if v == 0: continue
                        idx = palobj + (a2 >> 12) * 16 + v
                        cands.append(((a2 >> 10) & 3, 0, k, {'kind': 'obj', 'layer': 4, 'oam': k, 'idx': idx, 'semi': (a0 >> 10) & 3 == 1,
                                                             'vram': (0x06400000 if eng == 0 else 0x06600000) + a, 'half': px % 2}))
                        break
                    cands.append(((a2 >> 10) & 3, 0, k, {'kind': 'obj', 'layer': 4, 'oam': k, 'idx': idx, 'semi': (a0 >> 10) & 3 == 1, 'half': None}))
                    break    # the first (front) sprite covering the pixel with an opaque pixel
                cands.sort(key=lambda c: (c[0], c[1], c[2]))
                if not cands:
                    scr[y][x] = {'kind': 'backdrop', 'layer': 5, 'idx': palbg, 'below': None, 'effect': 0}
                    continue
                top = dict(cands[0][3])
                below = cands[1][3] if len(cands) > 1 else {'kind': 'backdrop', 'layer': 5, 'idx': palbg}
                eff = (bldcnt >> 6) & 3
                first = bool(bldcnt & (1 << top['layer']))
                second = bool(bldcnt & (0x100 << below['layer']))
                top['below'] = below
                top['effect'] = 0
                if top.get('semi') and second:
                    top['effect'] = ('alpha', bldalpha & 31, (bldalpha >> 8) & 31)
                elif first and eff == 1 and second:
                    top['effect'] = ('alpha', bldalpha & 31, (bldalpha >> 8) & 31)
                elif first and eff == 2:
                    top['effect'] = ('light', bldy)
                elif first and eff == 3:
                    top['effect'] = ('dark', bldy)
                scr[y][x] = top
        res.append(scr)
    return res


def shown(b, px):
    """the colour the hardware shows for a rendered pixel (5-bit rgb tuple), applying the colour effect"""
    c = colour(b, px['idx'])
    rgb = [c & 31, c >> 5 & 31, c >> 10 & 31]
    e = px['effect']
    if e and e[0] == 'alpha':
        c2 = colour(b, px['below']['idx']); rgb2 = [c2 & 31, c2 >> 5 & 31, c2 >> 10 & 31]
        eva, evb = min(e[1], 16), min(e[2], 16)
        rgb = [min(31, (a * eva + c * evb) // 16) for a, c in zip(rgb, rgb2)]
    elif e and e[0] == 'dark':
        rgb = [a - (a * min(e[1], 16)) // 16 for a in rgb]
    elif e and e[0] == 'light':
        rgb = [a + ((31 - a) * min(e[1], 16)) // 16 for a in rgb]
    return tuple(rgb)


def needed(b, px, want):
    """the palette colour (5-bit rgb) that makes this pixel show `want` under its colour effect, or None if impossible"""
    e = px['effect']
    out = []
    for ch in range(3):
        w = want[ch]
        if not e:
            out.append(w); continue
        found = None
        for c in range(32):
            rgb = [0, 0, 0]; rgb[ch] = c
            if e[0] == 'alpha':
                c2 = colour(b, px['below']['idx']); o = (c2 >> (5 * ch)) & 31
                v = min(31, (c * min(e[1], 16) + o * min(e[2], 16)) // 16)
            elif e[0] == 'dark':
                v = c - (c * min(e[1], 16)) // 16
            else:
                v = c + ((31 - c) * min(e[1], 16)) // 16
            if v == w:
                found = c; break
        if found is None:
            return None
        out.append(found)
    return tuple(out)


ENGINES = [(0, 1)]     # the engine order of the last screens() call: (top engine, bottom engine)


def screens(b, shot):
    """the rendered engines in screen order (top, bottom), whichever order matches the screenshot better; and for
    each screen the share of rendered pixels that match it"""
    scr = render(b)
    best = None
    for order in ((0, 1), (1, 0)):
        ok = 0
        halves = []
        for half, eng in enumerate(order):
            hok = tot = 0
            for y in range(0, 192, 2):
                for x in range(0, 256, 2):
                    p = scr[eng][y][x]
                    if p is None: continue
                    tot += 1
                    hok += shown(b, p) == tuple(v >> 3 for v in shot.getpixel((x, y + 192 * half)))
            ok += hok
            halves.append(hok / max(1, tot))
        if best is None or ok > best[0]:
            best = (ok, [scr[order[0]], scr[order[1]]], halves, order)
    ENGINES[0] = best[3]
    return best[1], best[2]
