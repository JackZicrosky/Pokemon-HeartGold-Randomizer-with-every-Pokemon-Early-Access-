#!/usr/bin/env python3
# Romhack: builds data/rh/rh_amoled.bin, the AMOLED UI Theme's replacement palettes (read by src/rh_theme.c).
#
# tools/rh/amoled/learned.json holds the owner's design: for palette entries "narc,file,index" the colours seen at
# those entries in the owner's edited screenshots ("UI screenshots - AMOLED"), with how many pixels showed each
# (made by matching colour-id captures of every screen with the edited pictures, see tools/rh/amoled/README.md).
# Every palette file of the UI archives is then rebuilt:
#   - entries the owner coloured: the colour most pixels had;
#   - the other entries (screens / buttons states not in the pictures): the owner's colour for the same original
#     colour in the same file, else what the owner did at the same position of a sibling palette (another row of the
#     file, or a same-size file of the archive: same change of lightness, own hue), else the owner's colour for the
#     same original colour in the same archive, else anywhere (when the owner was consistent about it),
#     else the owner's colour of the nearest original colour the owner recoloured, shifted by the difference;
#     never an automatic inversion. Colours far from anything the owner recoloured stay as they are.
# Only UI archives are filled: those where the owner changed something, plus EXTRA_UI (UI screens the pictures don't
# show); every other archive (Pokemon, trainers, items, overworld, battle backgrounds, ...) is only changed where the
# owner changed it.
# File layout: see src/rh_theme.c (AmoledFind).
# Usage (repository root, WSL): python3 tools/rh/gen_rh_amoled.py
import collections, json, os, struct, sys

ROOT = 'base/root/'
LEARNED = 'tools/rh/amoled/learned.json'
OUT = 'data/rh/rh_amoled.bin'
ART = {4, 114, 194, 195, 18, 20, 69, 46, 174, 264, 81, 8, 120}
EXTRA_UI = set()
NEAR = 3            # max channel distance for "nearest recoloured colour"
CONSISTENT = 0.75   # share of votes the most common target needs for the per-archive / global maps


def lz(b):
    size = struct.unpack_from('<I', b, 0)[0] >> 8
    out = bytearray(); i = 4
    while len(out) < size:
        f = b[i]; i += 1
        for _ in range(8):
            if len(out) >= size:
                break
            if f & 0x80:
                x = b[i] << 8 | b[i + 1]; i += 2
                n, dist = (x >> 12) + 3, (x & 0xFFF) + 1
                for _ in range(n):
                    out.append(out[-dist])
            else:
                out.append(b[i]); i += 1
            f <<= 1
    return bytes(out)


def members(path):
    d = open(path, 'rb').read()
    if d[:4] != b'NARC':
        return []
    o = struct.unpack_from('<H', d, 12)[0]
    n = struct.unpack_from('<I', d, o + 8)[0]
    fat = [struct.unpack_from('<II', d, o + 12 + 8 * i) for i in range(n)]
    o += struct.unpack_from('<I', d, o + 4)[0]
    o += struct.unpack_from('<I', d, o + 4)[0]
    base = o + 8
    return [d[base + s:base + e] for s, e in fat]


STARTS = {}     # (narc, file) -> where the colours start in the palette file


def colours(b, key=None):
    if b[:1] == b'\x10':
        try:
            b = lz(b)
        except Exception:
            return None
    if b[:4] != b'RLCN':
        return None
    size = struct.unpack_from('<I', b, 0x20)[0]
    start = 0x18 + struct.unpack_from('<I', b, 0x24)[0]
    size = min(size, len(b) - start)
    if key is not None:
        STARTS[key] = start
    return [struct.unpack_from('<H', b, start + 2 * i)[0] for i in range(size // 2)]


def rgb(c):
    return c & 31, c >> 5 & 31, c >> 10 & 31


def pack(r, g, b):
    return max(0, min(31, r)) | max(0, min(31, g)) << 5 | max(0, min(31, b)) << 10


def main():
    learned = json.load(open(LEARNED))
    owner = {}                                   # (narc, file, idx) -> colour
    for k, v in learned.items():
        owner[tuple(map(int, k.split(',')))] = int(max(v.items(), key=lambda kv: kv[1])[0])
    narcs = sorted({k[0] for k in owner} | {n for n in range(0, 300) if os.path.exists(ROOT + 'a/%d/%d/%d' % (n // 100, n // 10 % 10, n % 10))})
    pals = {}
    for n in narcs:
        p = ROOT + 'a/%d/%d/%d' % (n // 100, n // 10 % 10, n % 10)
        if not os.path.exists(p):
            continue
        for f, b in enumerate(members(p)):
            c = colours(b, (n, f))
            if c:
                pals[(n, f)] = c
    # original colour -> owner colour maps: per file, per archive, global
    per_file = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
    per_narc = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
    glob_map = collections.defaultdict(collections.Counter)
    for (n, f, i), t in owner.items():
        c = pals.get((n, f))
        if c is None or i >= len(c):
            continue
        o = c[i] & 0x7FFF
        w = sum(learned['%d,%d,%d' % (n, f, i)].values())
        per_file[(n, f)][o][t] += w
        per_narc[n][o][t] += w
        glob_map[o][t] += w

    def consistent(counter):
        t, w = counter.most_common(1)[0]
        return t if w >= CONSISTENT * sum(counter.values()) else None
    narc_map = {n: {o: consistent(cn) for o, cn in m.items()} for n, m in per_narc.items()}
    g_map = {o: consistent(cn) for o, cn in glob_map.items()}
    g_keys = [o for o, t in g_map.items() if t is not None and t != o]

    # the same position (index within its row of 16) in sibling palettes the owner coloured: other rows of the same
    # file, or same-size files of the same archive (button states, Pokegear skins, ...)
    siblings = collections.defaultdict(list)     # (narc, palette length, index % 16) -> [(original, owner colour, file)]
    for (n, f, i), t in owner.items():
        c = pals.get((n, f))
        if c is not None and i < len(c):
            siblings[(n, len(c), i % 16)].append((c[i] & 0x7FFF, t, f))

    def lum(c):
        r, g, b = rgb(c)
        return 0.3 * r + 0.59 * g + 0.11 * b

    def sibling(n, f, i, o):
        cands = siblings.get((n, len(pals[(n, f)]), i % 16))
        if not cands:
            return None
        r, g, b = rgb(o)
        # nearest original colour, preferring the same file
        def dist(cand):
            kr, kg, kb = rgb(cand[0])
            return (cand[2] != f, abs(r - kr) + abs(g - kg) + abs(b - kb))
        so, st, _ = min(cands, key=dist)
        if so == st:
            return o                                  # the owner kept that colour: keep this one too
        lo = lum(so)
        if lo == 0:
            return st
        k = lum(st) / lo                              # same lightness change, own hue
        return pack(round(r * k), round(g * k), round(b * k))

    def fill(n, f, o, i=None):
        m = per_file.get((n, f))
        if m and o in m:
            return m[o].most_common(1)[0][0]
        if i is not None:
            t = sibling(n, f, i, o)
            if t is not None:
                return t
        t = narc_map.get(n, {}).get(o)
        if t is not None:
            return t
        t = g_map.get(o)
        if t is not None:
            return t
        r, g, b = rgb(o)
        best, bd = None, NEAR + 1
        for k in g_keys:
            kr, kg, kb = rgb(k)
            d = max(abs(r - kr), abs(g - kg), abs(b - kb))
            if d < bd:
                best, bd = k, d
        if best is None:
            return o
        kr, kg, kb = rgb(best); tr, tg, tb = rgb(g_map[best])
        return pack(tr + r - kr, tg + g - kg, tb + b - kb)

    changed = {n for (n, f, i), t in owner.items() if (n, f) in pals and i < len(pals[(n, f)]) and t != pals[(n, f)][i] & 0x7FFF}
    fill_archives = (changed | EXTRA_UI) - ART
    print('filled archives:', sorted(fill_archives))
    entries = []
    stats = collections.Counter()
    for (n, f), c in sorted(pals.items()):
        new = list(c)
        for i, o in enumerate(c):
            if (n, f, i) in owner:
                new[i] = owner[(n, f, i)] | (o & 0x8000); stats['owner'] += 1
            elif n in fill_archives:
                new[i] = fill(n, f, o & 0x7FFF, i) | (o & 0x8000); stats['filled' if new[i] != o else 'kept'] += 1
        # every palette file outside the art archives (the colour-id mode uses the list too), and art files the owner
        # changed
        if (n not in ART and n < 320) or new != c:
            entries.append(((n << 16) | f, STARTS[(n, f)], new))
    entries.sort()
    mask = [0] * 10
    for key, _, _ in entries:
        mask[key >> 21] |= 1 << (key >> 16 & 31)
    data = bytearray()
    offs = []
    base = 48 + 8 * len(entries)
    for key, start, new in entries:
        offs.append(base + len(data))
        data += struct.pack('<II', len(new), start) + struct.pack('<%dH' % len(new), *new)
    out = bytearray(b'RHA2' + struct.pack('<I', len(entries)) + struct.pack('<10I', *mask))
    for (key, _, _), off in zip(entries, offs):
        out += struct.pack('<II', key, off)
    out += data
    open(OUT, 'wb').write(out)
    print('%s: %d palettes, %d bytes; entries: %s' % (OUT, len(entries), len(out), dict(stats)))


if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
    sys.exit(main())
