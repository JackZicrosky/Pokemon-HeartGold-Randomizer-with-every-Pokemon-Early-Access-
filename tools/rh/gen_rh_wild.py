#!/usr/bin/env python3
"""Romhack: wild encounter table for "Randomize Wild Pokemon" (port of the FireRed/Emerald wild pair tables).

Reads the built encounter NARC (base/root/a/0/3/7, one EncounterData per encounter bank) and writes
data/rh/RhWildPairs.c (a028 member 21):

    u16 bankStart[NUM_BANKS + 1]       first pair of each bank (pairs are sorted by bank)
    struct RhWildPair pairs[]          every (bank, area, species) with its precomputed zone facts

Zone modes (index m in the arrays), like FireRed/Emerald: 0 whole game, 1 whole game split by encounter type,
2 location, 3 location split, 4 map, 5 map split, 6 encounter set. "Location" = the map code before the room
number (D25R0101 -> D25: all of Union Cave). In HeartGold each encounter bank is one map.

Areas: 0 land (grass, incl. morning/day/night, swarms, radio sounds), 1 surfing, 2 Rock Smash, 3 fishing.

Usage (WSL, in source/, after `make`): .venv/bin/python tools/rh/gen_rh_wild.py
"""
import os
import re
import struct

import ndspy.narc

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')) + '/'


def main():
    enc_names = {}
    for m in re.finditer(r'^#define\s+(ENCDATA_\w+)\s+(\d+)', open(R + 'include/constants/encounter_tables.h').read(), re.M):
        enc_names[int(m.group(2))] = m.group(1)
    personal = ndspy.narc.NARC(open(R + 'base/root/a/0/0/2', 'rb').read()).files
    formtbl = open(R + 'build/PokeFormDataTbl.bin', 'rb').read()
    banks = ndspy.narc.NARC(open(R + 'base/root/a/0/3/7', 'rb').read()).files
    nb = len(banks)

    def to_personal(v):
        sp, form = v & 0x7FF, v >> 11
        if form:
            fs = struct.unpack_from('<H', formtbl, 2 * (32 * sp + form - 1))[0] & 0x7FFF
            return fs if fs else sp
        return sp

    evo = ndspy.narc.NARC(open(R + 'base/root/a/0/3/4', 'rb').read()).files
    pre = {}
    for i, d in enumerate(evo):
        for j in range(9):
            if 6 * j + 6 > len(d):
                break
            meth, param, tgt = struct.unpack_from('<HHH', d, 6 * j)
            if meth and tgt:
                pre.setdefault(to_personal(tgt), i)

    def root(p):
        n = 0
        while p in pre and n < 4:
            p = pre[p]
            n += 1
        return p

    def types(p):
        d = personal[p]
        return (d[6], d[7])

    low = {}       # (bank, area, personal) -> lowest average level
    for b, d in enumerate(banks):
        if len(d) < 196:
            continue
        levels = d[8:20]
        land = struct.unpack_from('<36H', d, 20)
        sounds = struct.unpack_from('<4H', d, 92)
        add = lambda area, v, lv: low.__setitem__((b, area, to_personal(v)), min(low.get((b, area, to_personal(v)), 100), lv)) if v else None
        for i, v in enumerate(land):
            add(0, v, levels[i % 12])
        swarm = struct.unpack_from('<4H', d, 188)
        avg_land = min([l for l in levels if l] or [5])
        for v in sounds + (swarm[0],):
            add(0, v, avg_land)
        for area, off, n in ((1, 100, 5), (2, 120, 2), (3, 128, 15)):
            for i in range(n):
                lo, hi, v = struct.unpack_from('<BBH', d, off + 4 * i)
                add(area, v, (lo + hi) // 2)
        add(1, swarm[1], 20)
        add(3, swarm[2], 20)
        add(3, swarm[3], 20)

    def loc(b):
        n = enc_names.get(b, 'ENCDATA_X%d' % b)[len('ENCDATA_'):]
        m = re.match(r'([A-Z]+\d+)', n)
        return m.group(1) if m else n

    pairs = sorted(low)
    locs = sorted(set(loc(b) for b, a, s in pairs))
    loc_id = {l: i for i, l in enumerate(locs)}

    def zone(mode, b, a):
        return [(), (a,), (loc(b),), (loc(b), a), (b,), (b, a), (b, a)][mode]

    recs = []
    rank_seen = [dict() for _ in range(7)]
    zrank_seen = [dict() for _ in range(7)]
    members = [dict() for _ in range(7)]   # zone -> list of pairs
    for p in pairs:
        for mode in range(7):
            members[mode].setdefault(zone(mode, p[0], p[1]), []).append(p)
    for (b, a, s) in pairs:
        rank, zrank, theme, zlow, rel = [], [], [], [], 0
        for mode in range(7):
            z = zone(mode, b, a)
            k = z + (s,)
            rank.append(rank_seen[mode].setdefault(k, len(rank_seen[mode])))
            d = zrank_seen[mode].setdefault(z, {})
            zrank.append(d.setdefault(s, len(d)))
            # Keep Set/Zone Themes: a type every Pokemon of the zone has (vanilla types), else 0xFF
            mem = members[mode][z]
            t1, t2 = types(mem[0][2])
            for q in mem[1:]:
                tt = types(q[2])
                if t1 not in tt:
                    t1 = 0xFF
                if t2 not in tt:
                    t2 = 0xFF
            theme.append(t1 if t1 != 0xFF else t2)
            zlow.append(min(low[q] for q in mem if q[2] == s))
            if any(q[2] != s and root(q[2]) == root(s) for q in mem):
                rel |= 1 << mode            # "Keep Relations": a relative lives in the same zone
        recs.append((s, b, a, low[(b, a, s)], loc_id[loc(b)], rank, zrank, theme, zlow, rel))

    start = []
    i = 0
    for b in range(nb + 1):
        while i < len(recs) and recs[i][1] < b:
            i += 1
        start.append(i)

    out = ['// AUTO-GENERATED by tools/rh/gen_rh_wild.py from the built encounter data - do not edit by hand.',
           '// See include/rh/rh_data.h (struct RhWildPair) and the generator for the zone modes.',
           '#include "../../include/types.h"', '#include "../../include/rh/rh_data.h"', '',
           f'// {len(recs)} pairs in {nb} encounter banks, {len(locs)} locations',
           f'const struct RhWildFile UNUSED gRhWildFile = {{',
           f'    .bankStart = {{ {", ".join(map(str, start))} }},',
           '    .pairs = {']
    for r in recs:
        out.append(f'        {{ {r[0]}, {r[1]}, {r[2]}, {r[3]}, {r[4]}, {{ {", ".join(map(str, r[5]))} }}, '
                   f'{{ {", ".join(map(str, r[6]))} }}, {{ {", ".join(map(str, r[7]))} }}, {{ {", ".join(map(str, r[8]))} }}, {r[9]} }},')
    out.append('    },')
    out.append('};')
    open(R + 'data/rh/RhWildPairs.c', 'w', newline='\n').write('\n'.join(out) + '\n')
    hdr = open(R + 'include/rh/rh_data.h').read()
    hdr = re.sub(r'#define RH_WILD_BANKS \d+', f'#define RH_WILD_BANKS {nb}', hdr)
    hdr = re.sub(r'#define RH_WILD_PAIRS \d+', f'#define RH_WILD_PAIRS {len(recs)}', hdr)
    open(R + 'include/rh/rh_data.h', 'w', newline='\n').write(hdr)
    print('banks', nb, 'pairs', len(recs), 'locations', len(locs), 'ranks per mode', [len(x) for x in rank_seen])


if __name__ == '__main__':
    main()
