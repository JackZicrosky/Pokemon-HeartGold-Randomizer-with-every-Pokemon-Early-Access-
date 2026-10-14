# test only: compare AMOLED captures (caps/am_*) with the owner's designs; writes vs/NAME.png (owner | game | diff)
# Work folder (captures in WORK/caps, test saves in WORK): $RH_AMOLED_WORK, default: this folder.
import glob, os
from PIL import Image, ImageChops
D = os.environ.get('RH_UI_DEFAULT', '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots') + '/'
A = os.environ.get('RH_UI_AMOLED', '/mnt/d/AI shit/Claude/Pokemon Romhack Stuff/HeartGold/UI screenshots - AMOLED') + '/'
SP = os.environ.get('RH_AMOLED_WORK', os.path.dirname(os.path.abspath(__file__)))
C = SP + '/caps/'
os.makedirs(SP + '/vs', exist_ok=True)
def o1(v): return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))
def q5(im): return im.point(lambda v: o1(v) * 8)
defs = {os.path.basename(f)[4:-4]: Image.open(f).convert('RGB') for f in glob.glob(C + 'def_*.png')
        if os.path.exists(C + 'am_' + os.path.basename(f)[4:-4] + '.png')}     # only captures with an AMOLED twin
for f in sorted(glob.glob(A + '[0-9]*.png')):
    name = os.path.basename(f)
    if name[:2] in ('01', '14') or not os.path.exists(D + name):
        continue
    d = Image.open(D + name).convert('RGB')
    best, bn = None, 10**9
    for k, im in defs.items():
        diff = 98304 - ImageChops.difference(d, im).convert('L').histogram()[0]
        if diff < bn: best, bn = k, diff
    if not os.path.exists(C + 'am_' + best + '.png'):
        print('%-38s no AMOLED capture (%s)' % (name, best)); continue
    o = q5(Image.open(f).convert('RGB')); g = Image.open(C + 'am_' + best + '.png').convert('RGB')
    dm = ImageChops.difference(o, g).convert('L').point(lambda v: 255 if v else 0)
    bad = dm.histogram()[255]
    # pixels that also differ between the Default screenshot and the capture (animation, different frame) don't count
    frame = ImageChops.difference(d, defs[best]).convert('L').point(lambda v: 255 if v else 0)
    real = ImageChops.multiply(dm, ImageChops.invert(frame)).histogram()[255]
    print('%-38s <- %-14s mismatched %5d (%4.1f%%), not frame-related %5d' % (name, best, bad, bad / 983.04, real))
    sheet = Image.new('RGB', (768, 384)); sheet.paste(o, (0, 0)); sheet.paste(g, (256, 0)); sheet.paste(dm.convert('RGB'), (512, 0))
    sheet.save(SP + '/vs/' + name)
