#!/usr/bin/env python3
# Test helper: byte offsets of the RhSettings fields (include/rh/rh_settings.h), for play.py's rh:OFFSET:V.
# Usage: python3 tools/rh/test/rh_offsets.py [FIELD...]
import re
import sys

SIZES = {'u8': 1, 'u16': 2, 'u32': 4, 's8': 1, 's16': 2, 's32': 4}


def offsets():
    src = open('include/rh/rh_settings.h').read()
    body = src[src.index('struct RhSettings'):]
    body = body[body.index('{') + 1:body.index('};')]
    body = re.sub(r'//[^\n]*', '', body)
    out, off = {}, 0
    for decl in body.split(';'):
        decl = decl.strip()
        m = re.match(r'([us](?:8|16|32))\s+(.*)', decl, re.S)
        if not m:
            continue
        size = SIZES[m.group(1)]
        for name in m.group(2).split(','):
            name = name.strip()
            arr = re.match(r'(\w+)\[(.+)\]', name)
            count = 1
            if arr:
                name, n = arr.group(1), arr.group(2).replace('RH_SEED_TEXT_LENGTH', '10')
                count = eval(n)
            off = (off + size - 1) // size * size
            out[name] = off
            off += size * count
    return out


if __name__ == '__main__':
    o = offsets()
    for f in sys.argv[1:] or o:
        print(f, o[f])
