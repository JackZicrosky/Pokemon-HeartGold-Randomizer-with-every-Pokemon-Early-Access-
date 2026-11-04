# mk_theme_blob.py ELF OUT: data/rh_theme.bin from the AMOLED theme's loadable part (rhtheme/, linked with
# --emit-relocs at RHTB_BASE). The file (include/rh/rh_theme_blob.h): { magic, image size, memory size, relocation
# count }, the image, then the offsets of the words that hold the blob's own addresses (the loader adds where it
# landed). Calls out of the blob must go through addresses (-mlong-calls): a pc-relative call/branch to outside it
# would break when it moves, so those are errors.
import struct, sys

BASE, MAGIC = 0x0A000000, 0x42544852
R_ARM_ABS32, PCREL = 2, {3, 10, 11, 25, 28, 29, 30, 102, 103}   # REL32, THM_CALL, THM_JUMP11, ..., THM_JUMP24, ...

elf = open(sys.argv[1], 'rb').read()
assert elf[:4] == b'\x7fELF' and elf[4] == 1, 'need a 32-bit ELF'
shoff, = struct.unpack_from('<I', elf, 0x20)
shentsize, shnum, shstrndx = struct.unpack_from('<HHH', elf, 0x2E)
secs = []
for i in range(shnum):
    secs.append(struct.unpack_from('<IIIIIIIIII', elf, shoff + i * shentsize))
def name(off):
    st = secs[shstrndx][4]
    return elf[st + off:elf.index(b'\0', st + off)].decode()
byname = {name(s[0]): s for s in secs}
text = byname['.text']
assert text[3] == BASE, 'blob must be linked at RHTB_BASE'
image = bytearray(elf[text[4]:text[4] + text[5]])
bss = byname.get('.bss')
memsize = (bss[3] + bss[5] - BASE) if bss else len(image)
end = BASE + memsize

symtab = byname['.symtab']
def symval(i):
    return struct.unpack_from('<I', elf, symtab[4] + i * 16 + 4)[0]

relocs, bad = [], []
for s in secs:
    if s[1] != 9:                                    # SHT_REL
        continue
    target = secs[s[7]]                              # sh_info: the section it applies to
    if target is not text:
        continue
    for k in range(s[5] // 8):
        off, info = struct.unpack_from('<II', elf, s[4] + k * 8)
        typ, sym = info & 255, info >> 8
        if typ == R_ARM_ABS32:
            v, = struct.unpack_from('<I', image, off - BASE)
            if BASE <= (v & ~1) <= end:
                relocs.append(off - BASE)
        elif typ == 10:                              # THM_CALL: where the BL/BLX really goes (the linker may
            h1, h2 = struct.unpack_from('<HH', image, off - BASE)     # have put a veneer in between)
            o = ((h1 & 0x7FF) << 12) | ((h2 & 0x7FF) << 1)
            o -= (1 << 23) if o & (1 << 22) else 0
            v = off + 4 + o
            if (h2 >> 11) == 0b11101:
                v &= ~3
            if not (BASE <= v < end):
                bad.append((hex(off), typ, hex(v)))
        elif typ in PCREL:
            v = symval(sym)
            if not (BASE <= (v & ~1) < end):
                bad.append((hex(off), typ, hex(v)))
if bad:
    sys.exit('pc-relative references out of the blob (use long calls): %s' % bad[:10])
relocs = sorted(set(relocs))
with open(sys.argv[2], 'wb') as f:
    f.write(struct.pack('<IIII', MAGIC, len(image), memsize, len(relocs)))
    f.write(image)
    f.write(b''.join(struct.pack('<I', r) for r in relocs))
print('%s: AMOLED theme code %d bytes (+%d zeroed), %d relocations' % (sys.argv[2], len(image), memsize - len(image), len(relocs)))
