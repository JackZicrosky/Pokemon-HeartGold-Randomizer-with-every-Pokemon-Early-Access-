#!/usr/bin/env python3
# Romhack: item tables for the Items randomizers (field items, shops, pickup), from hg-engine's data.
# Writes:
#   data/rh/RhItems.c       a028 member 24: the random item pool with FVX's flags (bad, overpowered, evolution, X...)
#   data/rh/RhFieldItems.c  a028 member 25: every item ball / hidden item (for "Shuffle"); key = item ball number,
#                           or 0x1000 + hidden item index
#   data/scr_seq/scr_seq_00141_itemballs.s  the item ball scripts (pret's scr_seq_0141, tools/rh/pret) with the
#                           randomizer call (nop_var_490 = the romhack's script command, RH_SPECIAL_ITEM_BALL)
# Usage: python3 tools/rh/gen_rh_items.py   (from the repository root)
import re

POOL_POCKETS = {'POCKET_ITEMS', 'POCKET_MEDICINE', 'POCKET_BALLS', 'POCKET_BERRIES', 'POCKET_BATTLE_ITEMS', 'POCKET_MAIL'}
# never random items: event / unobtainable Balls, Z-Crystals, quest items, relics and shards (FVX bad items)
BANNED = {'MASTER_BALL', 'CHERISH_BALL', 'PARK_BALL', 'SAFARI_BALL', 'SPORT_BALL', 'DREAM_BALL', 'BEAST_BALL',
          'STRANGE_BALL', 'SOUL_DEW', 'RUSTED_SWORD', 'RUSTED_SHIELD', 'ODD_KEYSTONE', 'RELIC_COPPER', 'RELIC_SILVER',
          'RELIC_VASE', 'RELIC_BAND', 'RELIC_STATUE', 'RELIC_CROWN', 'RELIC_GOLD', 'RED_SHARD', 'BLUE_SHARD',
          'YELLOW_SHARD', 'GREEN_SHARD', 'HEART_SCALE', 'SHOAL_SALT', 'SHOAL_SHELL', 'GRISEOUS_ORB', 'ADAMANT_ORB',
          'LUSTROUS_ORB', 'ENIGMA_BERRY', 'DYNAMAX_CANDY', 'MAX_MUSHROOMS', 'GALARICA_TWIG', 'WISHING_PIECE'}
# FVX junk ("Ban Bad Items")
BAD = {'TINY_MUSHROOM', 'PRETTY_FEATHER', 'PRETTY_WING', 'HONEY', 'BLACK_FLUTE', 'WHITE_FLUTE', 'BLUE_FLUTE',
       'YELLOW_FLUTE', 'RED_FLUTE', 'FRESH_WATER', 'SODA_POP', 'LEMONADE', 'MOOMOO_MILK', 'BERRY_JUICE', 'LAGGING_TAIL',
       'IRON_BALL', 'RING_TARGET', 'STICKY_BARB', 'FULL_INCENSE', 'CLEANSE_TAG', 'SMOKE_BALL', 'SOOTHE_BELL',
       'POKE_DOLL', 'FLUFFY_TAIL', 'POKE_TOY', 'BLACK_SLUDGE', 'FLOAT_STONE', 'DESTINY_KNOT'}
OVERPOWERED = {'LUCKY_EGG', 'RARE_CANDY', 'MASTER_BALL', 'EXP_CANDY_XL', 'ABILITY_PATCH', 'BIG_NUGGET', 'COMET_SHARD',
               'PEARL_STRING', 'BALM_MUSHROOM', 'SACRED_ASH', 'MAX_HONEY', 'AMULET_COIN', 'GOLD_BOTTLE_CAP'}
REGULAR_SHOP = {'POKE_BALL', 'GREAT_BALL', 'ULTRA_BALL', 'POTION', 'SUPER_POTION', 'HYPER_POTION', 'MAX_POTION',
                'FULL_RESTORE', 'REVIVE', 'ANTIDOTE', 'PARALYZE_HEAL', 'AWAKENING', 'BURN_HEAL', 'ICE_HEAL', 'FULL_HEAL',
                'REPEL', 'SUPER_REPEL', 'MAX_REPEL', 'ESCAPE_ROPE'}
X_ITEMS = {'X_ATTACK', 'X_DEFENSE', 'X_SP_ATK', 'X_SP_DEF', 'X_SPEED', 'X_ACCURACY', 'DIRE_HIT', 'GUARD_SPEC'}
SELL_ONLY = {'NUGGET', 'BIG_NUGGET', 'PEARL', 'BIG_PEARL', 'PEARL_STRING', 'STARDUST', 'STAR_PIECE', 'COMET_SHARD',
             'BALM_MUSHROOM', 'BIG_MUSHROOM', 'RARE_BONE', 'TINY_BAMBOO_SHOOT', 'BIG_BAMBOO_SHOOT', 'SLOWPOKE_TAIL'}
CONSUMABLE_HELD = {'WHITE_HERB', 'MENTAL_HERB', 'POWER_HERB', 'FOCUS_SASH', 'AIR_BALLOON', 'WEAKNESS_POLICY',
                   'RED_CARD', 'EJECT_BUTTON', 'ABSORB_BULB', 'CELL_BATTERY', 'LUMINOUS_MOSS', 'SNOWBALL',
                   'ROOM_SERVICE', 'EJECT_PACK', 'BLUNDER_POLICY', 'THROAT_SPRAY', 'ADRENALINE_ORB', 'MIRROR_HERB'}

F_BAD, F_OVERPOWERED, F_EVO, F_X, F_REGULAR, F_SELL_ONLY, F_CONSUMABLE, F_HELD = 1, 2, 4, 8, 16, 32, 64, 128
POCKETS = ['POCKET_ITEMS', 'POCKET_MEDICINE', 'POCKET_BALLS', 'POCKET_TMHMS', 'POCKET_BERRIES', 'POCKET_MAIL',
           'POCKET_BATTLE_ITEMS', 'POCKET_KEY_ITEMS']


def main():
    ids = {}
    for m in re.finditer(r'#define ITEM_(\w+)\s+(\d+)', open('include/constants/item.h').read()):
        ids.setdefault(m.group(1), int(m.group(2)))
    evo_items = set(re.findall(r'\{ EVO_(?:STONE\w*|TRADE_ITEM|ITEM_DAY|ITEM_NIGHT), ITEM_(\w+)', open('data/Evolutions.c').read()))
    src = open('data/itemdata/itemdata.c', encoding='utf-8').read()
    pool = []
    for m in re.finditer(r'\[ITEM_(\w+)\] =\s*\{(.*?)\n\}', src, re.S):
        name, body = m.group(1), m.group(2)
        if name not in ids or ids[name] == 0:
            continue
        price = re.search(r'ITEM_PRICE\((\d+)\)', body)
        price = int(price.group(1)) if price else 0
        pocket = re.search(r'\.fieldPocket = (\w+)', body)
        pocket = pocket.group(1) if pocket else ''
        hold = re.search(r'\.holdEffect = (\w+)', body)
        hold = hold.group(1) if hold else '0'
        if pocket not in POOL_POCKETS or price == 0 or name in BANNED:
            continue
        if name.endswith('_Z') or '_Z_' in name or name.endswith('_ORAS') or name.startswith('DUMMY'):
            continue
        f = 0
        if name in BAD or pocket == 'POCKET_MAIL' or (pocket == 'POCKET_BERRIES' and name not in ('LUM_BERRY', 'SITRUS_BERRY')):
            f |= F_BAD
        if name in OVERPOWERED:
            f |= F_OVERPOWERED
        if name in evo_items:
            f |= F_EVO
        if name in X_ITEMS:
            f |= F_X
        if name in REGULAR_SHOP:
            f |= F_REGULAR
        if name in SELL_ONLY:
            f |= F_SELL_ONLY
        if pocket == 'POCKET_BERRIES' or name in CONSUMABLE_HELD or name.endswith('_GEM'):
            f |= F_CONSUMABLE
        if hold not in ('0', 'HOLD_EFFECT_NONE') and pocket != 'POCKET_MAIL':
            f |= F_HELD
        pool.append((ids[name], name, f, POCKETS.index(pocket)))
    pool.sort()
    out = ['// AUTO-GENERATED by tools/rh/gen_rh_items.py from data/itemdata/itemdata.c - the random item pool (a028 member 24).',
           '#include "../../include/types.h"',
           '#include "../../include/constants/item.h"',
           '#include "../../include/rh/rh_data.h"',
           '',
           'const struct { u16 count, pad; struct RhItem items[%d]; } sRhItemFile = { %d, 0, {' % (len(pool), len(pool))]
    for i, name, f, p in pool:
        out.append('    { ITEM_%s, 0x%02X, %d },' % (name, f, p))
    out.append('} };')
    open('data/rh/RhItems.c', 'w', newline='\n').write('\n'.join(out) + '\n')

    # field items: item balls (pret's scr_seq_0141) and hidden items (data/HiddenItems.c)
    balls = re.findall(r'scr_seq_0141_(\d+):\n\tSetVar VAR_SPECIAL_x8008, (\d+)\n\tSetVar VAR_SPECIAL_x8009, (\d+)',
                       open('tools/rh/pret/scr_seq_0141.s').read())
    hidden = re.findall(r'\{ ITEM_(\w+), (\d+), \d+, \d+, (\d+) \}', open('data/HiddenItems.c').read())
    field = [(int(n), int(item)) for n, item, q in balls] + [(0x1000 + int(idx), ids[name]) for name, q, idx in hidden]
    field.sort()
    out = ['// AUTO-GENERATED by tools/rh/gen_rh_items.py - every item ball / hidden item (a028 member 25).',
           '#include "../../include/types.h"',
           '#include "../../include/rh/rh_data.h"',
           '',
           'const struct { u16 count, pad; struct RhFieldItem items[%d]; } sRhFieldItemFile = { %d, 0, {' % (len(field), len(field))]
    for k, it in field:
        out.append('    { 0x%04X, %d },' % (k, it))
    out.append('} };')
    open('data/rh/RhFieldItems.c', 'w', newline='\n').write('\n'.join(out) + '\n')

    # the item ball scripts, in hg-engine's script syntax
    s = ['// AUTO-GENERATED by tools/rh/gen_rh_items.py from pret/pokeheartgold scr_seq_0141 (item balls), with the',
         '// romhack\'s field item randomizer: each ball sets VAR_SPECIAL_x800A = its number, the common part asks the',
         '// randomizer (script command 490 = RH_SPECIAL_ITEM_BALL) for the item it holds.',
         '.include "asm/include/interop_macros.inc"',
         '.include "asm/include/scriptmacros.inc"',
         '.include "asm/include/vars.inc"',
         '.include "asm/include/soundeffects.inc"',
         '',
         '.data', '']
    for n, item, q in balls:
        s.append('scrdef scr_seq_0141_%s' % n)
    s.append('scrdef ItemBallCommon')                    # entry 255, as in pret
    s.append('scrdef_end')
    s.append('')
    for n, item, q in balls:
        s += ['scr_seq_0141_%s:' % n,
              '    setvar VAR_SPECIAL_x8008, %s' % item,
              '    setvar VAR_SPECIAL_x8009, %s' % q,
              '    setvar VAR_SPECIAL_x800A, %d' % int(n),
              '    goto ItemBallCommon',
              '    end', '']
    s.append(ITEM_BALL_COMMON)
    open('data/scr_seq/scr_seq_00141_itemballs.s', 'w', newline='\n').write('\n'.join(s) + '\n')
    print('pool', len(pool), 'field', len(field), 'balls', len(balls))


# pret's common part (scr_seq_0141_255 .. end), with the randomizer call after the item is copied
ITEM_BALL_COMMON = '''ItemBallCommon:
    play_se SEQ_SE_DP_SELECT
    lockall
    faceplayer
    wait_se SEQ_SE_DP_SELECT
    copyvar VAR_SPECIAL_x8004, VAR_SPECIAL_x8008
    copyvar VAR_SPECIAL_x8005, VAR_SPECIAL_x8009
    setvar VAR_SPECIAL_RESULT, 1
    nop_var_490 VAR_SPECIAL_RESULT
    hasspaceforitem VAR_SPECIAL_x8004, VAR_SPECIAL_x8005, VAR_SPECIAL_RESULT
    compare VAR_SPECIAL_RESULT, 1
    goto_if_eq ItemBallGive
    compare VAR_SPECIAL_RESULT, 0
    goto_if_eq ItemBallFull
    end

ItemBallClose:
    closemsg
    releaseall
    end

ItemBallGive:
    callstd 2001                          // std_hidden_item_fanfare
    hide_person VAR_SPECIAL_LAST_TALKED
    giveitem VAR_SPECIAL_x8004, VAR_SPECIAL_x8005, VAR_SPECIAL_RESULT
    itemistmorhm VAR_SPECIAL_x8004, VAR_SPECIAL_RESULT
    compare VAR_SPECIAL_RESULT, 1
    goto_if_eq ItemBallTM
    compare VAR_SPECIAL_RESULT, 0
    goto_if_eq ItemBallItem
    end

ItemBallPut:
    wait_fanfare
    buffer_players_name 0
    buffer_item_name 1, VAR_SPECIAL_x8004
    getitempocket VAR_SPECIAL_x8004, VAR_SPECIAL_RESULT
    buffer_pocket_name 2, VAR_SPECIAL_RESULT
    npc_msg 9
    wait_button
    setvar VAR_SPECIAL_RESULT, 1
    goto ItemBallClose
    end

ItemBallTM:
    buffer_players_name 0
    buffer_item_name_indef 1, VAR_SPECIAL_x8004
    buffer_tmhm_move_name 2, VAR_SPECIAL_x8004
    npc_msg 6
    goto ItemBallPut
    end

ItemBallItem:
    buffer_players_name 0
    buffer_item_name_indef 1, VAR_SPECIAL_x8004
    npc_msg 3
    goto ItemBallPut
    end

ItemBallFull:
    buffer_item_name 0, VAR_SPECIAL_x8004
    npc_msg 4
    wait_button
    setvar VAR_SPECIAL_RESULT, 0
    goto ItemBallClose
    end

    .balign 4, 0'''

if __name__ == '__main__':
    main()
