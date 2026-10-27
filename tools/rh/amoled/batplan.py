# prints the cap.py tokens that record every battle menu state (300 frames idle + 300 frames after every input)
# usage: batplan.py TIME MODE   (needs bpre_TIME.dst: a savestate as the battle starts)
import sys
t, mode = sys.argv[1], sys.argv[2]
KEYS = ['U', 'D', 'L', 'R', 'A', 'B', 'X', 'Y', 'l', 'r', 'S', 's']
# state: (how to get there from the command menu state, touch inputs to try there)
STATES = [
    ('cmd', '', [('fight', 'T128_83'), ('bag', 'T40_170'), ('run', 'T128_172'), ('pokemon', 'T216_168')]),
    ('moves', 'T128,83 W300', [('move1', 'T64_40'), ('move2', 'T192_40'), ('move3', 'T64_100'), ('cancel', 'T128_172')]),
    ('bag', 'T40,170 W400', [('hpp', 'T64_40'), ('balls', 'T192_40'), ('status', 'T64_110'), ('battleitems', 'T192_110'), ('back', 'T240_176')]),
    ('bagitems', 'T40,170 W400 T64,40 W400', [('item1', 'T64_30'), ('prev', 'T20_176'), ('next', 'T60_176'), ('back', 'T240_176')]),
    ('itemuse', 'T40,170 W400 T64,40 W400 T64,30 W400', [('use', 'T128_172'), ('back', 'T240_176')]),
    ('party', 'T216,168 W400', [('slot1', 'T64_40'), ('slot2', 'T192_40'), ('back', 'T240_176')]),
    ('partymenu', 'T216,168 W400 T64,40 W400', [('shift', 'T128_100'), ('summary', 'T50_172'), ('checkmoves', 'T155_172'), ('back', 'T240_176')]),
    ('summary', 'T216,168 W400 T64,40 W400 T50,172 W500', [('up', 'T20_172'), ('down', 'T60_172'), ('checkmoves', 'T155_172'), ('back', 'T240_176')]),
    ('checkmoves', 'T216,168 W400 T64,40 W400 T155,172 W500', [('move1', 'T64_70'), ('up', 'T20_172'), ('down', 'T60_172'), ('summary', 'T155_172'), ('back', 'T240_176')]),
]
out = [' '.join(['sload:bpre_%s' % t, 'theme', 'rec:%s_bat_intro:800' % t, 'ssave:bcmd_%s_%s' % (t, mode)])]
for name, nav, touches in STATES:
    c = ['sload:bcmd_%s_%s' % (t, mode)] + nav.split() + ['ssave:bst_%s_%s' % (t, mode), 'rec:%s_bat_%s:300' % (t, name)]
    for k in KEYS:
        c += ['sload:bst_%s_%s' % (t, mode), 'rec:%s_bat_%s_key%s:300:%s' % (t, name, k if k.isupper() else k + k, k)]
    for tn, tt in touches:
        c += ['sload:bst_%s_%s' % (t, mode), 'rec:%s_bat_%s_%s:300:%s' % (t, name, tn, tt)]
    out.append(' '.join(c))
print('\n'.join(out))
