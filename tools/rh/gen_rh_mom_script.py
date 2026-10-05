#!/usr/bin/env python3
# Romhack: Mom gives the Mega Ring right after she explains the Pokegear (owner's choice), and turns Mega Evolution on
# (hg-engine's FLAG_MEGA_EVOLUTION_ENABLED). Talking to her later gives it if it was missed (older saves).
# From pret's player's house script (tools/rh/pret/scr_mom.s = scr_seq_0845_T20R0201) and text bank 545.
# Writes data/scr_seq/scr_seq_00845_T20R0201.s and data/text/545.txt.
# Usage: python3 tools/rh/gen_rh_mom_script.py   (from the repository root; needs tools/msgenc)
import sys

sys.path.insert(0, 'tools/rh')
from gen_rh_trade_scripts import text_bank  # noqa: E402
from import_pret_script import convert  # noqa: E402

FLAG_MEGA_EVOLUTION_ENABLED = 2518
BANK = 545

TEXT_RING = ('Oh, and this package came for you!\\nIt’s from a researcher in a far region.\\r'
             'They say this ring lets Pokémon\\nreach powers beyond their limits.\\r')
TEXT_RING_AFTER = ('With the Mega Ring, a Pokémon holding\\na Mega Stone can Mega Evolve.\\r'
                   'I hear the Dept. Stores in Goldenrod\\nand Celadon sell those stones.\\r')

GIVE = '''	@ romhack: the Mega Ring
	NPCMsg {ring}
	GiveItemNoCheck ITEM_MEGA_RING, 1
	SetFlag {flag}
	NPCMsg {after}
'''


def main():
    src = open('tools/rh/pret/scr_mom.s', encoding='utf-8').read()
    lines = text_bank(BANK)
    ring, after = len(lines), len(lines) + 1
    give = GIVE.format(ring=ring, after=after, flag=FLAG_MEGA_EVOLUTION_ENABLED)
    # after the Pokegear explanation (message 13 doesn't wait for a button by itself)
    old = '\tNPCMsg msg_0545_T20R0201_00013\n\tWaitButtonOrDpad\n'
    assert old in src
    src = src.replace(old, '\tNPCMsg msg_0545_T20R0201_00013\n\tWaitButton\n' + give + '\tWaitButtonOrDpad\n', 1)
    # later visits: the ring if it is missing
    old = '_01D4:\n\tNPCMsg msg_0545_T20R0201_00014\n'
    assert old in src
    src = src.replace(old, '_01D4:\n\tHasItem ITEM_MEGA_RING, 1, VAR_SPECIAL_RESULT\n\tCompare VAR_SPECIAL_RESULT, 1\n'
                      '\tGoToIfEq _rh_has_ring\n' + give + '\tWaitButton\n_rh_has_ring:\n\tNPCMsg msg_0545_T20R0201_00014\n', 1)
    head = '// Romhack: pret/pokeheartgold scr_seq_0845_T20R0201.s, Mom gives the Mega Ring (tools/rh/gen_rh_mom_script.py).\n'
    open('data/scr_seq/scr_seq_00845_T20R0201.s', 'w', newline='\n').write(head + convert(src))
    lines = [line if line != '' else ' ' for line in lines]
    lines += [TEXT_RING, TEXT_RING_AFTER]
    open('data/text/%03d.txt' % BANK, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    print('Mom: messages', ring, after)


if __name__ == '__main__':
    main()
