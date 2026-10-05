#!/usr/bin/env python3
# Headless DS test harness (py-desmume), the DS counterpart of the FireRed mGBA harness.
# Usage (WSL):  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy /root/dsvenv/bin/python dsharness.py ROM SCRIPT [SAVE.sav]
#
# Script commands (one per line, '#' starts a comment):
#   wait N                hold nothing for N frames
#   press KEYS N          hold KEYS for N frames, then release and wait 6 frames
#   repeat KEYS COUNT     press KEYS 4 frames, COUNT times (12 frames apart)
#   touch X Y N           touch the bottom screen at X,Y for N frames, then release
#   shot FILE.png         save both screens (256x384, top screen first)
#   savestate F / loadstate F
#   peek32 ADDR / peek16 ADDR / peek8 ADDR
#   dump ADDR LEN FILE    write LEN bytes of memory to FILE ("*ADDR" = read a pointer at ADDR first)
#   dumpstr ADDR LEN      print memory as ASCII
#   writesave FILE        export the cartridge save
#   until32 ADDR VALUE MAXFRAMES   run until the 32-bit word at ADDR == VALUE
# Keys: A B X Y S=START s=SELECT U D L R (D-pad) l r (shoulders)
import sys

from desmume.emulator import DeSmuME
from desmume.controls import Keys, keymask

KEYMAP = {
    'A': Keys.KEY_A, 'B': Keys.KEY_B, 'X': Keys.KEY_X, 'Y': Keys.KEY_Y,
    'S': Keys.KEY_START, 's': Keys.KEY_SELECT,
    'U': Keys.KEY_UP, 'D': Keys.KEY_DOWN, 'L': Keys.KEY_LEFT, 'R': Keys.KEY_RIGHT,
    'l': Keys.KEY_L, 'r': Keys.KEY_R,
}


def num(s):
    return int(s, 0)


class Harness:
    def __init__(self, rom, save=None):
        self.emu = DeSmuME()
        self.emu.open(rom)
        if save:
            import os
            if os.path.exists(save):
                self.emu.backup.import_file(save)
        self.frame = 0

    def run(self, n):
        for _ in range(n):
            self.emu.cycle(with_joystick=False)
            self.frame += 1

    def hold(self, keys, n):
        for k in keys:
            self.emu.input.keypad_add_key(keymask(KEYMAP[k]))
        self.run(n)
        for k in keys:
            self.emu.input.keypad_rm_key(keymask(KEYMAP[k]))

    def addr(self, s):
        if s.startswith('*'):
            return self.emu.memory.unsigned.read_long(num(s[1:]))
        return num(s)

    def command(self, parts):
        c = parts[0]
        m = self.emu.memory.unsigned
        if c == 'wait':
            self.run(num(parts[1]))
        elif c == 'press':
            self.hold(parts[1], num(parts[2]) if len(parts) > 2 else 4)
            self.run(6)
        elif c == 'repeat':
            for _ in range(num(parts[2])):
                self.hold(parts[1], 4)
                self.run(8)
        elif c == 'touch':
            self.emu.input.touch_set_pos(num(parts[1]), num(parts[2]))
            self.run(num(parts[3]) if len(parts) > 3 else 4)
            self.emu.input.touch_release()
            self.run(6)
        elif c == 'shot':
            self.emu.screenshot().save(parts[1])
            print(f'shot {parts[1]} @ frame {self.frame}')
        elif c == 'savestate':
            self.emu.savestate.save_file(parts[1])
        elif c == 'loadstate':
            self.emu.savestate.load_file(parts[1])
        elif c == 'peek32':
            print(f'{parts[1]} = 0x{m.read_long(self.addr(parts[1])):08x}')
        elif c == 'peek16':
            print(f'{parts[1]} = 0x{m.read_short(self.addr(parts[1])):04x}')
        elif c == 'peek8':
            print(f'{parts[1]} = 0x{m.read_byte(self.addr(parts[1])):02x}')
        elif c == 'dump':
            a = self.addr(parts[1])
            data = bytes(m.read_byte(a + i) for i in range(num(parts[2])))
            open(parts[3], 'wb').write(data)
        elif c == 'dumpstr':
            a = self.addr(parts[1])
            data = bytes(m.read_byte(a + i) for i in range(num(parts[2])))
            print(data.split(b'\0')[0].decode('ascii', 'replace'))
        elif c == 'writesave':
            self.emu.backup.export_file(parts[1])
        elif c == 'until32':
            a, v, mx = self.addr(parts[1]), num(parts[2]), num(parts[3])
            start = self.frame
            while m.read_long(a) != v and self.frame - start < mx:
                self.run(60)
            print(f'until32 {parts[1]}: {"ok" if m.read_long(a) == v else "TIMEOUT"} after {self.frame - start} frames')
        else:
            raise SystemExit(f'unknown command: {c}')


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    h = Harness(sys.argv[1], sys.argv[3] if len(sys.argv) > 3 else None)
    for line in open(sys.argv[2]):
        line = line.split('#')[0].strip()
        if line:
            h.command(line.split())
    print(f'done @ frame {h.frame}')


if __name__ == '__main__':
    main()
