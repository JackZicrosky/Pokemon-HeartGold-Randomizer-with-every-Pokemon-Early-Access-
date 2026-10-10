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
SETTINGS_INDEX = FIRST_INDEX       # the Randomizer Settings key item's script (src/item.c RH_SCRIPT_RANDOMIZER_SETTINGS)
SHOPS_INDEX = FIRST_INDEX + 1
HEALING_KIT_INDEX = SHOPS_INDEX + 7  # the Healing Kit key item's script (src/item.c RH_SCRIPT_HEALING_KIT)
HM_KIT_INDEX = HEALING_KIT_INDEX + 1  # the HM Kit's explanation (src/item.c RH_SCRIPT_HM_KIT_INFO)
HM_KIT_TEXT = (r'The HM Kit works by itself: just walk\nup to a tree, rock, boulder, whirlpool,\r'
               r'waterfall, rock wall or water.\rFrom the Bag it can Fly you once you\nhave HM02 and the Storm Badge.\r')
RH_SPECIAL_OPEN_SETTINGS = 3       # include/rh/rh_items.h
SETTINGS_TEXTS = [
    r'WARNING!\nChanging the settings mid-run\rcan BREAK your current playthrough.\n'
    r'Pokémon, Trainers and items may\fall change.\rBe careful when using this!\r',
    r'Open the Randomizer Settings? {YESNO 0}',
]
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
    return 7000 + SHOPS_INDEX + ORDER.index(name)


def scripts(first_msg):
    """scrdef lines and bodies for scr_seq 141 (hg-engine syntax); first_msg = this module's first text number"""
    assert 7000 + SETTINGS_INDEX == 7256
    defs = ['scrdef RhRandomizerSettings'] + ['scrdef RhShop_%s' % n for n in ORDER]
    warn, ask = first_msg + len(ORDER), first_msg + len(ORDER) + 1
    body = ['RhRandomizerSettings:',
            '    lockall',
            '    npc_msg %d' % warn,
            '    npc_msg %d' % ask,
            '    touchscreen_menu_hide',
            '    getmenuchoice VAR_SPECIAL_RESULT',
            '    touchscreen_menu_show',
            '    compare VAR_SPECIAL_RESULT, 0',
            '    goto_if_ne RhRandomizerSettingsEnd',
            '    closemsg',
            '    fade_screen 6, 1, 0, 0',
            '    wait_fade',
            '    setvar VAR_SPECIAL_RESULT, %d' % RH_SPECIAL_OPEN_SETTINGS,
            '    nop_var_490 VAR_SPECIAL_RESULT',
            '    restore_overworld',
            '    fade_screen 6, 1, 1, 0',
            '    wait_fade',
            '    releaseall',
            '    end',
            '',
            'RhRandomizerSettingsEnd:',
            '    closemsg',
            '    releaseall',
            '    end', '']
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
    # the Healing Kit: heals the whole party (scr_seq 3's Pokemon Center healing: heal_party)
    assert len(defs) == HEALING_KIT_INDEX - FIRST_INDEX and 7000 + HEALING_KIT_INDEX == 7264
    defs.append('scrdef RhHealingKit')
    body += ['RhHealingKit:',
             '    lockall',
             '    heal_party',                          # owner: no message, just the potion sound
             '    play_se SEQ_SE_DP_KAIFUKU',
             '    wait_se SEQ_SE_DP_KAIFUKU',
             '    releaseall',
             '    end', '']
    defs.append('scrdef RhHMKitInfo')
    body += ['RhHMKitInfo:',
             '    lockall',
             '    npc_msg %d' % (ask + 1),
             '    closemsg',
             '    releaseall',
             '    end', '']
    return defs, body


def main():
    from gen_rh_trade_scripts import text_bank
    lines = [line if line != '' else ' ' for line in text_bank(BANK)]
    lines += [SELLERS[n][2] for n in ORDER] + SETTINGS_TEXTS + [HM_KIT_TEXT]
    open('data/text/%03d.txt' % BANK, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    print('shop NPC greetings: messages', len(lines) - len(ORDER), '-', len(lines) - 1)


if __name__ == '__main__':
    main()
