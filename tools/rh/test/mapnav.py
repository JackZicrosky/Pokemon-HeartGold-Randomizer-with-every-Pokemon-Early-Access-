#!/usr/bin/env python3
# Test helper: walkable-tile map of HeartGold maps from the unpacked ROM (base/), and a path finder for play.py.
# Map headers (arm9) give each map's matrix; the matrix gives the land data (32x32 chunks) of each tile, whose
# permissions hold the collision bit (0x8000) and the tile behaviour (low byte).
import os
import struct
from collections import deque

from narc import read_narc

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
MAP_HEADERS = 0xF6BE0          # arm9 offset of the map header table (24 bytes per map)
MAP_HEADER_SIZE = 24

JUMP = {0x38: (1, 0), 0x39: (-1, 0), 0x3A: (0, -1), 0x3B: (0, 1)}       # ledges: the direction you can hop
WATER = set(range(0x10, 0x17)) | {0x18, 0x19, 0x1A, 0x13}               # needs Surf / Waterfall
WARPS = set(range(0x5E, 0x70))                                          # stairs, entrances, doors, warp tiles


class MapData:
    def __init__(self, root=ROOT):
        base = os.path.join(root, 'base')
        self.arm9 = open(os.path.join(base, 'arm9.bin'), 'rb').read()
        self.matrices = read_narc(os.path.join(base, 'root', 'a', '0', '4', '1'))
        self.lands = read_narc(os.path.join(base, 'root', 'a', '0', '6', '5'))
        self.cache = {}

    def matrix_id(self, map_id):
        o = MAP_HEADERS + map_id * MAP_HEADER_SIZE + 4
        return struct.unpack('<H', self.arm9[o:o + 2])[0]

    def matrix(self, mid):
        if mid not in self.cache:
            d = self.matrices[mid]
            w, h, has_headers, has_alt, name_len = d[0], d[1], d[2], d[3], d[4]
            o = 5 + name_len
            headers = None
            if has_headers:
                headers = struct.unpack('<%dH' % (w * h), d[o:o + 2 * w * h])
                o += 2 * w * h
            if has_alt:
                o += w * h
            lands = struct.unpack('<%dH' % (w * h), d[o:o + 2 * w * h])
            self.cache[mid] = (w, h, headers, lands)
        return self.cache[mid]

    def perm(self, map_id, x, z):
        """The tile's permission word, or None outside the map."""
        w, h, _, lands = self.matrix(self.matrix_id(map_id))
        cx, cz = x // 32, z // 32
        if x < 0 or z < 0 or cx >= w or cz >= h:
            return None
        land = lands[cz * w + cx]
        if land == 0xFFFF or land >= len(self.lands):
            return None
        d = self.lands[land]
        # header: permissions / objects / model / BDHC sizes, then HGSS's 0x1234 + background sound section size
        o = 0x14 + struct.unpack('<H', d[0x12:0x14])[0] + ((z % 32) * 32 + (x % 32)) * 2
        return struct.unpack('<H', d[o:o + 2])[0]

    def walkable(self, map_id, x, z, target=False):
        p = self.perm(map_id, x, z)
        if p is None or p & 0x8000:
            return False
        b = p & 0xFF
        if b in WATER or b in JUMP:
            return False
        if b in WARPS and not target:
            return False
        return True

    def path(self, map_id, start, goal, blocked=()):
        """Directions ('U','D','L','R') from start to goal (ledges hopped), or None."""
        dirs = {'L': (-1, 0), 'R': (1, 0), 'U': (0, -1), 'D': (0, 1)}
        prev = {start: None}
        q = deque([start])
        while q:
            cur = q.popleft()
            if cur == goal:
                break
            for k, (dx, dz) in dirs.items():
                nxt = (cur[0] + dx, cur[1] + dz)
                p = self.perm(map_id, *nxt)
                if p is None:
                    continue
                if (p & 0xFF) in JUMP and JUMP[p & 0xFF] == (dx, dz):
                    land = (nxt[0] + dx, nxt[1] + dz)                # hop over the ledge
                    if land not in prev and land not in blocked and self.walkable(map_id, *land, target=land == goal):
                        prev[land] = (cur, k)
                        q.append(land)
                    continue
                if nxt in prev or nxt in blocked or not self.walkable(map_id, *nxt, target=nxt == goal):
                    continue
                prev[nxt] = (cur, k)
                q.append(nxt)
        if goal not in prev:
            return None
        out = []
        cur = goal
        while prev[cur] is not None:
            cur, k = prev[cur]
            out.append(k)
        return out[::-1]


if __name__ == '__main__':
    import sys
    md = MapData()
    m, x0, z0, x1, z1 = [int(v) for v in sys.argv[1:6]]
    print(''.join(md.path(m, (x0, z0), (x1, z1)) or ['-']))
