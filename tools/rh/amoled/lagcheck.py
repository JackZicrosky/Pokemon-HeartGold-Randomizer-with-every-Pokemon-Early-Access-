# lagcheck.py : runs lag.sh output through: the AMOLED walk must reach exactly the Default walk's tiles
import sys, re
lines = [l for l in open(sys.argv[1]).read().splitlines() if l.startswith('pos')]
half = len(lines) // 2
d, a = lines[:half], lines[half:]
print('Default:', ' '.join(l[4:] for l in d)); print('AMOLED: ', ' '.join(l[4:] for l in a))
print('LAG CHECK', 'OK (same walk)' if d == a else 'DIFFERENT (the AMOLED theme slows the game)')
