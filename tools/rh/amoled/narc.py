# test only: read a NARC's members; render NCLR palettes as rows of swatches
import os, struct, sys
from PIL import Image, ImageDraw
ROOT = '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/source/base/root/'
def members(path):
    d = open(path, 'rb').read()
    o = struct.unpack_from('<H', d, 12)[0]          # header size
    n = struct.unpack_from('<I', d, o + 8)[0]
    fat = [struct.unpack_from('<II', d, o + 12 + 8 * i) for i in range(n)]
    o += struct.unpack_from('<I', d, o + 4)[0]
    o += struct.unpack_from('<I', d, o + 4)[0]       # FNTB
    base = o + 8
    out = []
    for s, e in fat:
        b = d[base + s: base + e]
        if b[:1] == b'\x10':                         # LZ77
            b = lz(b)
        out.append(b)
    return out
def lz(b):
    size = struct.unpack_from('<I', b, 0)[0] >> 8; out = bytearray(); i = 4
    while len(out) < size:
        f = b[i]; i += 1
        for k in range(8):
            if len(out) >= size: break
            if f & 0x80:
                x = b[i] << 8 | b[i+1]; i += 2; ln = (x >> 12) + 3; dist = (x & 0xFFF) + 1
                for _ in range(ln): out.append(out[-dist])
            else:
                out.append(b[i]); i += 1
            f <<= 1
    return bytes(out)
def colors(b):
    sz = struct.unpack_from('<I', b, 0x20)[0]; st = 0x18 + struct.unpack_from('<I', b, 0x24)[0]
    sz = min(sz, len(b) - st)
    return [struct.unpack_from('<H', b, st + 2 * i)[0] for i in range(sz // 2)]
if __name__ == '__main__':
    narc = int(sys.argv[1]); ms = members(ROOT + 'a/%d/%d/%d' % (narc // 100, narc // 10 % 10, narc % 10))
    rows = []
    for i, b in enumerate(ms):
        if b[:4] == b'RLCN':
            c = colors(b)
            for r in range(min(len(c) // 16, 4)):
                rows.append((i, r, c[16 * r:16 * r + 16]))
    S = 6; im = Image.new('RGB', (40 + 16 * S, max(1, len(rows)) * S), (255, 0, 255)); dr = ImageDraw.Draw(im)
    for y, (i, r, c) in enumerate(rows):
        if r == 0: dr.text((0, y * S - 3), str(i), fill=(255, 255, 255))
        for x, v in enumerate(c):
            dr.rectangle([40 + x * S, y * S, 40 + x * S + S - 1, y * S + S - 1], fill=((v & 31) * 8, (v >> 5 & 31) * 8, (v >> 10 & 31) * 8))
    im.save(sys.argv[2])
    print(len(ms), 'members', len(rows), 'rows')
