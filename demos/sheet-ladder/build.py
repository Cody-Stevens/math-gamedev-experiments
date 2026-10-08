"""Build the load-ladder Tacksheet from each ladder's out/ladder.json (quiet re-run) + videos."""
import json, os, shutil, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from charts import loglog, CSS, COL, _ms, _lbl

HERE = os.path.dirname(os.path.abspath(__file__))
DEMOS = os.path.dirname(HERE)
OUT = os.path.join(HERE, 'out')
os.makedirs(OUT, exist_ok=True)
SRC = sys.argv[1] if len(sys.argv) > 1 else 'ladder.json'


def load(d):
    return json.load(open(os.path.join(DEMOS, d, 'out', SRC), encoding='utf-8'))


def med(s, k):
    v = s.get(k)
    if not isinstance(v, dict): return None
    m = v.get('median_ms')
    return m if isinstance(m, (int, float)) and m == m and m != float('inf') else None


def series(L, key, name, color, floor=False, dash=False):
    pts = []
    for s in L['stages']:
        m = med(s, key)
        if m is None: continue
        if floor and (m <= 0 or (key == 'with' and s.get('with_resolution_limited'))):
            pts.append((s['n'], max(m, 1e-4), 'floor'))
        else:
            pts.append((s['n'], m))
    return dict(name=name, color=color, pts=pts, dash=dash)


def fits(L, key):
    """largest n with median <= budget, first n over budget (None if never)."""
    ok, over = None, None
    for s in L['stages']:
        m = med(s, key)
        if m is None: continue
        if m <= L['budget_ms']: ok = s['n']
        elif over is None: over = s['n']
    return ok, over


def rx(v): return f'{v:.0f}×' if v >= 10 else f'{v:.2g}×'


def pct(x, d=0): return f'{100*x:.{d}f}%'


def n(v):
    v = float(v)
    if v >= 1e6: return f'{v/1e6:.3g}M'
    return f'{int(v):,}'


def video(d, stem):
    if os.environ.get('NOVID'):
        return f'<figure class="vid"><img src="../../{d}/out/poster.png" style="width:100%">'
    src = os.path.join(DEMOS, d, 'out', f'{stem}.mp4')
    dst = os.path.join(OUT, f'{stem}.mp4')
    mb = os.path.getsize(src) / 1e6
    if mb > 12:
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', src, '-c:v', 'libx264', '-preset', 'slow', '-crf', '30',
                        '-pix_fmt', 'yuv420p', '-movflags', '+faststart', '-an', dst], check=True)
    else:
        shutil.copy(src, dst)
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', os.path.join(DEMOS, d, 'out', 'poster.png'),
                    '-vf', 'scale=1280:-1', '-q:v', '4', os.path.join(OUT, f'{stem}.jpg')], check=True)
    return (f'<figure class="vid"><video controls playsinline preload="none" poster="./files/{stem}.jpg" '
            f'src="./files/{stem}.mp4"></video>')


W_, T_, G_, Y_ = COL['without'], COL['with'], COL['third'], COL['fourth']
sec = {}
summary = []

# ---------------- L1 snaky ----------------
L = load('L1-snaky-ladder')
wo, wo_over = fits(L, 'without'); wi, wi_over = fits(L, 'with')
ratios = [s.get('ratio_without_over_with') for s in L['stages'] if s.get('ratio_without_over_with')]
c1 = loglog([series(L, 'without', 'α-β search', W_), series(L, 'with', 'certificate', T_, floor=True)],
            'Snaky: Maker decision cost per frame vs concurrent games', 'concurrent games',
            note='hollow = at the 0.1 µs timer floor (true cost is lower)')
q = [(s['n'], 100 * s['quality']['without'], 100 * s['quality']['with']) for s in L['stages']
     if s.get('quality') and s['quality'].get('without') is not None]
c1q = loglog([dict(name='α-β search', color=W_, pts=[(a, max(b, 0.01)) for a, b, _ in q]),
              dict(name='certificate', color=T_, pts=[(a, c) for a, _, c in q])],
             'Win rate vs the α-β Breaker when 16.7 ms is split across all games', 'concurrent games',
             ylabel='Maker win rate', budget=None, ylin=(0, 104, [0, 25, 50, 75, 100]), ylog=False, yfmt=lambda v: f'{v:.0f}%')
q50 = next((a for a, b, _ in q if b < 50), None)
sec['snaky'] = dict(wo=wo, wo_over=wo_over, wi=wi, wi_over=wi_over, rmin=min(ratios), rmax=max(ratios), c=c1, cq=c1q,
                    q50=q50, fit=L['fit'])
summary.append(['Snaky opponent (187)', 'concurrent games', n(wo), n(wi) + ('' if wi_over else ' (largest tested)'),
                f'×{min(ratios)/1e3:.0f}k–{max(ratios)/1e3:.0f}k per frame', 'Speedup, tens of thousands × per decision'])

# ---------------- L2 blob ----------------
L = load('L2-blob-ladder')
c2 = loglog([series(L, 'without', 'MBO from noise', W_), series(L, 'without_coarse_to_fine', 'MBO coarse→fine', G_, dash=True),
             series(L, 'with', 'closed form', T_)],
            'Periodic blob: time to get within 2% of the optimal area vs grid size', 'voxels',
            ylabel='median time to target')
st = {s['grid']: s for s in L['stages']}
big = L['stages'][-1]
r2 = [s['ratio_without_over_with'] for s in L['stages'] if s.get('ratio_without_over_with')]
ctf = [(s['grid'], med(s, 'without_coarse_to_fine') / med(s, 'with')) for s in L['stages'] if med(s, 'without_coarse_to_fine') and med(s, 'with')]
fails = [(s['grid'], s['without'].get('failures', 0), s['quality'].get('runs', 9)) for s in L['stages']]
wi2, wi2_over = fits(L, 'with'); wo2, _ = fits(L, 'without')
sec['blob'] = dict(c=c2, rmin=min(r2), rmax=max(r2), ctf=ctf, fails=fails, wi=wi2, wi_over=wi2_over, wo=wo2, st=st,
                   fit=L.get('fit_large_n', L['fit']), big=big)
summary.append(['Periodic blob (354)', 'grid voxels', 'never (32³ already over)',
                next(s['grid'] for s in L['stages'] if s['n'] == wi2).replace('^3','³') if wi2 else '—',
                f'×{min(r2):.0f}–{max(r2):.0f} time to target', 'Constant-factor speedup + never stuck'])

# ---------------- L3 fractal ----------------
L = load('L3-fractal-ladder')
ct = L['cost_to_target']
pts_w = [(s['without']['median_ms'], abs(s['quality']['median_err_signed']['without'])) for s in L['stages'] if s.get('quality')]
pts_t = [(s['with']['median_ms'], max(abs(s['quality']['median_err_signed']['with']), 1e-4)) for s in L['stages'] if s.get('quality')]
c3 = loglog([dict(name='length clock', color=W_, pts=pts_w), dict(name='δ^(7/4) clock', color=T_, pts=pts_t)],
            'Fractal frontier: timing error vs compute per curve (each dot is a grid size)', 'ms per curve',
            ylabel='median-duration error', budget=None, hlines=[(0.05, '5% target')], xfmt=lambda v: _ms(v),
            yfmt=lambda v: f'{100*v:g}%', note='dots left→right: n = 8, 16, 32 … 8192')
c3c = loglog([series(L, 'without', 'either clock (same curve)', G_)],
             'Fractal frontier: cost per curve vs grid size (identical on both sides)', 'grid size n')
sec['fractal'] = dict(c=c3, cc=c3c, ct=ct, fit=L['fit'], mx=L['max_n_within_budget'])
summary.append(['Fractal frontier (223)', 'grid size', f'n = {ct["without"]["n"]} to hit target ({_ms(round(ct["without"]["median_ms_per_curve"],0))}/curve)',
                f'n = {ct["with"]["n"]} ({_ms(ct["with"]["median_ms_per_curve"])}/curve)',
                f'×{ct["cost_ratio_ms"]:,.0f} at equal accuracy', 'Run coarse, get fine-grid statistics'])

# ---------------- L4 economy ----------------
L = load('L4-economy-ladder')
ext = L.get('with_only_extension') or []
s_with = series(L, 'with', 'weakly reversible', T_)
for e in ext:
    if isinstance(e.get('with'), dict): s_with['pts'].append((e['n'], e['with']['median_ms']))
c4 = loglog([series(L, 'without', 'two recipes removed', W_), s_with],
            'Economy: integrator cost per frame vs number of towns', 'towns')
wo4, wo4_over = fits(L, 'without'); wi4, wi4_over = fits(L, 'with')
for e in ext:
    if isinstance(e.get('with'), dict):
        if e['with']['median_ms'] <= L['budget_ms']: wi4 = max(wi4 or 0, e['n'])
        elif not wi4_over or e['n'] < wi4_over: wi4_over = e['n']
r4 = [s['ratio_median'] for s in L['stages'] if s['n'] >= 16 and s.get('ratio_median')]
sec['econ'] = dict(c=c4, wo=wo4, wo_over=wo4_over, wi=wi4, wi_over=wi4_over, rmin=min(r4), rmax=max(r4),
                   hz=L['horizon_halt_1e12'], hzn=L['horizon_never_halt'], fx=L['fixed_step_rk2'], fit=L['fit'])
summary.append(['Stable economy (149)', 'towns', n(wo4), n(wi4) + ('' if wi4_over else ' (largest tested)'), f'×{min(r4):.0f}–{max(r4):.0f} per frame',
                'Cheaper while the other side collapses'])

# ---------------- L5 sweeps ----------------
L = load('L5-sweeps/02-fluid-sweep')
c5a = loglog([series(L, 'without', 'exp-scaling tween', W_), series(L, 'lerp', 'lerp tween', G_, dash=True),
              series(L, 'with', 'shear flow (RK4)', T_)], 'Fluid gates: cost vs particles per region', 'particles per region')
a = dict(L=L, wi=fits(L, 'with'), wo=fits(L, 'without'), q=L['stages'][0]['quality'])
L = load('L5-sweeps/05-crowd-sweep')
c5b = loglog([series(L, 'without', 'frozen labels', W_), series(L, 'with', 'recomputed labels', T_)],
             'Crowd hub: cost vs agents', 'agents')
b = dict(L=L, wi=fits(L, 'with'), wo=fits(L, 'without'))
L = load('L5-sweeps/06-dungeon-sweep')
c5c = loglog([series(L, 'without', '1 swap per door', W_), series(L, 'empirical', 'Curveball, 1/door', G_, dash=True),
              series(L, 'with', 'certified Curveball', T_)], 'Dungeon shuffle: cost per layout vs rooms', 'rooms')
c = dict(L=L, wi=fits(L, 'with'), fit=L['fit'])
L = load('L5-sweeps/08-morph-sweep')
c5d = loglog([series(L, 'without', 'Minkowski', W_), series(L, 'with', 'log Wulff', T_)],
             'Convex morph: cost per body vs half-space directions', 'directions')
d = dict(L=L, wi=fits(L, 'with'), wo=fits(L, 'without'))
r8 = [s['ratio_without_over_with'] for s in L['stages']]
ra = [s['ratio_without_over_with'] for s in a['L']['stages']]
rb = [s['ratio_without_over_with'] for s in b['L']['stages']]
sec['sweeps'] = dict(a=a, b=b, c=c, d=d, ca=c5a, cb=c5b, cc=c5c, cd=c5d, r8=r8, ra=ra, rb=rb)
summary += [
    ['Fluid gates (376)', 'particles/region', 'beyond 1M', f'{n(a["wi"][0])}', f'WITH {1/max(ra):.0f}–{1/min(ra):.0f}× slower', 'Guarantee costs more'],
    ['Crowd hub (374)', 'agents', f'{n(b["wo"][0])}' + ('' if b['wo'][1] else ' (largest tested)'), f'{n(b["wi"][0])}', f'WITH {1/max(rb):.1f}–{1/min(rb):.1f}× slower', 'Guarantee costs more'],
    ['Dungeon shuffle (131)', 'rooms', 'beyond 320', f'{c["wi"][0]}', f'WITH grows ~n^{c["fit"]["with_exponent"]:.1f}', 'Certificate is offline-only past ~50 rooms'],
    ['Convex morph (091)', 'directions', f'{d["wo"][0]}', f'{d["wi"][0]}', f'WITH {min(r8):.1f}–{max(r8):.1f}× faster', 'Incidental: smaller body to clip'],
]
json.dump(sec, open(os.path.join(HERE, 'sec.json'), 'w', encoding='utf-8'), indent=1, default=str)
json.dump(summary, open(os.path.join(HERE, 'summary.json'), 'w', encoding='utf-8'), indent=1, ensure_ascii=False)

# ---------------- videos ----------------
V = {k: video(dd, dd) for k, dd in [('snaky', 'L1-snaky-ladder'), ('blob', 'L2-blob-ladder'),
                                     ('fractal', 'L3-fractal-ladder'), ('econ', 'L4-economy-ladder')]}

tpl = open(os.path.join(HERE, 'src.html'), encoding='utf-8').read()
import re
rows = [[f'{r[0]} · {r[1]}', r[2], r[3], r[4]] for r in summary]
ctx = dict(CSS=CSS, V=V, S=sec, summary=json.dumps({'columns': ['Demo · load', 'WITHOUT fits 60 fps up to', 'WITH fits up to',
                                                                'Gap'], 'rows': rows}, ensure_ascii=False), rx=rx,
           n=n, pct=pct, ms=_ms)
html = re.sub(r'\{\{(.+?)\}\}', lambda m: str(eval(m.group(1), dict(ctx))), tpl, flags=re.S)
html = html.replace('^3', '³')
open(os.path.join(OUT, 'load-ladders.html'), 'w', encoding='utf-8').write(html)
print('ok', os.path.join(OUT, 'load-ladders.html'))
for r in summary: print(r)
