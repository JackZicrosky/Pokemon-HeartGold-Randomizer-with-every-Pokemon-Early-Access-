# bufdump.py CAP [ID] : the theme's tracked palette buffers (unfaded/faded) next to palette RAM, from caps/CAP.ram and
# caps/am_CAP.vram2 (addresses from the current build's linked.o: nm.sh)
import struct, sys, subprocess
n = sys.argv[1]; want = int(sys.argv[2]) if len(sys.argv) > 2 else 1
A = {'sBufs': 0x023df5c8, 'sIndexCount': 0x023df5ac, 'sIndexUndo': 0x023df5a8, 'sLastSig': 0x023df5a4}
def c(v): return (v & 31, v >> 5 & 31, v >> 10 & 31)
r = open('caps/%s.ram' % n, 'rb').read(); v = open('caps/am_%s.vram2' % n, 'rb').read()
u = lambda a: struct.unpack_from('<I', r, a - 0x02000000)[0]
base = {0: 0, 1: 0x400, 2: 0x200, 3: 0x600}[want]
print('lastsig', u(A['sLastSig']), 'index patches', u(A['sIndexCount']))
for i in range(24):
    a = A['sBufs'] + i * 20; unf, fad, size, id_, pd = [u(a + 4 * j) for j in range(5)]
    if unf and id_ == want:
        U = [struct.unpack_from('<H', r, unf - 0x02000000 + k * 2)[0] for k in range(128)]
        F = [struct.unpack_from('<H', r, fad - 0x02000000 + k * 2)[0] for k in range(128)]
        P = [struct.unpack_from('<H', v, 0xE0 + base + k * 2)[0] for k in range(128)]
        for row in range(8):
            print('row', row, 'unf', [c(x) for x in U[row * 16:row * 16 + 5]], 'fad', [c(x) for x in F[row * 16:row * 16 + 5]],
                  'ram', [c(x) for x in P[row * 16:row * 16 + 5]])
