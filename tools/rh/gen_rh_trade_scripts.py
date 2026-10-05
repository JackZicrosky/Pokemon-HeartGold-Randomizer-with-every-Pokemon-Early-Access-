#!/usr/bin/env python3
# Romhack: the in-game trade NPCs name the Pokemon they really trade (In-Game Trades). For each trade script
# (pret, tools/rh/pret/trades/), the trade entry first loads its trade and buffers the requested species (slot 6)
# and the offered one (slot 7); the text bank names them through those buffers.
# Writes data/scr_seq/scr_seq_0NNNN_*.s and data/text/BANK.txt.
# Usage: python3 tools/rh/gen_rh_trade_scripts.py   (from the repository root; needs tools/msgenc)
import os
import re
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, 'tools/rh')
sys.path.insert(0, 'tools/rh/test')
from import_pret_script import convert  # noqa: E402
from narc import read_narc  # noqa: E402

# pret script, script number, text bank, trade number
TRADES = [
    ('scr_seq_0005_D01R0101', 5, 47, 11),
    ('scr_seq_0195_R10R0201', 195, 344, 8),
    ('scr_seq_0196_R10R0202', 196, 345, 8),
    ('scr_seq_0753_T03PC0101', 753, 463, 9),
    ('scr_seq_0837_T11R0701', 837, 537, 12),
    ('scr_seq_0864_T22R0601', 864, 562, 0),
    ('scr_seq_0903_T25R1005', 903, 596, 1),
    ('scr_seq_0916_T26R0301', 916, 608, 2),
    ('scr_seq_0947_T30R0301', 947, 634, 3),
]
SLOT_ASK, SLOT_GIVE = 6, 7

PRELOAD = '''	@ romhack: the trade's (randomized) Pokemon, for the text
	LoadNPCTrade {n}
	NPCTradeGetReqSpecies VAR_SPECIAL_x8008
	GetOfferedSpecies VAR_SPECIAL_x8009
	NPCTradeEnd
	BufferSpeciesName {ask}, VAR_SPECIAL_x8008, 0, 0
	BufferSpeciesName {give}, VAR_SPECIAL_x8009, 0, 0
'''


def clean_text_archive():
    # the ROM's own text (rom.nds): base/root is rewritten by every build
    if not os.path.exists('build/a027_clean'):
        subprocess.run(['python3', 'tools/rh/nds_extract.py', 'rom.nds', 'a/0/2/7', 'build/a027_clean'], check=True)
    return read_narc('build/a027_clean')


def text_bank(bank):
    data = clean_text_archive()[bank]
    with tempfile.TemporaryDirectory() as d:
        src, out = os.path.join(d, '7_%03d' % bank), os.path.join(d, 'out.txt')
        open(src, 'wb').write(data)
        subprocess.run(['tools/msgenc', '-d', '-c', 'charmap.txt', src, out], check=True, stdout=subprocess.DEVNULL)
        return open(out, encoding='utf-8').read().rstrip('\n').split('\n')   # one entry per line


def main():
    names = open('build/rawtext/237.txt', encoding='utf-8').read().split('\n')
    trades = read_narc('base/root/a/1/1/2')
    for pret, num, bank, n in TRADES:
        t = struct.unpack('<21i', trades[n][:84])
        give, ask = names[t[0]].upper(), names[t[19]].upper()
        src = open('tools/rh/pret/trades/%s.s' % pret, encoding='utf-8').read()
        # the entry that loads this trade: buffer the names right after it faces the player
        entries = re.split(r'(?m)^(?=scr_seq_\w+:)', src)
        for i, e in enumerate(entries):
            if 'LoadNPCTrade %d' % n in e:
                anchor = 'FacePlayer\n' if 'FacePlayer\n' in e else 'LockAll\n'
                entries[i] = e.replace(anchor, anchor + PRELOAD.format(n=n, ask=SLOT_ASK, give=SLOT_GIVE), 1)
        src = ''.join(entries)
        if n == 3:
            # Dragonair (female) only while the trade still asks for Dragonair
            src = src.replace('\tMonGetGender VAR_SPECIAL_x8004, VAR_SPECIAL_x8006\n',
                              '\tCompare VAR_SPECIAL_x8008, SPECIES_DRAGONAIR\n\tGoToIfNe _rh_skip_gender\n'
                              '\tMonGetGender VAR_SPECIAL_x8004, VAR_SPECIAL_x8006\n', 1)
            src = src.replace('\tGoToIfEq _00B2\n\tNPCTradeGetReqSpecies', '\tGoToIfEq _00B2\n_rh_skip_gender:\n\tNPCTradeGetReqSpecies', 1)
        name = pret.split('_', 3)[3]
        head = '// Romhack: pret/pokeheartgold %s.s with the trade\'s Pokemon buffered for the text (tools/rh/gen_rh_trade_scripts.py).\n' % pret
        open('data/scr_seq/scr_seq_%05d_%s.s' % (num, name), 'w', newline='\n').write(head + convert(src))
        lines = text_bank(bank)
        for j, line in enumerate(lines):
            line = line.replace(ask + '㊛', ask)
            line = re.sub(r'\b%s\b' % re.escape(ask), '{STRVAR_1 0, %d, 0}' % SLOT_ASK, line)
            line = re.sub(r'\b%s\b' % re.escape(give), '{STRVAR_1 0, %d, 0}' % SLOT_GIVE, line)
            lines[j] = line if line != '' else ' '   # msgenc drops empty entries
        open('data/text/%03d.txt' % bank, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
        print(pret, 'trade', n, ask, '->', give)


if __name__ == '__main__':
    main()
