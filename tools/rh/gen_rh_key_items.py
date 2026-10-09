#!/usr/bin/env python3
# Romhack: the romhack's key items as hg-engine custom items (ids MAX_BASE_ITEM_NUM + 1.., include/constants/item.h):
# item data (data/itemdata/itemdata.c), name (text bank 222, by item id), description / article / plural / "give"
# texts (custom item banks 221, 223, 224, 829, by custom index), icon (data/graphics/itemgra.mk: file number item + 2,
# then the dummy and return icons). The icons are the owner's / the FireRed romhack's pixel art in
# data/graphics/item (art at (1,1), at most 28x28; the Healing Kit icon is from HeartGold Generations, RH_CREDITS.md).
# Idempotent.
# Usage: python3 tools/rh/gen_rh_key_items.py   (from the repository root)
import re

FIRST_ID = 2685                    # MAX_BASE_ITEM_NUM + 1

# (constant, name, description, article, plural, icon, item data: ('key', field use) or ('copy', item))
ITEMS = [
    ('RANDOMIZER_SETTINGS', 'Rando Settings',     # the bag's name buffer can't fit 'Randomizer Settings'
     r'Opens the randomizer\nsettings to change\nor reset your run.',
     'the Rando Settings', 'Rando Settings', 'rh_randomizer_settings', ('key', 36)),
    ('INFINITE_CANDY', 'Infinite Candy',
     r'A Rare Candy that\nnever runs out.\nRaises a level.',
     'an Infinite Candy', 'Infinite Candies', 'rh_infinite_candy', ('copy', 'RARE_CANDY')),
    ('HEALING_KIT', 'Healing Kit',
     r'Fully heals your\nwhole party, just\nlike a Pokémon Center.',
     'a Healing Kit', 'Healing Kits', 'rh_healing_kit', ('key', 37)),
]

KEY_ITEM_DATA = '''[ITEM_%s] =
{
    ITEM_PRICE(0),
    .holdEffect = 0,
    .holdEffectParam = 0,
    .pluckEffect = 0,
    .flingEffect = 0,
    .flingPower = 0,
    .naturalGiftPower = 0,
    .naturalGiftType = 31,
    .prevent_toss = TRUE,
    .selectable = TRUE,
    .fieldPocket = POCKET_KEY_ITEMS,
    .battlePocket = BATTLE_POCKET_NONE,
    .fieldUseFunc = %d,
    .battleUseFunc = 0,
    .partyUse = 0,
},
'''

ICON_RULES = '''$(ITEMGFX_DIR)/{n:04d}-00.NCGR:$(ITEMGFX_DEPENDENCIES_DIR)/{icon}.png
\t$(GFX) $< $@ -clobbersize -version101 -bitdepth 4

$(ITEMGFX_DIR)/{n:04d}-01.NCLR:$(ITEMGFX_DEPENDENCIES_DIR)/{icon}.png
\t$(GFX) $< $@ -ir -bitdepth 4

ITEMGFX_SRCS += $(ITEMGFX_DEPENDENCIES_DIR)/{icon}.png
ITEMGFX_OBJS += $(ITEMGFX_DIR)/{n:04d}-00.NCGR
ITEMGFX_PALS += $(ITEMGFX_DIR)/{n:04d}-01.NCLR

'''


def lines(path):
    t = open(path, encoding='utf-8').read()
    return t[:-1].split('\n') if t.endswith('\n') else t.split('\n')


def write(path, ls):
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(ls) + '\n')


def item_data(src, const, data):
    kind, arg = data
    if kind == 'key':
        return KEY_ITEM_DATA % (const, arg)
    # a key item copy of another item (its party use parameters stay): no price, Key Items pocket, can't be tossed
    body = re.search(r'\n\[ITEM_%s\] =\n(\{.*?\n\},\n)' % arg, src, re.S).group(1)
    body = re.sub(r'ITEM_PRICE\(\d+\)', 'ITEM_PRICE(0)', body)
    body = re.sub(r'\.fieldPocket = \w+', '.fieldPocket = POCKET_KEY_ITEMS', body)
    body = re.sub(r'\.prevent_toss = \w+', '.prevent_toss = TRUE', body)
    body = re.sub(r'\.flingPower = \d+', '.flingPower = 0', body)
    return '[ITEM_%s] =\n%s' % (const, body)


def main():
    # item data: the romhack's entries at the end of the table (rebuilt each run)
    src = open('data/itemdata/itemdata.c', encoding='utf-8').read()
    src = re.sub(r'// romhack key items \(tools/rh/gen_rh_key_items\.py\)\n.*?// end of romhack key items\n', '', src,
                 flags=re.S)
    src = re.sub(r'\[ITEM_RANDOMIZER_SETTINGS\] =\n\{.*?\n\},\n\n?', '', src, flags=re.S)    # first version's entry
    block = ''.join(item_data(src, it[0], it[6]) + '\n' for it in ITEMS)
    end = src.rindex('};')
    src = (src[:end] + '// romhack key items (tools/rh/gen_rh_key_items.py)\n' + block +
           '// end of romhack key items\n' + src[end:])
    open('data/itemdata/itemdata.c', 'w', encoding='utf-8', newline='\n').write(src)
    # names by item id (msgenc drops empty entries, which shifted every later item's name: unnamed ids get a space)
    names = [n if n != '' else ' ' for n in lines('data/text/222.txt')[:FIRST_ID]]
    write('data/text/222.txt', names + [it[1] for it in ITEMS])
    # the per-generation item text banks with unused (empty) entries: same msgenc problem
    for bank in (847, 848, 849, 851, 852, 853):
        path = 'data/text/%03d.txt' % bank
        write(path, [t if t != '' else ' ' for t in lines(path)])
    # custom item texts, by custom index
    for bank, col in ((221, 2), (223, 3), (224, 4), (829, 3)):
        write('data/text/%03d.txt' % bank, [it[col] for it in ITEMS])
    # icons: the romhack's after the last item's (file number = item + 2), then the dummy and return icons
    mk = open('data/graphics/itemgra.mk', encoding='utf-8').read()
    first = FIRST_ID + 2
    start = mk.index('$(ITEMGFX_DIR)/%04d-00.NCGR:' % first)
    end = mk.index('$(ITEMGFX_NARC):')
    icons = [it[5] for it in ITEMS] + ['dummy', 'return']
    mk = mk[:start] + ''.join(ICON_RULES.format(n=first + i, icon=ic) for i, ic in enumerate(icons)) + '\n\n' + mk[end:]
    open('data/graphics/itemgra.mk', 'w', encoding='utf-8', newline='\n').write(mk)
    print('key items:', ', '.join('%s = %d' % (it[0], FIRST_ID + i) for i, it in enumerate(ITEMS)))


if __name__ == '__main__':
    main()
