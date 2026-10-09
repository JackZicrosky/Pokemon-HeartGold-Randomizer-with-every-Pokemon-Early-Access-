#!/usr/bin/env python3
# Romhack helper: reads / writes HeartGold zone event files (a/0/3/2: bg events, object events, warps, coord
# events; layout as pret's files/fielddata/eventdata).
# Usage: python3 tools/rh/zone_events.py MAP_ID...                 prints the maps' events (from base/)
#        python3 tools/rh/zone_events.py --patch ARM9 IN.narc OUT  adds the romhack's NPCs (gen_rh_shop_npcs.py)
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'test'))
from narc import read_narc  # noqa: E402

MAP_HEADERS = 0xF6BE0          # arm9 offset of the map header table (24 bytes per map)
SIZES = (20, 32, 12, 12)       # bg, object, warp, coord event sizes
OBJ_FIELDS = ('id', 'sprite', 'movement', 'type', 'flag', 'script', 'facing', 'sight', 'unk10', 'unk12',
              'xrange', 'yrange', 'x', 'z', 'y')
OBJ_FMT = '<14Hi'


def events_bank(arm9, map_id):
    return struct.unpack('<H', arm9[MAP_HEADERS + map_id * 24 + 0x10:MAP_HEADERS + map_id * 24 + 0x12])[0]


def parse(d):
    out, o = [], 0
    for size in SIZES:
        n = struct.unpack('<I', d[o:o + 4])[0]
        out.append([d[o + 4 + i * size:o + 4 + (i + 1) * size] for i in range(n)])
        o += 4 + n * size
    return out, d[o:]


def build(sections, tail):
    d = b''
    for recs in sections:
        d += struct.pack('<I', len(recs)) + b''.join(recs)
    return d + tail


def obj(rec):
    return dict(zip(OBJ_FIELDS, struct.unpack(OBJ_FMT, rec)))


def pack_obj(o):
    return struct.pack(OBJ_FMT, *[o[f] for f in OBJ_FIELDS])


def write_narc(path, members, btnf):
    data, fat = b'', b''
    for m in members:
        fat += struct.pack('<II', len(data), len(data) + len(m))
        data += m + bytes([0xFF]) * (-len(m) % 4)
    btaf = b'BTAF' + struct.pack('<IHH', 12 + len(fat), len(members), 0) + fat
    gmif = b'GMIF' + struct.pack('<I', 8 + len(data)) + data
    body = btaf + btnf + gmif
    open(path, 'wb').write(b'NARC' + struct.pack('<HHIHH', 0xFFFE, 0x0100, 16 + len(body), 16, 3) + body)


def patch(arm9_path, src, dst):
    import gen_rh_shop_npcs as npcs
    arm9 = open(arm9_path, 'rb').read()
    raw = open(src, 'rb').read()
    off = struct.unpack('<H', raw[12:14])[0]
    off += struct.unpack('<I', raw[off + 4:off + 8])[0]
    btnf = raw[off:off + struct.unpack('<I', raw[off + 4:off + 8])[0]]
    ev = read_narc(src)
    for m, name, x, z in npcs.PLACEMENTS:
        b = events_bank(arm9, m)
        (bgs, objs, warps, coords), tail = parse(ev[b])
        assert all((obj(r)['x'], obj(r)['z']) != (x, z) for r in objs), (m, x, z)
        objs.append(pack_obj(dict(id=len(objs), sprite=npcs.SELLERS[name][1], movement=0, type=0, flag=0,
                                  script=npcs.script_id(name), facing=npcs.FACING_DOWN, sight=0, unk10=0, unk12=0,
                                  xrange=0, yrange=0, x=x, z=z, y=0)))
        ev[b] = build((bgs, objs, warps, coords), tail)
    write_narc(dst, ev, btnf)
    print('zone events: added', len(npcs.PLACEMENTS), 'NPCs')


if __name__ == '__main__' and sys.argv[1] == '--patch':
    patch(*sys.argv[2:5])
elif __name__ == '__main__':
    arm9 = open('base/arm9.bin', 'rb').read()
    ev = read_narc('base/root/a/0/3/2')
    for m in sys.argv[1:]:
        b = events_bank(arm9, int(m))
        (bgs, objs, warps, coords), tail = parse(ev[b])
        print('map', m, 'events', b, 'bg', len(bgs), 'warps', len(warps), 'coords', len(coords), 'tail', len(tail))
        for r in objs:
            print('  ', obj(r))
