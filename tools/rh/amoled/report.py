# report.py OUTDIR : the owner's frame report from rec/ (every menu state, idle and after every input, at every time of
# day, Default and AMOLED). Per state: the inputs grouped by what they do (the same frames in both modes), and for each
# group a sheet of its distinct moments: Default on top, AMOLED below, the frame numbers (60 per second) under it.
# Flags: AMOLED shows frames Default doesn't have (extra in-between frames / flicker), and how far each AMOLED
# moment is from the closest of the owner's pictures (reports/report_TIME_PART.json, analyze.py).
import json, os, sys, glob, hashlib, collections, html
from PIL import Image, ImageDraw
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
TIMES = ['morning', 'day', 'evening', 'night']
COLS = 8


def frames(mode, name):
    d = 'rec/%s_%s' % (mode, name)
    if not os.path.exists(d + '/index.json'):
        return None
    return d, json.load(open(d + '/index.json'))['frames']


def digest(d, f, cache={}):
    k = d + '/' + f
    if k not in cache:
        cache[k] = hashlib.md5(open(k, 'rb').read()).hexdigest()
    return cache[k]


def moments(df, af):
    """frame ranges where neither mode changes: [(first, last)]"""
    (dd, d), (ad, a) = df, af
    r = []
    for i in range(len(a)):
        key = (d[i], a[i])
        if r and r[-1][2] == key:
            r[-1][1] = i
        else:
            r.append([i, i, key])
    return [(x, y) for x, y, _ in r]


refs = {}
for p in glob.glob('reports/report_*.json'):
    for k, v in json.load(open(p)).items():
        refs[k] = {tuple(r['frames']): r for r in v['runs']}

page = ['<!doctype html><meta charset="utf-8"><title>AMOLED frame report</title><style>body{background:#000;color:#fff;'
        'font:14px sans-serif;margin:16px}img{image-rendering:pixelated;max-width:100%}h2{border-top:1px solid #444;'
        'padding-top:12px}.f{color:#f66}.ok{color:#8c8}</style><h1>AMOLED frame report</h1>'
        '<p>Every menu state, 300 frames (5 s at 60 fps) idle and after every button / touch, at morning 07:00, day '
        '13:00, evening 18:00 and night 22:00. Each sheet: the distinct moments in order, Default on top, AMOLED below, '
        'frame numbers under them. Buttons that do exactly the same are grouped.</p>']
summary = []
for t in TIMES:
    names = sorted({os.path.basename(p)[4:] for p in glob.glob('rec/def_%s_*' % t)})
    # a state = its idle recording NAME (the one with NAME_keyA); its inputs: NAME_keyX and NAME_TOUCH
    for st in sorted({n.split('_key')[0] for n in names if '_key' in n} | {n for n in names if n.endswith('_intro')}):
        members = [n for n in names if n == st or (n.startswith(st + '_') and '_' not in n[len(st) + 1:])]
        groups = collections.OrderedDict()
        for n in members:
            df, af = frames('def', n), frames('am', n)
            if df is None or af is None:
                continue
            sig = tuple(digest(df[0], f) for f in df[1]) + tuple(digest(af[0], f) for f in af[1])
            groups.setdefault(sig, []).append((n, df, af))
        page.append('<h2>%s</h2>' % html.escape(st))
        for gi, (sig, items) in enumerate(groups.items()):
            n, df, af = items[0]
            labels = ', '.join((x[len(st) + 1:] or 'idle').replace('key', '') for x, _, _ in items)
            ms = moments(df, af)
            nd, na = len(set(df[1])), len(set(af[1]))
            extra = na > nd
            sheet = '%s_%d.png' % (st, gi)
            rows = (len(ms) + COLS - 1) // COLS
            im = Image.new('RGB', (COLS * 258, rows * (384 * 2 + 20)), (40, 40, 40))
            dr = ImageDraw.Draw(im)
            worst = 0
            for k, (x, y) in enumerate(ms[:COLS * 12]):
                cx, cy = (k % COLS) * 258, (k // COLS) * (768 + 20)
                im.paste(Image.open(df[0] + '/' + df[1][x]), (cx, cy))
                im.paste(Image.open(af[0] + '/' + af[1][x]), (cx, cy + 384))
                r = refs.get(n, {}).get((x, y))
                w = r['wrong'] if r and 'wrong' in r else None
                worst = max(worst, w or 0)
                dr.text((cx + 2, cy + 770), 'f%d-%d' % (x, y) + ('  %d px off %s' % (w, r['ref'][:2]) if w else ''),
                        fill=(255, 255, 255))
            im.save(os.path.join(out, sheet), optimize=True)
            page.append('<h3>%s %s</h3><p>%d moments; Default %d distinct frames, AMOLED %d%s</p><img src="%s">' % (
                html.escape(labels), '' if len(items) == 1 else '(%d inputs, same result)' % len(items), len(ms), nd, na,
                ' <span class="f">AMOLED has extra in-between frames</span>' if extra else '', sheet))
            summary.append((st, labels, len(ms), nd, na, worst))
open(os.path.join(out, 'index.html'), 'w', encoding='utf-8').write('\n'.join(page))
json.dump(summary, open(os.path.join(out, 'summary.json'), 'w'), indent=0)
print('states', len({s[0] for s in summary}), 'groups', len(summary), 'with extra AMOLED frames',
      sum(1 for s in summary if s[4] > s[3]))
