#!/usr/bin/env python3
# Test helper: moves the player of a HeartGold save to another map / tile, so a test can start anywhere.
# Use indoor maps (floors at height 0): the saved height is not recomputed, so an outdoor tile at another height
# leaves the player stuck under the ground. Walk out of the building from there.
# Usage: savwarp.py IN.sav OUT.sav MAP X Z
# Changes the current Location (map, warp -1, x, z) and the player's saved map object (id 0xFF: initial / current
# x and z), then recomputes the general block's CRC-16 (CCITT, init 0xFFFF).
import struct
import sys

MAGIC = 0x20060623


def crc16(data):
    c = 0xFFFF
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else (c << 1)
            c &= 0xFFFF
    return c


def blocks(d):
    i = 0
    while True:
        i = d.find(struct.pack('<I', MAGIC), i)
        if i < 0:
            return
        f = i - 8
        _, size, _, bid, _ = struct.unpack('<IIIHH', d[f:f + 16])
        yield f, f + 16 - size, bid
        i += 4


def main():
    src, dst, mapid, x, z = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
    d = bytearray(open(src, 'rb').read())
    newest = None
    for footer, start, bid in blocks(d):
        if bid == 0:
            cnt = struct.unpack('<I', d[footer:footer + 4])[0]
            if newest is None or cnt > newest[0]:
                newest = (cnt, footer, start)
    _, footer, start = newest
    old = None
    for o in range(start, footer - 20, 4):
        m, w, px, pz, dr = struct.unpack('<IiiiI', d[o:o + 20])
        if w == -1 and 0 < m < 600 and 0 <= px < 2048 and 0 <= pz < 2048 and dr < 4:
            print('location at', hex(o), (m, px, pz), '->', (mapid, x, z))
            d[o:o + 16] = struct.pack('<Iiii', mapid, -1, x, z)
            old = (px, pz)
            break
    for o in range(start, footer - 0x50, 2):
        if old and d[o + 8] == 0xFF and struct.unpack('<HH', d[o + 0x26:o + 0x28] + d[o + 0x2A:o + 0x2C]) == old:
            print('player object at', hex(o))
            d[o + 0x20:o + 0x22] = d[o + 0x26:o + 0x28] = struct.pack('<H', x)
            d[o + 0x24:o + 0x26] = d[o + 0x2A:o + 0x2C] = struct.pack('<H', z)
            # height: initial / current y (tiles) and the position vector's y (fx32) = the floor of an indoor map
            print('old height', struct.unpack('<hhi', d[o + 0x22:o + 0x24] + d[o + 0x28:o + 0x2A] + d[o + 0x2C:o + 0x30]))
            d[o + 0x22:o + 0x24] = d[o + 0x28:o + 0x2A] = bytes(2)
            d[o + 0x2C:o + 0x30] = bytes(4)
            break
    d[footer + 14:footer + 16] = struct.pack('<H', crc16(d[start:footer]))
    open(dst, 'wb').write(d)
    print('wrote', dst)


if __name__ == '__main__':
    main()
