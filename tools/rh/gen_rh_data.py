#!/usr/bin/env python3
"""Romhack: builds the randomizer's species table from hg-engine's BUILT data (run after `make`).

Reads base/root/a/0/0/2 (personal), a/0/3/4 (evolutions), build/PokeFormDataTbl.bin and the species
constants, and writes data/rh/RhSpeciesInfo.c (committed; compiled into a028 member 20 by the Makefile).

One 12-byte record per personal-data index (base species and form species alike):
    u16 key      base species | form << 11
    u16 bst      vanilla base stat total
    u16 preEvo   personal index of the direct pre-evolution (0 = basic)
    u8  type1, type2
    u8  gsc      gen (bits 0-3) | stage-1 (bits 4-5) | chain-1 (bits 6-7)
    u8  flags    RH_SPF_* (include/rh/rh_data.h)
    u8  children distinct vanilla evolutions (forms count once): all (bits 0-3), gen 1 only (4-5), gens 1-3 (6-7)
    u8  flags2   RH_SPF2_*

Usage (WSL, in source/):  .venv/bin/python tools/rh/gen_rh_data.py
"""
import os
import re
import struct
import sys

import ndspy.narc

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')) + '/'

# national dex numbers of legendaries, mythicals, Ultra Beasts and Paradox Pokemon (same list as the
# FireRed/Emerald versions: isRestrictedLegendary | isSubLegendary | isMythical | isUltraBeast | isParadox)
LEGENDARY_DEX = {
    144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251, 377, 378, 379, 380, 381, 382, 383, 384, 385, 386,
    480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 638, 639, 640, 641, 642, 643,
    644, 645, 646, 647, 648, 649, 716, 717, 718, 719, 720, 721, 772, 773, 785, 786, 787, 788, 789, 790, 791,
    792, 793, 794, 795, 796, 797, 798, 799, 800, 801, 802, 803, 804, 805, 806, 807, 808, 809, 888, 889, 890,
    891, 892, 893, 894, 895, 896, 897, 898, 905, 984, 985, 986, 987, 988, 989, 990, 991, 992, 993, 994, 995,
    1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009, 1010, 1014, 1015, 1016, 1017, 1020, 1021, 1022,
    1023, 1024, 1025,
}
# "strong" legendaries (FVX: restricted legendaries) for "Standardize EXP Curves: Strong Legendaries"
STRONG_LEGENDARY_DEX = {150, 249, 250, 382, 383, 384, 483, 484, 487, 643, 644, 646, 716, 717, 718, 789, 790,
                        791, 792, 800, 888, 889, 890, 898, 1007, 1008, 1024}

GEN_LAST_DEX = [151, 251, 386, 493, 649, 721, 809, 905, 1025]

RH_SPF_LEGENDARY = 0x01
RH_SPF_VARIANT = 0x02      # regional / alternate / cosmetic form (only in the "All + Forms" pool, or "Allow Alternate Formes")
RH_SPF_MEGA = 0x04         # Mega Evolution (only for "Permanent Mega" wild Pokemon)
RH_SPF_POOL = 0x08         # can be picked at all (base species, usable forms, Megas)
RH_SPF_STRONG_LEGEND = 0x10
RH_SPF_BASE = 0x20         # the base form of its species

RH_SPF2_REGIONAL = 0x01    # regional form: a Pokemon of its own (own types, abilities...)
RH_SPF2_MEGA_LIKE = 0x02   # Mega / Primal / Ultra Burst: follows its base form only with "Follow Mega Evolutions"
RH_SPF2_VIVILLON = 0x04    # a Vivillon wing pattern: never picked itself (Vivillon counts once)


# Forms that can be picked as "alternate formes" (regional forms and forms a wild Pokemon can have). Every other
# form is excluded: battle-only forms, event/cosplay forms, totems, Gigantamax, forms that need a held item, and
# gender-difference forms (their form must match the Pokemon's gender).
VARIANT_SUFFIXES = ('_ALOLAN', '_GALARIAN', '_HISUIAN', '_PALDEAN')
VARIANT_PREFIXES = ('UNOWN_', 'BURMY_', 'WORMADAM_', 'SHELLOS_', 'GASTRODON_', 'BASCULIN_', 'DEERLING_', 'SAWSBUCK_',
                    'VIVILLON_', 'SCATTERBUG_', 'SPEWPA_', 'FLABEBE_', 'FLOETTE_', 'FLORGES_', 'PUMPKABOO_', 'GOURGEIST_',
                    'ORICORIO_', 'LYCANROC_', 'MINIOR_METEOR_', 'TOXTRICITY_', 'SINISTEA_', 'POLTEAGEIST_', 'SQUAWKABILLY_',
                    'TATSUGIRI_', 'DUDUNSPARCE_', 'GIMMIGHOUL_', 'TAUROS_', 'ROTOM_', 'DEOXYS_', 'POLTCHAGEIST_',
                    'SINISTCHA_', 'URSALUNA_', 'MAUSHOLD_')
VARIANT_NAMES = {'GIRATINA_ORIGIN', 'SHAYMIN_SKY', 'DIALGA_ORIGIN', 'PALKIA_ORIGIN', 'TORNADUS_THERIAN',
                 'THUNDURUS_THERIAN', 'LANDORUS_THERIAN', 'ENAMORUS_THERIAN', 'KYUREM_WHITE', 'KYUREM_BLACK',
                 'HOOPA_UNBOUND', 'NECROZMA_DUSK_MANE', 'NECROZMA_DAWN_WINGS', 'CALYREX_ICE_RIDER',
                 'CALYREX_SHADOW_RIDER', 'URSHIFU_RAPID_STRIKE', 'ZARUDE_DADA', 'MAGEARNA_ORIGINAL', 'ZYGARDE_10'}
VARIANT_EXCLUDE = {'FLOETTE_ETERNAL_FLOWER', 'PIKACHU_ALOLA_CAP'}


def is_variant(name):
    n = name[len('SPECIES_'):]
    if n in VARIANT_EXCLUDE or n.endswith('_LARGE') and not n.startswith(('PUMPKABOO_', 'GOURGEIST_')):
        return False
    return n in VARIANT_NAMES or n.endswith(VARIANT_SUFFIXES) or n.startswith(VARIANT_PREFIXES)


def species_constants():
    txt = open(R + 'include/constants/species.h').read()
    names = {}
    for m in re.finditer(r'^#define\s+(SPECIES_\w+|MAX_\w+|NUM_OF_\w+)\s+(.+?)\s*(//.*)?$', txt, re.M):
        names[m.group(1)] = m.group(2)
    vals = {}

    def ev(n, depth=0):
        if n in vals:
            return vals[n]
        expr = names[n]
        expr = re.sub(r'\b([A-Z_][A-Z0-9_]*)\b', lambda mm: str(ev(mm.group(1), depth + 1)) if mm.group(1) in names else mm.group(1), expr)
        v = eval(expr)
        vals[n] = v
        return v

    out = {}
    for n in names:
        try:
            out[n] = ev(n)
        except Exception:
            pass
    return out


def dex_of(species):
    if species <= 493:
        return species
    if species >= 544:
        return species - 50
    return 0


def gen_of(dex):
    for g, last in enumerate(GEN_LAST_DEX, 1):
        if dex <= last:
            return g
    return 9


def main():
    C = species_constants()
    by_id = {}
    for n, v in sorted(C.items(), key=lambda kv: (kv[1], kv[0].endswith('_START'))):
        if n.startswith('SPECIES_') and v not in by_id:
            by_id[v] = n
    max_mon = C['SPECIES_MAX_MON_NUM']
    max_all = C['MAX_SPECIES_INCLUDING_FORMS']
    mega_start, mega_end = C['SPECIES_MEGA_START'], C['MAX_MEGA_NUM']

    personal = ndspy.narc.NARC(open(R + 'base/root/a/0/0/2', 'rb').read()).files
    evo = ndspy.narc.NARC(open(R + 'base/root/a/0/3/4', 'rb').read()).files
    formtbl = open(R + 'build/PokeFormDataTbl.bin', 'rb').read()
    n_entries = max_all + 1
    print('personal members', len(personal), 'evo members', len(evo), 'species incl forms', n_entries)

    # forms: personal index -> (base species, form number, battle only)
    key = {}
    battle_only = set()
    for sp in range(1, max_mon + 1):
        key[sp] = (sp, 0)
        for f in range(32):
            off = 2 * (32 * sp + f)
            if off + 2 > len(formtbl):
                break
            v = struct.unpack_from('<H', formtbl, off)[0]
            if v == 0:
                continue
            fs = v & 0x7FFF
            if fs not in key or key[fs][0] == fs:
                key[fs] = (sp, f + 1)
            if v & 0x8000:
                battle_only.add(fs)

    def stats(i):
        d = personal[i] if i < len(personal) else b''
        if len(d) < 0x1C:
            return None
        return d

    # evolutions (by personal index); targets are personal indexes
    children = {}
    pre = {}
    for i in range(min(len(evo), n_entries)):
        d = evo[i]
        for j in range(9):
            if 6 * j + 6 > len(d):
                break
            meth, param, tgt = struct.unpack_from('<HHH', d, 6 * j)
            if tgt >> 11:   # MON_WITH_FORM(species, form) -> the form's personal index
                fs = struct.unpack_from('<H', formtbl, 2 * (32 * (tgt & 0x7FF) + (tgt >> 11) - 1))[0] & 0x7FFF
                tgt = fs if fs else tgt & 0x7FF
            if meth == 0 or tgt == 0 or tgt == i or tgt >= n_entries:
                continue
            children.setdefault(i, []).append(tgt)
            pre.setdefault(tgt, i)

    def stage(i):
        s, seen = 1, 0
        while i in pre and seen < 4:
            i = pre[i]; s += 1; seen += 1
        return min(s, 3)

    def chain_len(i, depth=0):
        ch = children.get(i, [])
        if not ch or depth > 4:
            return 1
        return 1 + max(chain_len(c, depth + 1) for c in ch)

    def base_of(i):
        return key.get(i, (i, 0))[0]

    def children_counts(i):
        seen_all, seen_g1, seen_g3 = set(), set(), set()
        for c in children.get(i, []):
            b = base_of(c)
            g = gen_of(dex_of(b)) if b else 9
            seen_all.add(b)
            if g <= 1:
                seen_g1.add(b)
            if g <= 3:
                seen_g3.add(b)
        return min(len(seen_all), 15) | (min(len(seen_g1), 3) << 4) | (min(len(seen_g3), 3) << 6)

    recs = []
    pool = 0
    for i in range(n_entries):
        base, form = key.get(i, (i, 0) if i <= max_mon else (0, 0))
        d = stats(i)
        name = by_id.get(i, '')
        flags = 0
        bst = t1 = t2 = 0
        if d is not None and base:
            bst = sum(d[0:6])
            t1, t2 = d[6], d[7]
        dex = dex_of(base) if base else 0
        g = gen_of(dex) if dex else 0
        line = base if name.startswith('SPECIES_MEGA_') else i   # a Mega has its base form's stage
        st = stage(line)
        ch = min(3, st + chain_len(line) - 1)
        usable = d is not None and bst > 0 and dex != 0
        if usable:
            if dex in LEGENDARY_DEX:
                flags |= RH_SPF_LEGENDARY
            if dex in STRONG_LEGENDARY_DEX:
                flags |= RH_SPF_STRONG_LEGEND
            if form == 0:
                flags |= RH_SPF_BASE | RH_SPF_POOL
            elif name.startswith('SPECIES_MEGA_'):
                flags |= RH_SPF_MEGA | RH_SPF_POOL
            elif is_variant(name):
                flags |= RH_SPF_VARIANT | RH_SPF_POOL
        flags2 = 0
        if name.endswith(('_ALOLAN', '_GALARIAN', '_HISUIAN', '_PALDEAN')) or name in (
                'SPECIES_TAUROS_COMBAT', 'SPECIES_TAUROS_BLAZE', 'SPECIES_TAUROS_AQUA', 'SPECIES_URSALUNA_BLOODMOON'):
            flags2 |= RH_SPF2_REGIONAL
        if name.startswith('SPECIES_MEGA_') or 'PRIMAL' in name or '_ULTRA_' in name:
            flags2 |= RH_SPF2_MEGA_LIKE
        if name.startswith('SPECIES_VIVILLON_'):
            flags2 |= RH_SPF2_VIVILLON
        recs.append((base | (form << 11), bst, pre.get(i, 0), t1, t2, (g & 15) | ((st - 1) << 4) | ((ch - 1) << 6), flags,
                     children_counts(i), flags2))
        if flags & RH_SPF_POOL:
            pool += 1

    os.makedirs(R + 'data/rh', exist_ok=True)
    out = ['// AUTO-GENERATED by tools/rh/gen_rh_data.py from the built hg-engine data - do not edit by hand.',
           '// Randomizer species table, one record per personal-data index. See include/rh/rh_data.h.',
           '#include "../../include/types.h"', '#include "../../include/rh/rh_data.h"', '',
           f'const struct RhSpeciesInfo UNUSED gRhSpeciesInfo[{n_entries}] = {{']
    for i, r in enumerate(recs):
        out.append(f'    /* {i:4} {by_id.get(i, "")[8:]:<24} */ {{ 0x{r[0]:04X}, {r[1]:3}, {r[2]:4}, {r[3]:2}, {r[4]:2}, 0x{r[5]:02X}, 0x{r[6]:02X}, 0x{r[7]:02X}, 0x{r[8]:02X} }},')
    out.append('};')
    open(R + 'data/rh/RhSpeciesInfo.c', 'w', newline='\n').write('\n'.join(out) + '\n')
    print('records', n_entries, 'pool candidates', pool,
          'legendary', sum(1 for r in recs if r[6] & RH_SPF_LEGENDARY and r[6] & RH_SPF_BASE),
          'variants', sum(1 for r in recs if r[6] & RH_SPF_VARIANT), 'megas', sum(1 for r in recs if r[6] & RH_SPF_MEGA))


if __name__ == '__main__':
    main()
