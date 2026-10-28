# test only: capture UI screens in Default (MODE=def) or colour-id (MODE=id, uiTheme 3) mode.
# usage: cap.py MODE SAVE|none TOKENS...
#   theme      poke uiTheme (0 in def mode, 3 in id mode) into every settings copy
#   idreset    restart the palette slot log (do it right before a screen loads)
#   cap:NAME   save caps/MODE_NAME.png (+ caps/id_NAME.json: the slot log)
#   Kn / K     hold key (A B X Y L R S=start s=select U D l r) n frames; Wn wait; Tx,y touch
#   goto:x,z   walk (play.goto); nowild
import json, os, sys
sys.path.insert(0, '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/source/tools/rh/test')
import play
from dsharness import Harness
SP = '/root/amw'
OUT = SP + '/caps'
os.makedirs(OUT, exist_ok=True)
mode, save = sys.argv[1], sys.argv[2]
cont = not save.endswith('@'); save = save.rstrip('@')      # SAVE@ = stay on the title screen
h = Harness(os.environ.get('ROM', '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/source/test.nds'), None if save == 'none' else SP + '/' + save)
m = h.emu.memory.unsigned
if os.environ.get('RTC'):                       # RTC="YYYY-MM-DD HH:MM": a fixed clock (time of day, and the same RNG every run)
    from desmume.emulator import DeSmuME_Date, StartFrom
    import re
    y, mo, d, hh, mi = map(int, re.findall(r'\d+', os.environ['RTC']))
    blank = '/tmp/cap_blank_%d.sav' % os.getpid()
    if save == 'none':
        open(blank, 'wb').write(bytes([255]) * 0x80000)
    h.emu.movie.record('/tmp/cap_%d.dsm' % os.getpid(), 'rh', StartFrom.START_SRAM,
                       blank if save == 'none' else SP + '/' + save, DeSmuME_Date(y, mo, d, hh, mi, 0, 0))
h.run(600)
if save != 'none' and cont:
    h.hold('A', 4); h.run(300); h.hold('A', 4); h.run(300); h.hold('A', 4); h.run(500)
def io_regs():
    # the display registers of both engines, with the write-only ones (BG scroll...) as the game wrote them: from a
    # savestate (DeSmuME keeps every written register value there); 0x70 bytes each
    rd = lambda a, n: bytes(h.emu.memory.read(a, a + n, 1, False))[:n]
    tmp = '/tmp/cap_%d.dst' % os.getpid()
    h.emu.savestate.save_file(tmp)
    st = open(tmp, 'rb').read(); os.remove(tmp)
    out = b''
    i = -1
    for base in (0x04000000, 0x04001000):         # (the sub engine's block comes after the main one)
        live = rd(base, 0x70)
        i = st.find(live[:12], i + 1)
        out += st[i:i + 0x70] if i >= 0 else live
    return out

LOG = None
def log_addr():
    # the log is on the heap (allocated by the first id-mode palette read); its signature is also in overlay 129's code
    ram = bytes(h.emu.memory.read(0x02000000, 0x023FFFFF, 1, False))
    sig = bytes([0x44, 0x50, 0x48, 0x52, 0xBB, 0xAF, 0xB7, 0xAD])     # "RHPD", ~"RHPD"
    i = ram.find(sig)
    while i >= 0 and 0x023D8000 <= 0x02000000 + i < 0x023E0000:
        i = ram.find(sig, i + 1)
    return 0x02000000 + i + 4 if i >= 0 else None      # +4: the reads below expect count at LOG + 4
for k in sys.argv[3:]:
    if k == 'theme':
        for a in play.find_all_rh_settings(h.emu.memory):
            h.emu.memory.write_byte(a + 213, {'id': 3, 'am': 2}.get(mode, 0))
        if mode == 'am' and os.environ.get('NOPAGES'):          # (the captures learn.py reads: no screen pages)
            ram = bytes(h.emu.memory.read(0x023D8000, 0x023FFFFF, 1, False)); j = ram.find(b'RHSG')
            for i, v in enumerate((0x45474150).to_bytes(4, 'little')):
                h.emu.memory.write_byte(0x023D8000 + j + 16 + i, v)
    elif k == 'idreset':
        a = log_addr()
        if a is not None:
            for i in range(4): h.emu.memory.write_byte(a + 4 + i, 0)
    elif k.startswith('cap:'):
        n = k[4:]
        if mode == 'am' and os.path.exists('%s/id_%s.json' % (OUT, n)):
            # wait (up to 2 s) for the screen the colour-id capture saw (same fingerprint): the runs drift apart
            want = json.load(open('%s/id_%s.json' % (OUT, n))).get('sig')
            for _ in range(120):
                ram = bytes(h.emu.memory.read(0x023D8000, 0x023FFFFF, 1, False)); j = ram.find(b'RHSG')
                if want is None or j < 0 or int.from_bytes(ram[j + 4:j + 8], 'little') == want:
                    break
                h.run(1)
            else:
                print('WARN', n, 'screen never matched the id capture')
        h.emu.screenshot().save('%s/%s_%s.png' % (OUT, mode, n))
        if mode in ('id', 'am') or n.startswith('st'):   # registers, palettes, OAM, BG + OBJ VRAM (dsrender.py)
            rd = lambda a, n: bytes(h.emu.memory.read(a, a + n, 1, False))[:n]
            open('%s/%s_%s.vram2' % (OUT, mode, n), 'wb').write(io_regs() + rd(0x05000000, 0x800)
                + rd(0x07000000, 0x800) + rd(0x06000000, 0x80000) + rd(0x06200000, 0x20000) + rd(0x06400000, 0x40000) + rd(0x06600000, 0x20000))
        a = log_addr() if mode == 'id' else None
        if a is not None:
            rd = lambda o: m[a+o] | (m[a+o+1] << 8)
            cnt = rd(4) | rd(6) << 16
            ram = bytes(h.emu.memory.read(0x023D8000, 0x023FFFFF, 1, False)); j = ram.find(b'RHSG')
            sig = int.from_bytes(ram[j + 4:j + 8], 'little') if j >= 0 else None
            json.dump({'sig': sig, 'count': cnt, 'narc': [rd(8 + 2*i) for i in range(64)], 'file': [rd(136 + 2*i) for i in range(64)], 'ctx': [rd(264 + 2*i) for i in range(64)]},
                      open('%s/id_%s.json' % (OUT, n), 'w'))
            if cnt > 64: print('WARN', n, 'slots', cnt)
    elif k.startswith('give4:'):                 # give4:MOVE  every RAM party copy: first Pokemon's empty 4th move slot gets MOVE
        import struct
        mv = int(k[6:])
        ram = bytearray(h.emu.memory.read(0x02000000, 0x023FFFFF, 1, False))
        def crypt(buf, seed):
            out = bytearray(buf); x = seed
            for i in range(0, len(buf), 2):
                x = (x * 0x41C64E6D + 0x6073) & 0xFFFFFFFF
                v = struct.unpack_from('<H', buf, i)[0] ^ (x >> 16)
                struct.pack_into('<H', out, i, v)
            return out
        ORDER = ['ABCD','ABDC','ACBD','ACDB','ADBC','ADCB','BACD','BADC','BCAD','BCDA','BDAC','BDCA',
                 'CABD','CADB','CBAD','CBDA','CDAB','CDBA','DABC','DACB','DBAC','DBCA','DCAB','DCBA']
        pat = struct.pack('<I', 6); n = 0; i = ram.find(pat)
        while 0 <= i < len(ram) - 8 - 236:
            cnt = struct.unpack_from('<I', ram, i + 4)[0]
            if 1 <= cnt <= 6 and i % 4 == 0:
                o = i + 8
                pid, _, chk = struct.unpack_from('<IHH', ram, o)
                d = crypt(ram[o + 8:o + 136], chk)
                if sum(struct.unpack('<64H', d)) & 0xFFFF == chk and pid != 0:
                    order = ORDER[((pid >> 13) & 31) % 24]
                    b = order.index('B') * 32
                    moves = struct.unpack_from('<4H', d, b)
                    if moves[3] == 0 and moves[0] != 0:
                        struct.pack_into('<H', d, b + 6, mv); d[b + 8 + 3] = 20
                        chk2 = sum(struct.unpack('<64H', d)) & 0xFFFF
                        struct.pack_into('<H', ram, o + 6, chk2)
                        ram[o + 8:o + 136] = crypt(d, chk2)
                        for j in range(o + 6, o + 136): h.emu.memory.write_byte(0x02000000 + j, ram[j])
                        n += 1
                    print('give4 party at', hex(0x02000000 + i), 'moves', moves)
            i = ram.find(pat, i + 1)
        print('give4 patched', n)
    elif k.startswith('setmon:'):                # setmon:HP,ATK,DEF,SPE,SPA,SPD[,PP1]  every RAM party copy: the first
        import struct                             # Pokemon's current HP and stats (and its first move's PP)
        vals = [int(v) for v in k[7:].split(',')]
        ram = bytearray(h.emu.memory.read(0x02000000, 0x023FFFFF, 1, False))
        def crypt(buf, seed):
            out = bytearray(buf); x = seed
            for i in range(0, len(buf), 2):
                x = (x * 0x41C64E6D + 0x6073) & 0xFFFFFFFF
                struct.pack_into('<H', out, i, struct.unpack_from('<H', buf, i)[0] ^ (x >> 16))
            return out
        ORDER = ['ABCD','ABDC','ACBD','ACDB','ADBC','ADCB','BACD','BADC','BCAD','BCDA','BDAC','BDCA',
                 'CABD','CADB','CBAD','CBDA','CDAB','CDBA','DABC','DACB','DBAC','DBCA','DCAB','DCBA']
        pat = struct.pack('<I', 6); n = 0; i = ram.find(pat)
        while 0 <= i < len(ram) - 8 - 236:
            cnt = struct.unpack_from('<I', ram, i + 4)[0]
            if 1 <= cnt <= 6 and i % 4 == 0:
                o = i + 8
                pid, _, chk = struct.unpack_from('<IHH', ram, o)
                d = crypt(ram[o + 8:o + 136], chk)
                if sum(struct.unpack('<64H', d)) & 0xFFFF == chk and pid != 0:
                    ext = crypt(ram[o + 136:o + 236], pid)
                    struct.pack_into('<H', ext, 6, vals[0])                     # current HP
                    for j, v in enumerate(vals[1:6]):
                        struct.pack_into('<H', ext, 10 + 2 * j, v)             # attack, defense, speed, sp. atk, sp. def
                    ram[o + 136:o + 236] = crypt(ext, pid)
                    if len(vals) > 6:
                        b = ORDER[((pid >> 13) & 31) % 24].index('B') * 32
                        d[b + 8] = vals[6]
                        chk2 = sum(struct.unpack('<64H', d)) & 0xFFFF
                        struct.pack_into('<H', ram, o + 6, chk2)
                        ram[o + 8:o + 136] = crypt(d, chk2)
                    for j in range(o + 6, o + 236): h.emu.memory.write_byte(0x02000000 + j, ram[j])
                    n += 1
            i = ram.find(pat, i + 1)
        print('setmon patched', n)
    elif k.startswith('rec:'):                   # rec:NAME[:FRAMES[:INPUT]]  every frame for FRAMES frames (default 300 = 5 s)
        # INPUT (held for the first 6 frames): a key (A B X Y U D l r S s L R) or Tx_y (touch). Frames identical to an
        # earlier one of the same recording are stored once; index.json gives the stored frame of every frame.
        import hashlib
        parts = k.split(':'); name = parts[1]; nfr = int(parts[2]) if len(parts) > 2 else 300
        inp = parts[3] if len(parts) > 3 else None
        d = '%s/rec/%s_%s' % (SP, mode, name); os.makedirs(d, exist_ok=True)
        seen = {}; index = []
        for fi in range(nfr):
            if inp and fi == 0:
                if inp[0] == 'T':
                    x, y = inp[1:].split('_'); h.emu.input.touch_set_pos(int(x), int(y))
                else:
                    h.emu.input.keypad_add_key(__import__('desmume.controls', fromlist=['keymask']).keymask(__import__('dsharness').KEYMAP[inp]))
            if inp and fi == 6:
                if inp[0] == 'T':
                    h.emu.input.touch_release()
                else:
                    h.emu.input.keypad_rm_key(__import__('desmume.controls', fromlist=['keymask']).keymask(__import__('dsharness').KEYMAP[inp]))
            h.run(1)
            img = h.emu.screenshot()
            hs = hashlib.md5(img.tobytes()).hexdigest()
            if hs not in seen:
                seen[hs] = 'f%03d.png' % fi; img.save(d + '/' + seen[hs])
            index.append(seen[hs])
        json.dump({'mode': mode, 'input': inp, 'frames': index}, open(d + '/index.json', 'w'))
        print('rec', name, nfr, 'frames', len(seen), 'distinct')
    elif k.startswith('ssave:'):                 # ssave:NAME / sload:NAME  emulator savestate (scratchpad/NAME.dst)
        h.emu.savestate.save_file(SP + '/' + k[6:] + '.dst')
    elif k.startswith('sload:'):
        h.emu.savestate.load_file(SP + '/' + k[6:] + '.dst'); h.run(1)
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
    elif k.startswith('tdown:'):                  # tdown:x,y  touch and stay pressed, no frames run
        x, y = k[6:].split(','); h.emu.input.touch_set_pos(int(x), int(y))
    elif k == 'rel':
        h.emu.input.touch_release(); h.run(20)
    elif k.startswith('paldump:'):              # paldump:NAME  all 4 palette RAMs (2 KB) -> caps/NAME.pal
        open('%s/%s.pal' % (OUT, k[8:]), 'wb').write(bytes(m[0x05000000 + i] for i in range(0x800)))
    elif k == 'memuse':                           # highest non-zero byte below overlay 129, in 4 KB steps
        ram = bytes(h.emu.memory.read(0x02300000, 0x023D7FFF, 1, False))
        top = max((i for i in range(0, len(ram), 4096) if any(ram[i:i + 4096])), default=0)
        free = 0
        for i in range(len(ram) - 4096, -1, -4096):
            if any(ram[i:i + 4096]): break
            free += 4096
        print('memuse top used block', hex(0x02300000 + top), 'free below 129:', hex(free))
    elif k == 'heaps':                            # gRhDebug.heapMinFree (core, ITCM 0x01FF8000+)
        ram = bytes(h.emu.memory.read(0x01FF8000, 0x01FFFFFF, 1, False)); j = ram.find(bytes([0x42, 0x44, 0x48, 0x52]))
        print('heaps', j, [int.from_bytes(ram[j + 4 * (1 + 3 + i): j + 4 * (2 + 3 + i)], 'little') for i in range(12)] if j >= 0 else None)
    elif k.startswith('vram:'):                  # vram:NAME  display registers, palettes, BG VRAM -> caps/NAME.vram
        rd = lambda a, n: bytes(h.emu.memory.read(a, a + n, 1, False))[:n]
        blob = rd(0x04000000, 0x70) + rd(0x04001000, 0x70) + rd(0x05000000, 0x800) + rd(0x06000000, 0x80000) + rd(0x06200000, 0x20000)
        open('%s/%s.vram' % (OUT, k[5:]), 'wb').write(blob)
    elif k.startswith('waitobj:'):                # waitobj:X,Y  wait (up to 120 frames) until a shown sprite covers
        x, y = map(int, k[8:].split(','))          # screen pixel X,Y (Y 192+: bottom screen): blinking icons, any theme
        base = 0x07000000 if y < 192 else 0x07000400
        yy = y % 192
        sizes = {(0, 0): (8, 8), (0, 1): (16, 16), (0, 2): (32, 32), (0, 3): (64, 64), (1, 0): (16, 8), (1, 1): (32, 8),
                 (1, 2): (32, 16), (1, 3): (64, 32), (2, 0): (8, 16), (2, 1): (8, 32), (2, 2): (16, 32), (2, 3): (32, 64)}
        for _ in range(120):
            o = bytes(h.emu.memory.read(base, base + 0x3FF, 1, False))
            hit = False
            for i in range(128):
                a0, a1 = o[i * 8] | o[i * 8 + 1] << 8, o[i * 8 + 2] | o[i * 8 + 3] << 8
                if a0 & 0x300 == 0x200 or (a0 >> 14) == 3: continue
                w, hh = sizes[(a0 >> 14, a1 >> 14)]
                if a0 & 0x300 == 0x300: w, hh = w * 2, hh * 2      # (affine double size)
                sy, sx = a0 & 255, a1 & 511
                if sy >= 192: sy -= 256
                if sx >= 256: sx -= 512
                if sx <= x < sx + w and sy <= yy < sy + hh:
                    hit = True; break
            if hit: break
            h.run(1)
    elif k == 'nopages':                          # test builds: the AMOLED theme without its screen pages
        ram = bytes(h.emu.memory.read(0x023D8000, 0x023FFFFF, 1, False)); j = ram.find(b'RHSG')
        for i, v in enumerate((0x45474150).to_bytes(4, 'little')):
            h.emu.memory.write_byte(0x023D8000 + j + 16 + i, v)
    elif k.startswith('poke:'):                   # poke:ADDR,HEXBYTES  write bytes into memory (test setups)
        a, _, hx = k[5:].partition(',')
        for i, v in enumerate(bytes.fromhex(hx)):
            h.emu.memory.write_byte(int(a, 16) + i, v)
    elif k.startswith('peek:'):                  # peek:ADDR[,COUNT]  print COUNT bytes (hex) at ADDR
        a, _, c = k[5:].partition(',')
        print('peek', k[5:], bytes(h.emu.memory.read(int(a, 16), int(a, 16) + int(c or '4') - 1, 1, False)).hex())
    elif k.startswith('ramdump:'):              # ramdump:NAME  main RAM (4 MB) -> caps/NAME.ram
        open('%s/%s.ram' % (OUT, k[8:]), 'wb').write(bytes(h.emu.memory.read(0x02000000, 0x023FFFFF, 1, False))[:0x400000])
    elif k == 'sigdbg':
        ram = bytes(h.emu.memory.read(0x023D8000, 0x023FFFFF, 1, False)); j = ram.find(b'RHSG')
        print('sigdbg', [int.from_bytes(ram[j + 4 * i:j + 4 * i + 4], 'little') for i in range(1, 5)])
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
