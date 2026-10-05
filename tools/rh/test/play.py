#!/usr/bin/env python3
# Romhack test driver: runs a list of steps on a ROM and reports the player's position.
# Usage (WSL): SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy /root/dsvenv/bin/python play.py ROM STEPS [OUTDIR] [SAVE]
# STEPS: a file with whitespace-separated steps (lines starting with # are comments):
#   U16 D16 L16 R16   hold a direction for N frames      A / B / S / s / X / Y / l / r   press a button (4 frames)
#   A10               hold A for 10 frames                W60   wait 60 frames
#   T123,73           touch the bottom screen             shot:name   screenshot to OUTDIR/name.png
#   mash:N            press A N times, 25 frames apart    bmash:N     same with B
#   poke8:ADDR:V poke16:ADDR:V poke32:ADDR:V   write memory      rh:FIELD_OFFSET:V   write a byte of the RhSettings
#   save:FILE         export the cartridge save          state:FILE  savestate (only valid for this build)
#   pos               print the map / coordinates
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dsharness import Harness  # noqa: E402

FIELD_SYS_PTR = 0x023DE0F4
RH_MAGIC = 0x52480001


def player(m):
    """(mapId, x, z) of the player, or None outside the field."""
    fs = m.read_long(FIELD_SYS_PTR)
    if not fs or not (0x02000000 <= fs < 0x02400000):
        return None
    loc, av = m.read_long(fs + 0x20), m.read_long(fs + 0x40)
    if not (0x02000000 <= loc < 0x02400000 and 0x02000000 <= av < 0x02400000):
        return None
    mo = m.read_long(av + 0x30)
    if not (0x02000000 <= mo < 0x02400000):
        return None
    return (m.read_long(loc), m.read_long(mo + 0x64), m.read_long(mo + 0x6C))


def go(h, m, tx, tz, budget=200):
    """Walk to tile (tx, tz) on the current map (simple greedy steps, sidestepping when blocked)."""
    stuck = 0
    for _ in range(budget):
        p = player(m)
        if p is None:
            h.run(10)
            continue
        _, x, z = p
        if (x, z) == (tx, tz):
            return True
        moves = []
        if x != tx:
            moves.append('R' if tx > x else 'L')
        if z != tz:
            moves.append('D' if tz > z else 'U')
        if stuck and stuck % 3 == 0:
            h.hold('B', 4)                       # maybe a message is open: advance it
            h.run(20)
        if stuck:
            moves = moves[::-1] + ['U', 'D', 'L', 'R'][stuck % 4:stuck % 4 + 1]
        h.hold(moves[0], 16)
        h.run(4)
        q = player(m)
        stuck = stuck + 1 if q and q[1:] == (x, z) else 0
    print('go: gave up at', player(m), 'target', (tx, tz))
    return False


def find_rh_settings(m):
    # the settings live in the save's misc block; find their magic in main RAM
    ram = bytes(m.read(0x02000000, 0x023FFFFF, 1, False)) if hasattr(m, 'read') else None
    key = RH_MAGIC.to_bytes(4, 'little')
    i = ram.find(key)
    while i >= 0 and i % 4:
        i = ram.find(key, i + 1)
    return 0x02000000 + i if i >= 0 else None


def main():
    rom, steps = sys.argv[1], sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else '.'
    save = sys.argv[4] if len(sys.argv) > 4 else None
    h = Harness(rom, save)
    m = h.emu.memory.unsigned
    rh = None
    toks = []
    for line in open(steps):
        if line.strip().startswith('#'):
            continue
        toks += line.split()
    for t in toks:
        if t.startswith('shot:'):
            h.emu.screenshot().save(os.path.join(out, t[5:] + '.png'))
        elif t.startswith('save:'):
            h.emu.backup.export_file(t[5:])
        elif t.startswith('state:'):
            h.emu.savestate.save_file(t[6:])
        elif t.startswith('loadstate:'):
            h.emu.savestate.load_file(t[10:])
        elif t.startswith('mash:') or t.startswith('bmash:'):
            k = 'A' if t[0] == 'm' else 'B'
            for _ in range(int(t.split(':')[1])):
                h.hold(k, 4)
                h.run(25)
        elif t.startswith('poke'):
            kind, addr, v = t.split(':')
            addr, v = int(addr, 0), int(v, 0)
            {"poke8": h.emu.memory.write_byte, "poke16": h.emu.memory.write_short, "poke32": h.emu.memory.write_long}[kind](addr, v)
        elif t.startswith('rh:'):
            _, off, v = t.split(':')
            if rh is None:
                rh = find_rh_settings(h.emu.memory)
                print('RhSettings at', hex(rh) if rh else None)
            if rh:
                h.emu.memory.write_byte(rh + int(off, 0), int(v, 0))
        elif t.startswith('findenc:'):
            # dump the loaded wild table: findenc:RATEBYTES (hex, e.g. 000f00193248 for New Bark)
            pat = bytes.fromhex(t[8:])
            ram = bytes(h.emu.memory.read(0x02000000, 0x023FFFFF, 1, False))
            for a in [0x02000000 + i for i in range(0, len(ram) - 8) if ram[i:i + len(pat)] == pat]:
                    land = [m.read_short(a + 20 + 2 * i) for i in range(36)]
                    surf = [m.read_short(a + 100 + 4 * i + 2) for i in range(5)]
                    fish = [m.read_short(a + 128 + 4 * i + 2) for i in range(15)]
                    print('enc @', hex(a), 'land', land[:12], 'surf', surf, 'fish', fish)
        elif t == 'pos':
            print('pos', player(m), '@', h.frame)
        elif t.startswith('go:'):
            x, z = t[3:].split(',')
            go(h, m, int(x), int(z))
        elif t.startswith('waitmap:'):
            # press A (or the given key) until the player is on map N: waitmap:N[:KEY[:MAXPRESSES]]
            parts = t.split(':')
            want, key = int(parts[1]), (parts[2] if len(parts) > 2 else 'A')
            for _ in range(int(parts[3]) if len(parts) > 3 else 200):
                p = player(m)
                if p and p[0] == want:
                    break
                h.hold(key, 4)
                h.run(25)
            print('waitmap', want, '->', player(m), '@', h.frame)
        elif t[0] == 'W':
            h.run(int(t[1:]))
        elif t[0] == 'T':
            x, y = t[1:].split(',')
            h.command(['touch', x, y, '10'])
            h.run(20)
        else:
            k, n = t[0], int(t[1:]) if len(t) > 1 else 4
            h.hold(k, n)
            h.run(8)
    print('done @ frame', h.frame)


if __name__ == '__main__':
    main()
