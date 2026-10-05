#!/usr/bin/env python3
# Romhack helper: copies one file out of an NDS ROM's file system (e.g. to restore base/root/... from rom.nds).
# Usage: python3 tools/rh/nds_extract.py ROM.nds a/0/2/7 OUT
import struct
import sys


def files(rom):
    fnt_off, fnt_size, fat_off, fat_size = struct.unpack('<IIII', rom[0x40:0x50])

    def walk(dir_id, prefix):
        entry = fnt_off + (dir_id & 0xFFF) * 8
        sub_off, first_id, _ = struct.unpack('<IHH', rom[entry:entry + 8])
        p, fid = fnt_off + sub_off, first_id
        while True:
            n = rom[p]
            p += 1
            if n == 0:
                break
            name = rom[p:p + (n & 0x7F)].decode('ascii')
            p += n & 0x7F
            if n & 0x80:
                sub = struct.unpack('<H', rom[p:p + 2])[0]
                p += 2
                yield from walk(sub, prefix + name + '/')
            else:
                start, end = struct.unpack('<II', rom[fat_off + fid * 8:fat_off + fid * 8 + 8])
                yield prefix + name, start, end
                fid += 1

    yield from walk(0xF000, '')


if __name__ == '__main__':
    rom = open(sys.argv[1], 'rb').read()
    for path, start, end in files(rom):
        if path == sys.argv[2]:
            open(sys.argv[3], 'wb').write(rom[start:end])
            print('wrote', sys.argv[3], end - start, 'bytes')
            break
    else:
        sys.exit('not found: ' + sys.argv[2])
