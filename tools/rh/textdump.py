#!/usr/bin/env python3
# Romhack: writes a text bank of the unpacked ROM (base/root/a/0/2/7) in hg-engine's data/text format (via
# tools/msgenc), as the starting point for an edited bank.
# Usage: python3 tools/rh/textdump.py BANK [OUT.txt]   (from the repository root; default OUT = build/text_BANK.txt)
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, 'tools/rh/test')
from narc import read_narc  # noqa: E402

if __name__ == '__main__':
    bank = int(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else 'build/text_%d.txt' % bank
    data = read_narc('base/root/a/0/2/7')[bank]
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, '7_%03d' % bank)
        open(src, 'wb').write(data)
        subprocess.run(['tools/msgenc', '-d', '-c', 'charmap.txt', src, out], check=True, stdout=subprocess.DEVNULL)
    print('wrote', out)
