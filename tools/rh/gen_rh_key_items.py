#!/usr/bin/env python3
# Romhack: the romhack's key items as hg-engine custom items (ITEM_RANDOMIZER_SETTINGS = MAX_BASE_ITEM_NUM + 1):
# item data (data/itemdata/itemdata.c), name (text bank 222, by item id), description / article / plural / "give"
# texts (custom item banks 221, 223, 224, 829, by custom index), icon (data/graphics/itemgra.mk: member item + 2,
# before the dummy and return icons). Idempotent.
# Usage: python3 tools/rh/gen_rh_key_items.py   (from the repository root; run gen_rh_settings_icon.py for the icon)
import re

ITEM_ID = 2685                     # include/constants/item.h ITEM_RANDOMIZER_SETTINGS
FIELD_USE = 36                     # src/item.c sNewItemFieldUseFuncs[6] (30 vanilla + 6)
NAME = 'Rando Settings'        # the bag's item name buffer is short: 'Randomizer Settings' showed blank
DESCRIPTION = r'Opens the randomizer\nsettings to change\nor reset your run.'
ARTICLE = 'the Randomizer Settings'
PLURAL = 'Randomizer Settings'
ICON = 'rh_randomizer_settings'

ITEM_DATA = '''[ITEM_RANDOMIZER_SETTINGS] =
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
''' % FIELD_USE


def lines(path):
    t = open(path, encoding='utf-8').read()
    return t[:-1].split('\n') if t.endswith('\n') else t.split('\n')


def write(path, ls):
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(ls) + '\n')


def main():
    # item data: after the last item's entry
    src = open('data/itemdata/itemdata.c', encoding='utf-8').read()
    if '[ITEM_RANDOMIZER_SETTINGS]' not in src:
        end = src.rindex('};')
        src = src[:end] + ITEM_DATA + '\n' + src[end:]
        open('data/itemdata/itemdata.c', 'w', encoding='utf-8', newline='\n').write(src)
    # names by item id
    names = lines('data/text/222.txt')
    # (msgenc drops empty entries, which shifted every later item's name: unnamed ids get a space)
    names = [n if n != '' else ' ' for n in names[:ITEM_ID]] + [NAME]
    write('data/text/222.txt', names)
    # the per-generation item text banks with unused (empty) entries: same msgenc problem as the names
    for bank in (847, 848, 849, 851, 852, 853):
        path = 'data/text/%03d.txt' % bank
        write(path, [t if t != '' else ' ' for t in lines(path)])
    # custom item texts: index 0
    for bank, text in ((221, DESCRIPTION), (223, ARTICLE), (224, PLURAL), (829, ARTICLE)):
        write('data/text/%03d.txt' % bank, [text])
    # icon: member pair ITEM_ID + 2, then the dummy and return icons
    mk = open('data/graphics/itemgra.mk', encoding='utf-8').read()
    if ICON not in mk:
        for old, new in (('return', 2690), ('dummy', 2689)):
            mk = re.sub(r'(\d{4})(-0[01]\.(?:NCGR|NCLR))(:?\$\(ITEMGFX_DEPENDENCIES_DIR\)/%s\.png)' % old,
                        lambda m: '%04d%s%s' % (new - 1, m.group(2), m.group(3)), mk)
        # the OBJS / PALS lines for the moved icons and the new one
        mk = mk.replace('$(ITEMGFX_DIR)/2688-00.NCGR\nITEMGFX_PALS += $(ITEMGFX_DIR)/2688-01.NCLR',
                        '$(ITEMGFX_DIR)/2689-00.NCGR\nITEMGFX_PALS += $(ITEMGFX_DIR)/2689-01.NCLR')
        mk = mk.replace('$(ITEMGFX_DIR)/2687-00.NCGR\nITEMGFX_PALS += $(ITEMGFX_DIR)/2687-01.NCLR',
                        '$(ITEMGFX_DIR)/2688-00.NCGR\nITEMGFX_PALS += $(ITEMGFX_DIR)/2688-01.NCLR')
        n = ITEM_ID + 2
        block = ('$(ITEMGFX_DIR)/%04d-00.NCGR:$(ITEMGFX_DEPENDENCIES_DIR)/%s.png\n'
                 '\t$(GFX) $< $@ -clobbersize -version101 -bitdepth 4\n\n'
                 '$(ITEMGFX_DIR)/%04d-01.NCLR:$(ITEMGFX_DEPENDENCIES_DIR)/%s.png\n'
                 '\t$(GFX) $< $@ -ir -bitdepth 4\n\n'
                 'ITEMGFX_SRCS += $(ITEMGFX_DEPENDENCIES_DIR)/%s.png\n'
                 'ITEMGFX_OBJS += $(ITEMGFX_DIR)/%04d-00.NCGR\n'
                 'ITEMGFX_PALS += $(ITEMGFX_DIR)/%04d-01.NCLR\n\n') % (n, ICON, n, ICON, ICON, n, n)
        at = mk.index('$(ITEMGFX_DIR)/%04d-00.NCGR:$(ITEMGFX_DEPENDENCIES_DIR)/dummy.png' % (n + 1))
        mk = mk[:at] + block + mk[at:]
        open('data/graphics/itemgra.mk', 'w', encoding='utf-8', newline='\n').write(mk)
    print('key items: Randomizer Settings =', ITEM_ID)


if __name__ == '__main__':
    main()
