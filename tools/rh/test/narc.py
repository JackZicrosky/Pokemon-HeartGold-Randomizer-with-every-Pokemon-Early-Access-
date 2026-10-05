#!/usr/bin/env python3
# Test helper: reads the members of a NARC archive (Nintendo DS) from the unpacked ROM in base/root.
import struct


def read_narc(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'NARC', path
    off = struct.unpack('<H', d[12:14])[0]
    # BTAF (file allocation table)
    assert d[off:off + 4] == b'BTAF'
    btaf_size, count = struct.unpack('<IH', d[off + 4:off + 10])
    entries = [struct.unpack('<II', d[off + 12 + 8 * i:off + 20 + 8 * i]) for i in range(count)]
    off += btaf_size
    assert d[off:off + 4] == b'BTNF'
    off += struct.unpack('<I', d[off + 4:off + 8])[0]
    assert d[off:off + 4] == b'GMIF'
    base = off + 8
    return [d[base + s:base + e] for s, e in entries]
