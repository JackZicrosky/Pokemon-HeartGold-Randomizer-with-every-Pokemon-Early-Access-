# gsearch.py: find an RNG seed (poked just before the encounter) that makes each owner battle's wild Pokemon female
import subprocess, sys
from PIL import Image
P = '/root/dsvenv/bin/python'
BATTLES = {   # name: the pokes of that battle in caps_all.sh (species / level tables, potions, owner's Chikorita)
    'r3': 'poke:022ADC70,030303030303030303030303 poke:022ADCAC,130013001300130013001300130013001300130013001300',
    's2': 'poke:022ADC70,020202020202020202020202 poke:022ADCAC,a700a700a700a700a700a700a700a700a700a700a700a700',
    'h4': 'poke:022ADC70,040404040404040404040404 poke:022ADCAC,a300a300a300a300a300a300a300a300a300a300a300a300',
}
def gender(png):
    im = Image.open(png).convert('RGB'); s = set()
    for y in range(20, 40):
        for x in range(50, 100):
            p = tuple(v >> 3 for v in im.getpixel((x, y)))
            if p == (31, 11, 5): s.add('F')
            if p == (6, 12, 27): s.add('M')
    return ''.join(sorted(s))
for name in sys.argv[1:]:
    for seed in range(1, 40):
        sh = '%08x' % (seed * 0x9E3779B1 & 0xFFFFFFFF)
        le = bytes.fromhex(sh)[::-1].hex()
        cmd = ('%s cap.py def r30.sav W60 %s poke:0227CF72,0400 setmon:10,10,14,11,13,14 W21 poke:021D15A8,%s battle W600 cap:gs_%s'
               % (P, BATTLES[name], le, name)).split()
        subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
        g = gender('caps/def_gs_%s.png' % name)
        print(name, le, g, flush=True)
        if g == 'F': break
