#!/usr/bin/env python3
# Romhack: converts a pret/pokeheartgold map script (files/fielddata/script/scr_seq/scr_seq_NNNN_*.s) into a script
# hg-engine builds in place of the vanilla one (data/scr_seq/scr_seq_0NNNN_NAME.s), using the pret macros
# (asm/include/rh_pret_script.inc, tools/rh/gen_pret_script_inc.py). Message constants become their numbers.
# The unmodified result assembles to the ROM's own script, byte for byte.
# Usage: python3 tools/rh/import_pret_script.py PRET_FILE.s NUMBER NAME   (from the repository root)
import re
import sys

PREFIX = '''#include "constants/species.h"
#include "rh/pret_compat.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "rh/pret_constants.h"
.include "asm/include/rh_pret_script.inc"
.include "asm/include/flags.inc"
.include "asm/include/vars.inc"
.include "asm/include/soundeffects.inc"
'''


def convert(src):
    s = re.sub(r'msg_\d+(?:_\w+?)?_(\d{5})\b', lambda m: str(int(m.group(1))), src)
    # the map's object ids (pret's generated event header, kept in tools/rh/pret/events)
    ev = re.search(r'#include "fielddata/script/scr_seq/(event_\w+\.h)"', s)
    s = re.sub(r'#include.*\n', '', s)
    if ev:
        s = '#include "tools/rh/pret/events/%s"\n' % ev.group(1) + s
    s = s.replace('.include "asm/macros/script.inc"', '')
    s = s.replace('.rodata', '.data')
    return PREFIX + s


if __name__ == '__main__':
    src, num, name = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    out = 'data/scr_seq/scr_seq_%05d_%s.s' % (num, name)
    head = '// Romhack: pret/pokeheartgold %s, imported by tools/rh/import_pret_script.py.\n' % src.split('/')[-1]
    open(out, 'w', newline='\n').write(head + convert(open(src, encoding='utf-8').read()))
    print('wrote', out)
