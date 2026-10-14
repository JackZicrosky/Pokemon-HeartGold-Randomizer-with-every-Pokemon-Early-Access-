# test only: capture UI screens in Default (MODE=def) or colour-id (MODE=id, uiTheme 3) mode.
# Work folder (captures in WORK/caps, test saves in WORK): $RH_AMOLED_WORK, default: this folder.
# usage: cap.py MODE SAVE|none TOKENS...
#   theme      poke uiTheme (0 in def mode, 3 in id mode) into every settings copy
#   idreset    restart the palette slot log (do it right before a screen loads)
#   cap:NAME   save caps/MODE_NAME.png (+ caps/id_NAME.json: the slot log)
#   Kn / K     hold key (A B X Y L R S=start s=select U D l r) n frames; Wn wait; Tx,y touch
#   goto:x,z   walk (play.goto); nowild
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'test'))
import play
from dsharness import Harness
SP = os.environ.get('RH_AMOLED_WORK', os.path.dirname(os.path.abspath(__file__)))
OUT = SP + '/caps'
os.makedirs(OUT, exist_ok=True)
mode, save = sys.argv[1], sys.argv[2]
cont = not save.endswith('@'); save = save.rstrip('@')      # SAVE@ = stay on the title screen
h = Harness(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'test.nds'), None if save == 'none' else SP + '/' + save)
m = h.emu.memory.unsigned
h.run(600)
if save != 'none' and cont:
    h.hold('A', 4); h.run(300); h.hold('A', 4); h.run(300); h.hold('A', 4); h.run(500)
LOG = None
def log_addr():
    global LOG
    if LOG is None:
        for a in range(0x023D8000, 0x02400000, 4):
            if m[a] == 0x44 and m[a+1] == 0x50 and m[a+2] == 0x48 and m[a+3] == 0x52:
                LOG = a; break
    return LOG
for k in sys.argv[3:]:
    if k == 'theme':
        for a in play.find_all_rh_settings(h.emu.memory):
            h.emu.memory.write_byte(a + 213, {'id': 3, 'am': 2}.get(mode, 0))
    elif k == 'idreset':
        a = log_addr()
        for i in range(4): h.emu.memory.write_byte(a + 4 + i, 0)
    elif k.startswith('cap:'):
        n = k[4:]
        h.emu.screenshot().save('%s/%s_%s.png' % (OUT, mode, n))
        if mode == 'id':
            a = log_addr(); rd = lambda o: m[a+o] | (m[a+o+1] << 8)
            cnt = rd(4) | rd(6) << 16
            json.dump({'count': cnt, 'narc': [rd(8 + 2*i) for i in range(64)], 'file': [rd(136 + 2*i) for i in range(64)]},
                      open('%s/id_%s.json' % (OUT, n), 'w'))
            if cnt > 64: print('WARN', n, 'slots', cnt)
    elif k.startswith('goto:'):
        x, z = k[5:].split(','); play.goto(h, m, int(x), int(z))
    elif k == 'nowild':
        play.NO_WILD = True
    elif k == 'battle':                          # Route 30 grass: walk until a wild battle starts
        play.goto(h, m, 554, 331)
        def in_battle():
            img = h.emu.screenshot().convert('RGB')
            r, g, b = img.getpixel((8, 300)); r2, g2, b2 = img.getpixel((232, 220))
            return not (g > 150 and r < 100) and not (r2 > 150 and g2 > 150 and b2 > 150)
        for i in range(0 if in_battle() else 120):
            h.hold('U' if i % 2 == 0 else 'D', 16); h.run(4)
            if in_battle(): break
    elif k[0] == 'P':                             # Px,y: touch and stay pressed (button pressed look)
        x, y = k[1:].split(',')
        h.emu.input.touch_set_pos(int(x), int(y)); h.run(6)
    elif k == 'rel':
        h.emu.input.touch_release(); h.run(20)
    elif k.startswith('paldump:'):              # paldump:NAME  all 4 palette RAMs (2 KB) -> caps/NAME.pal
        open('%s/%s.pal' % (OUT, k[8:]), 'wb').write(bytes(m[0x05000000 + i] for i in range(0x800)))
    elif k == 'amdbg':
        for a in range(0x023D8000, 0x02400000, 4):
            if m[a] == 0x52 and m[a+1] == 0x48 and m[a+2] == 0x41 and m[a+3] == 0x44:
                print('amdbg', [m.read_long(a + 4 * i) for i in range(1, 6)]); break
    elif k[0] == 'W':
        h.run(int(k[1:]))
    elif k[0] == 'T':
        x, y = k[1:].split(',')
        h.command(['touch', x, y, '10']); h.run(20)
    else:
        h.hold(k[0], int(k[1:]) if len(k) > 1 else 4); h.run(8)
print('end', mode, play.player(m))
