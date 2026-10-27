# prints the cap.py tokens that record every field-side menu state (300 frames idle + 300 frames after every input)
# usage: fieldplan.py TIME MODE   (needs fs_TIME.dst: a savestate standing on Route 30)
import sys
t, mode = sys.argv[1], sys.argv[2]
KEYS = ['U', 'D', 'L', 'R', 'A', 'B', 'X', 'Y', 'l', 'r', 'S', 's']
FM = [('dex', 'T43_41'), ('party', 'T43_81'), ('bag', 'T43_121'), ('gear', 'T43_161'), ('card', 'T123_41'),
      ('save', 'T123_81'), ('options', 'T123_121'), ('close', 'T240_176')]
POCKETS = [('p%d' % x, 'T%d_16' % x) for x in (16, 48, 80, 112, 144, 176, 208, 240)]
STATES = [
    ('field', '', []),
    ('fieldmenu', 'X W100', FM),
    ('dex', 'X W100 T43,41 W200', [('list', 'T128_100'), ('back', 'T240_176')]),
    ('dexlist', 'X W100 T43,41 W200 A W120', [('entry', 'T128_60'), ('search', 'T30_178'), ('back', 'T240_176')]),
    ('dexarea', 'X W100 T43,41 W200 A W120 A W150', [('back', 'T240_176')]),
    ('dexsize', 'X W100 T43,41 W200 A W120 A W150 R W80', [('back', 'T240_176')]),
    ('dexforms', 'X W100 T43,41 W200 A W120 A W150 R W80 R W80', [('back', 'T240_176')]),
    ('dexsearch', 'X W100 T43,41 W200 A W120 T30,178 W300', [('back', 'T240_176')]),
    ('card', 'X W100 T123,41 W200', [('flip', 'T128_100'), ('back', 'T240_176')]),
    ('cardback', 'X W100 T123,41 W200 T128,100 W120', [('flip', 'T128_100'), ('back', 'T240_176')]),
    ('save', 'X W100 T123,81 W100', [('yes', 'T232_140'), ('no', 'T232_166')]),
    ('options', 'X W100 T123,121 W200', []),
    ('gearphone', 'X W100 T43,161 W200', [('skins', 'T30_180'), ('map', 'T128_180'), ('phone', 'T176_180'), ('close', 'T240_180'), ('call', 'T64_50')]),
    ('gearskins', 'X W100 T43,161 W200 T30,180 W150', [('skin2', 'T128_44'), ('skin3', 'T208_44'), ('map', 'T128_180'), ('close', 'T240_180')]),
    ('gearmap', 'X W100 T43,161 W200 T128,180 W150', [('zoom', 'T128_100'), ('phone', 'T176_180'), ('close', 'T240_180')]),
    ('party', 'X W100 T43,81 W200', [('slot1', 'T64_40'), ('slot2', 'T192_40'), ('back', 'T240_176')]),
    ('partymenu', 'X W100 T43,81 W200 A W60', [('summary', 'T200_80'), ('back', 'T240_176')]),
    ('summary', 'X W100 T43,81 W200 A W60 A W200', [('next', 'T240_20'), ('back', 'T240_176')]),
    ('summary2', 'X W100 T43,81 W200 A W60 A W200 R W80', [('back', 'T240_176')]),
    ('summary3', 'X W100 T43,81 W200 A W60 A W200 R W80 R W80', [('back', 'T240_176')]),
    ('summarymoves', 'X W100 T43,81 W200 A W60 A W200 R W80 R W80 R W80', [('move1', 'T128_60'), ('back', 'T240_176')]),
    ('bag', 'X W100 T43,121 W200', POCKETS + [('item1', 'T64_60'), ('back', 'T240_176')]),
    ('bagmenu', 'X W100 T43,121 W200 T64,60 W100', [('use', 'T50_140'), ('back', 'T240_176')]),
]
out = []
for name, nav, touches in STATES:
    c = ['sload:fs_%s_%s' % (t, mode), 'theme'] + nav.split() + ['ssave:fst_%s_%s' % (t, mode), 'rec:%s_fld_%s:300' % (t, name)]
    for k in KEYS:
        c += ['sload:fst_%s_%s' % (t, mode), 'rec:%s_fld_%s_key%s:300:%s' % (t, name, k if k.isupper() else k + k, k)]
    for tn, tt in touches:
        c += ['sload:fst_%s_%s' % (t, mode), 'rec:%s_fld_%s_%s:300:%s' % (t, name, tn, tt)]
    out.append(' '.join(c))
print('\n'.join(out))
