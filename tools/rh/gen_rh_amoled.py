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
SCREENS = 'tools/rh/amoled/screens.json'      # per screen: fingerprint + the owner's colours (tools/rh/amoled/learn.py)
PAGES_OUT = 'data/rh/rh_amoled_pages.bin'
ART = {4, 114, 194, 195, 18, 20, 69, 46, 174, 264, 81, 8, 120}
EXTRA_UI = set()
# files of filled archives that are not UI: never recoloured unless the owner did (battle backgrounds and platforms,
# one palette per terrain and time of day: the owner kept them)
NOFILL = {(7, f) for f in range(246)}
SKIN_FILES = [(143, 30), (143, 24), (143, 0), (144, 20), (144, 14), (145, 4), (146, 10), (146, 4)]   # Pokegear skin 1 files
NEAR = 3            # max channel distance for "nearest recoloured colour"
CONSISTENT = 0.75
ANY = 0xFFFFFFFF    # entry for any screen (else: a screen context, see src/rh_theme.c)   # share of votes the most common target needs for the per-archive / global maps


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
    raw = json.load(open(LEARNED))             # "narc,file,index@screen context": {colour: pixels}
    learned = collections.defaultdict(collections.Counter)
    by_ctx = collections.defaultdict(dict)       # (narc, file, context) -> {index: colour on that screen}
    for k, v in raw.items():
        e, _, ctx = k.partition('@')
        n, f, i = map(int, e.split(','))
        if ctx:
            by_ctx[(n, f, int(ctx))][i] = int(max(v.items(), key=lambda kv: kv[1])[0])
        for t, w in v.items():
            learned['%d,%d,%d' % (n, f, i)][t] += w
    owner = {}                                   # (narc, file, idx) -> colour, any screen (palettes of unseen screens)
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
    # Pokegear skins 2-6: the same palette layout as skin 1 (the owner's design), one file further per skin. Each entry
    # follows the owner's change of skin 1: made neutral (black / grey / white text) = the same colour; kept = the
    # skin's own colour; darkened or lightened = the skin's colour by the same change of lightness
    for n, base in SKIN_FILES:
        for k in range(1, 6):
            f = base + k
            if (n, base) not in pals or (n, f) not in pals:
                continue
            for i, b in enumerate(pals[(n, base)]):
                if (n, base, i) not in owner or (n, f, i) in owner or i >= len(pals[(n, f)]):
                    continue
                a, a2, o = rgb(b), rgb(pals[(n, f)][i]), rgb(owner[(n, base, i)])
                if owner[(n, base, i)] & 0x7FFF == b & 0x7FFF:
                    owner[(n, f, i)] = pals[(n, f)][i]
                elif max(o) - min(o) <= 3:
                    owner[(n, f, i)] = owner[(n, base, i)]
                else:
                    r = (sum(o) + 1) / (sum(a) + 1)
                    owner[(n, f, i)] = pack(*(round(c * r) for c in a2))
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
    contexts = collections.defaultdict(list)
    for (n, f, ctx) in by_ctx:
        contexts[(n, f)].append(ctx)
    entries = []
    loaded = {}
    stats = collections.Counter()
    for (n, f), c in sorted(pals.items()):
        new = list(c)
        for i, o in enumerate(c):
            if (n, f, i) in owner:
                new[i] = owner[(n, f, i)] | (o & 0x8000); stats['owner'] += 1
            elif n in fill_archives and (n, f) not in NOFILL:
                new[i] = fill(n, f, o & 0x7FFF, i) | (o & 0x8000); stats['filled' if new[i] != o else 'kept'] += 1
        # every palette file outside the art archives (the colour-id mode uses the list too), and art files the owner
        # changed
        if (n not in ART and n < 320) or new != c:
            entries.append(((n << 16) | f, ANY, STARTS[(n, f)], new))
            loaded[(n, f, None)] = new
            # each screen gets exactly the owner's colours: its own version where they differ from the above
            for ctx in contexts.get((n, f), ()):
                var = list(new)
                for i, t in by_ctx[(n, f, ctx)].items():
                    if i < len(var):
                        var[i] = t | (c[i] & 0x8000)
                if var != new:
                    entries.append(((n << 16) | f, ctx, STARTS[(n, f)], var)); stats['screen versions'] += 1
                loaded[(n, f, ctx)] = var
    entries.sort()
    mask = [0] * 10
    for key, _, _, _ in entries:
        mask[key >> 21] |= 1 << (key >> 16 & 31)
    data = bytearray()
    offs = []
    base = 48 + 16 * len(entries)
    for key, ctx, start, new in entries:
        offs.append(base + len(data))
        data += struct.pack('<%dH' % len(new), *new)
    out = bytearray(b'RHA4' + struct.pack('<I', len(entries)) + struct.pack('<10I', *mask))
    for (key, ctx, start, new), off in zip(entries, offs):
        assert len(new) < 0x10000 and start < 0x10000
        out += struct.pack('<IIII', key, ctx, off, len(new) | start << 16)
    out += data
    open(OUT, 'wb').write(out)
    pages(loaded)
    print('%s: %d palettes, %d bytes; entries: %s' % (OUT, len(entries), len(out), dict(stats)))


def pages(loaded):
    """data/rh/rh_amoled_pages.bin (layout: src/rh_theme.c ShowScreen): screens that show a palette another screen
    loaded, coloured differently by the owner: per screen fingerprint, which rows (16 colours) become which version."""
    if not os.path.exists(SCREENS):
        open(PAGES_OUT, 'wb').write(b'RHPG' + struct.pack('<II', 0, 0))
        return
    screens = json.load(open(SCREENS))
    # (narc, file, context) -> {screen: {index: colour}}
    shown = collections.defaultdict(dict)
    for name, sc in screens.items():
        for k, t in sc['cols'].items():
            e, ctx = k.split('@')
            n, f, i = map(int, e.split(','))
            shown[(n, f, int(ctx))].setdefault(name, {})[i] = t
    rows = {}                      # 16 colours -> row number
    def row_id(r):
        return rows.setdefault(tuple(r), len(rows))
    per_sig = collections.defaultdict(dict)    # fingerprint -> {(narc, file, context, row): target}
    skipped = 0
    for (n, f, ctx), by in shown.items():
        base = loaded.get((n, f, ctx)) or loaded.get((n, f, None))
        if base is None or len(by) < 2:
            continue
        targets = {}
        for name, cols in by.items():
            t = list(base)
            for i, c in cols.items():
                if i < len(t):
                    t[i] = c | (t[i] & 0x8000)
            targets[name] = t
        for r in range(len(base) // 16):
            vers = {tuple(base[16 * r:16 * r + 16])} | {tuple(t[16 * r:16 * r + 16]) for t in targets.values()}
            if len(vers) < 2:
                continue
            for name, t in targets.items():
                want = tuple(t[16 * r:16 * r + 16])
                for v in vers:
                    if v == want:
                        continue
                    if len(set(v)) < 2:
                        skipped += 1          # a one-colour row can't be told from empty memory: not searched for
                        continue
                    per_sig[screens[name]['sig']][(row_id(v), row_id(want))] = True
    # several of the owner's pictures can share a fingerprint (the bag's pockets: the same tile maps, each pocket's
    # colours copied in by the game from its own palette rows). What they disagree on is left to the game (the palette
    # files carry each state's colours): row versions wanted differently, index patches with different colours, tile
    # patches not in all of them.
    group = collections.defaultdict(list)
    for name, sc in screens.items():
        group[sc['sig']].append(name)
    for sg, names in group.items():
        if len(names) < 2:
            continue
        wants = collections.defaultdict(set)
        for v, w in per_sig.get(sg, {}):
            wants[v].add(w)
        for v, ws in wants.items():
            if len(ws) > 1:
                for w in ws:
                    per_sig[sg].pop((v, w), None)
    vpatch = collections.defaultdict(list)     # fingerprint -> tile patches (see src/rh_theme.c TilePatches)
    for sg, names in group.items():
        sets = [{(addr - 0x06000000) | half << 23 | frm << 24 | to << 28 for addr, half, frm, to in screens[n].get('vpatch', [])}
                for n in names]
        vpatch[sg] = sorted(set.intersection(*sets)) if sets else []
    ipatch = collections.defaultdict(dict)     # fingerprint -> {palette RAM index: colour} (src/rh_theme.c SetIndex)
    for sg, names in group.items():
        cols = collections.defaultdict(set)
        for n in names:
            for i, c in screens[n].get('ipatch', []):
                cols[i].add(c)
        ipatch[sg] = {i: next(iter(cs)) for i, cs in cols.items() if len(cs) == 1}
        # colour links (tools/rh/amoled/learn.py): the entry takes another entry's colour each frame (bit 31)
        links = [set(map(tuple, screens[n].get('link', []))) for n in names]
        for i, src in set.intersection(*links) if links else ():
            ipatch[sg][i] = 0x80000000 | src
    regs = collections.defaultdict(dict)       # fingerprint -> {register offset from 0x04000000: value}
    for name, sc in screens.items():
        for a, v in sc.get('regs', []):
            regs[sc['sig']][a] = v
    # the same screen in another state (e.g. a Pokemon with 4 moves: another tile map, so another fingerprint; the
    # same tile graphics) gets the palette versions, tile and register patches of the owner's picture. Not its index
    # patches: they set palette entries the other state uses for something else (4th move button)
    for name, sc in screens.items():
        for a in sc.get('alias', []):
            if a != sc['sig']:
                per_sig[a] = dict(per_sig.get(sc['sig'], {}))
                vpatch[a] = list(vpatch.get(sc['sig'], []))
                regs[a] = dict(regs.get(sc['sig'], {}))
    sigs = sorted(set(per_sig) | set(vpatch) | set(ipatch) | set(regs))
    data = bytearray()
    offs = []
    base_off = 12 + 8 * len(sigs) + 32 * len(rows)
    for sg in sigs:
        offs.append(base_off + len(data))
        pairs = list(per_sig.get(sg, {}))
        data += struct.pack('<I', len(pairs)) + b''.join(struct.pack('<II', a, b) for a, b in pairs)
        vp = sorted(set(vpatch.get(sg, [])), key=lambda v: (v & 0x7FFFFF, v))[:8192]     # by address (ScreenSig searches
        # it); the theme loads at most 8192 per screen (more: the first 8192, not none)
        data += struct.pack('<I', len(vp)) + b''.join(struct.pack('<I', v) for v in vp)
        ip = sorted(ipatch.get(sg, {}).items())
        data += struct.pack('<I', len(ip)) + b''.join(struct.pack('<I', i << 16 | c) for i, c in ip)
        rg = sorted(regs.get(sg, {}).items())
        data += struct.pack('<I', len(rg)) + b''.join(struct.pack('<I', a << 16 | v) for a, v in rg)
    out = bytearray(b'RHPG' + struct.pack('<II', len(sigs), len(rows)))
    for sg, off in zip(sigs, offs):
        out += struct.pack('<II', sg, off)
    for r, _ in sorted(rows.items(), key=lambda kv: kv[1]):
        out += struct.pack('<16H', *r)
    out += data
    open(PAGES_OUT, 'wb').write(out)
    print('%s: %d screens, %d rows, %d tile patches, %d palette index patches, %d one-colour rows not searched'
          % (PAGES_OUT, len(sigs), len(rows), sum(len(set(v)) for v in vpatch.values()),
             sum(len(v) for v in ipatch.values()), skipped))


HPART_OUT = 'data/rh/rh_hpart.bin'
HP_GAUGES = [188, 191, 194, 197, 200, 203, 253, 256]    # narc 8: the battle HP-box sprites (NCGR, 2 halves of 64x64)


def hp_art():
    """data/rh/rh_hpart.bin (read by src/battle/rh_hptext.c): for each HP-box sprite half, a 64x64 bit mask of its art
    pixels in entries 14 / 2 that are not text. The box's text (name, gender, level, HP numbers) is drawn in those
    entries too; the art's own text (the "/" between the HP numbers) = small groups of 14/2 next to only the text
    backgrounds (15 under the name, 3 around the numbers), not touching the sprite's edge.
    Layout: 'RHHP', count, then per half: u16 file, u16 half, 512 bytes (bit x of byte (y * 64 + x) / 8)."""
    ms = members(ROOT + 'a/0/0/8')
    out = bytearray(b'RHHP' + struct.pack('<I', len(HP_GAUGES) * 2))
    for f in HP_GAUGES:
        b = ms[f]
        if b[:1] == b'\x10':
            b = lz(b)
        for h in (0, 1):
            chr_ = b[0x30 + h * 0x800:0x30 + (h + 1) * 0x800]

            def nib(x, y):
                t, p = (y // 8) * 8 + x // 8, (y % 8) * 8 + x % 8
                v = chr_[t * 32 + p // 2]
                return v >> 4 if p & 1 else v & 15
            px = [[nib(x, y) for x in range(64)] for y in range(64)]
            mask = bytearray(512)
            seen = set()
            for y in range(64):
                for x in range(64):
                    if px[y][x] not in (2, 14) or (x, y) in seen:
                        continue
                    group, todo, text = [], [(x, y)], True
                    seen.add((x, y))
                    while todo:
                        cx, cy = todo.pop()
                        group.append((cx, cy))
                        for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                            if not (0 <= nx < 64 and 0 <= ny < 64):
                                text = False
                            elif px[ny][nx] in (2, 14):
                                if (nx, ny) not in seen:
                                    seen.add((nx, ny))
                                    todo.append((nx, ny))
                            elif px[ny][nx] not in (15, 3):
                                text = False
                    if not text:
                        for gx, gy in group:
                            mask[(gy * 64 + gx) >> 3] |= 1 << (gx & 7)
            out += struct.pack('<HH', f, h) + mask
    open(HPART_OUT, 'wb').write(out)
    print('%s: %d HP-box sprite halves' % (HPART_OUT, len(HP_GAUGES) * 2))


if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
    hp_art()
    sys.exit(main())
