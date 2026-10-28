# encsearch.py : which start delays give the wild battles of the owner's battle pictures (Route 30, Saturday 03:23).
# Reads caps/def_srch_K.png (cap.py: r30.sav, theme, WK, battle, W700, cap:srch_K) and compares the enemy HP box
# (name, gender, level) with the owner's Default pictures pixel for pixel.
import glob, re
from PIL import Image
D = 'D:/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots/'
BOX = (0, 18, 120, 36)
REF = {'rattata_f3': '60_battle_command_menu.png', 'rattata_m4': '61_battle_moves.png', 'hoothoot_m4': '66_battle_command_menu.png', 'spinarak_f2': '68_battle_bag_pressed.png'}
refs = {k: Image.open(D + v).convert('RGB').crop(BOX) for k, v in REF.items()}
def diff(a, b):
    return sum(1 for p, q in zip(a.getdata(), b.getdata()) if p != q)
for f in sorted(glob.glob('caps/def_srch*.png'), key=lambda s: int(re.findall(r'\d+', s)[-1])):
    im = Image.open(f).convert('RGB').crop(BOX)
    best = min((diff(im, r), k) for k, r in refs.items())
    print('%-26s best %-12s %4d px differ%s' % (f, best[1], best[0], '   <== MATCH' if best[0] == 0 else ''))
