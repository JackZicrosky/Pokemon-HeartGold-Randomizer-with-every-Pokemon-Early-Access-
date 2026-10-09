#!/usr/bin/env python3
# Romhack: the special shops' NPCs (port of the FireRed/Emerald shops): an evolution item seller in every Poke Mart
# and in both Dept. Stores, and the Dept. Store counters (Goldenrod and Celadon, owner's choice): battle gear,
# form items, Mega Stones, training items, Berries and snacks. Kurt sells the Poke Balls (gen_rh_kurt_script.py).
# - Their scripts are extra entries of the item ball script file (scr_seq 141, script ids 7000 + index), written by
#   tools/rh/gen_rh_items.py with scripts() below; their text is appended to its text bank (199).
# - The NPCs are added to the maps' event data at build time (tools/rh/zone_events.py --patch, from the clean ROM).
# Usage: python3 tools/rh/gen_rh_shop_npcs.py   (from the repository root: writes data/text/199.txt)
import sys

sys.path.insert(0, 'tools/rh')

BANK = 199
FIRST_INDEX = 256                  # entries 0-254 = item balls, 255 = ItemBallCommon (gen_rh_items.py)
RH_SPECIAL_MART_BASE = 30          # include/rh/rh_data.h

# name: (RH_SPECIAL_* list, sprite, greeting)
SELLERS = {
    'Evo': (0, 338, r'I deal in items that make Pokémon\nevolve. Stones, cords, sweets...\rWhatever your Pokémon needs!\r'),
    'Competitive': (2, 332, r'Welcome to the Battle Gear counter.\nOnly the finest held items here.\r'),
    'Forms': (3, 326, r'This counter carries rare artifacts\nthat change a Pokémon’s form.\r'),
    'Mega': (4, 341, r'Mega Stones from every region!\nYou’ll need a Mega Ring to use them.\r'),
    'Training': (5, 344, r'Serious Trainers shop here.\nWe help Pokémon reach full potential.\r'),
    'Berries': (6, 331, r'Berries from all over the world!\nFresh picked, every one.\r'),
    'Snacks': (7, 320, r'Famous treats from every region!\nPokémon love them.\r'),
}
ORDER = list(SELLERS)

# Poke Marts: the free tile at (12, 4) (the same in every Mart), facing down
MARTS = [68, 79, 157, 165, 227, 292, 360, 409, 426, 433, 474, 478, 500, 530]
# (map, seller, x, z): facing down, with a free tile below
PLACEMENTS = [(m, 'Evo', 12, 4) for m in MARTS] + [
    (192, 'Competitive', 2, 3),                          # Goldenrod Dept. Store 2F .. 6F
    (192, 'Evo', 14, 10), (192, 'Mega', 16, 10),         # 2F's open floor (owner: a free, accessible space)
    (193, 'Forms', 10, 3), (195, 'Training', 10, 3),
    (196, 'Berries', 3, 3), (196, 'Snacks', 6, 3),
    (371, 'Evo', 8, 3), (371, 'Competitive', 3, 3),      # Celadon Dept. Store 2F .. roof
    (372, 'Forms', 3, 3), (373, 'Mega', 3, 3), (374, 'Training', 3, 3),
    (375, 'Berries', 3, 3), (375, 'Snacks', 6, 3),
]
FACING_DOWN = 1


def script_id(name):
    return 7000 + FIRST_INDEX + ORDER.index(name)


def scripts(first_msg):
    """scrdef lines and bodies for scr_seq 141 (hg-engine syntax); first_msg = text number of the first greeting"""
    defs = ['scrdef RhShop_%s' % n for n in ORDER]
    body = []
    for i, n in enumerate(ORDER):
        body += ['RhShop_%s:' % n,
                 '    play_se SEQ_SE_DP_SELECT',
                 '    lockall',
                 '    faceplayer',
                 '    npc_msg %d' % (first_msg + i),
                 '    closemsg',
                 '    special_mart_buy %d' % (RH_SPECIAL_MART_BASE + SELLERS[n][0]),
                 '    releaseall',
                 '    end', '']
    return defs, body


def main():
    from gen_rh_trade_scripts import text_bank
    lines = [line if line != '' else ' ' for line in text_bank(BANK)]
    lines += [SELLERS[n][2] for n in ORDER]
    open('data/text/%03d.txt' % BANK, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    print('shop NPC greetings: messages', len(lines) - len(ORDER), '-', len(lines) - 1)


if __name__ == '__main__':
    main()
