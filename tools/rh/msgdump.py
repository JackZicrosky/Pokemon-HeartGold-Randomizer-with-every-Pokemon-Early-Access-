#!/usr/bin/env python3
# Romhack helper: prints a text bank of the unpacked ROM (base/root/a/0/2/7) in hg-engine's data/text format.
# Usage: python3 tools/rh/msgdump.py BANK   (from the repository root)
import struct
import sys

sys.path.insert(0, 'tools/rh/test')
from narc import read_narc  # noqa: E402


def charmap():
    m = {}
    for line in open('charmap.txt', encoding='utf-8'):
        line = line.rstrip('\n')
        if '=' in line and not line.startswith('//'):
            k, v = line.split('=', 1)
            try:
                m[int(k, 16)] = v
            except ValueError:
                pass
    return m


def decode(data):
    count, seed = struct.unpack('<HH', data[:4])
    key = (seed * 0x2FD) & 0xFFFF
    cm = charmap()
    out = []
    for i in range(count):
        lk = (key * (i + 1)) & 0xFFFF
        off, ln = struct.unpack('<II', data[4 + 8 * i:12 + 8 * i])
        off ^= lk | (lk << 16)
        ln ^= lk | (lk << 16)
        k2 = (0x91BD3 * (i + 1)) & 0xFFFF
        s = []
        for j in range(ln):
            c = struct.unpack('<H', data[off + 2 * j:off + 2 * j + 2])[0] ^ k2
            k2 = (k2 + 0x493D) & 0xFFFF
            if c == 0xFFFF:
                break
            if c == 0xE000:
                s.append('\\n')
            elif c == 0x25BC:
                s.append('\\r')
            elif c == 0x25BD:
                s.append('\\f')
            elif c == 0xFFFE:
                s.append('{VAR}')
            else:
                s.append(cm.get(c, '{%04X}' % c))
        out.append(''.join(s))
    return out


if __name__ == '__main__':
    for bank in [int(b) for b in sys.argv[1:]]:
        print("== bank", bank)
        for i, line in enumerate(decode(read_narc('base/root/a/0/2/7')[bank])):
            print(i, line)
