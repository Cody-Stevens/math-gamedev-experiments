import json, os, re, shutil, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
DEMOS = os.path.dirname(HERE)
OUT = os.path.join(HERE, 'out')
os.makedirs(OUT, exist_ok=True)
IDS = ['01-economy', '02-fluid-gates', '03-snaky', '04-fractal-frontier', '05-crowd-hub',
       '06-dungeon-shuffle', '07-periodic-blob', '08-convex-morph', '09-cubic-interference']


def fmt(ms):
    if ms >= 10: return f'{ms:.1f} ms'
    if ms >= 0.1: return f'{ms:.2f} ms'
    if ms >= 0.001: return f'{ms*1000:.1f} µs'
    return f'{ms*1000:.2f} µs'


src = open(os.path.join(HERE, 'src.html'), encoding='utf-8').read()
bench = {}
for d in IDS:
    r = json.load(open(os.path.join(DEMOS, d, 'out', 'clean', 'results.json'), encoding='utf-8'))
    p = {x['side']: x['compute_ms_per_frame']['median'] for x in r['panels']}
    bench[d] = p
    src = src.replace(f'__C_{d}__', f'without {fmt(p["left"])} → with {fmt(p["right"])}, median, measured alone')
    shutil.copy(os.path.join(DEMOS, d, 'out', f'{d}.mp4'), os.path.join(OUT, f'{d}.mp4'))
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', os.path.join(DEMOS, d, 'out', 'poster.png'),
                    '-vf', 'scale=1280:-1', '-q:v', '4', os.path.join(OUT, f'{d}.jpg')], check=True)

table = json.load(open(os.path.join(DEMOS, '..', 'verify-games', 'table.json'), encoding='utf-8'))
demoed = {'G1.3', 'G3.2', 'G4.1', 'G4.4', 'G5.1', 'G6.2', 'G10.1', 'G10.2', 'G10.3'}
rows = [[l['fam'], l['title'], l['pitch']] for l in table if l.get('verdict') == 'SOLID' and l['id'] not in demoed]
rows.sort(key=lambda r: r[0])
src = src.replace('__OTHER_ROWS__', json.dumps({'columns': ['Family', 'Lead', 'What you get'], 'rows': rows}, ensure_ascii=False))
assert '__' not in re.sub(r'<script src[^>]*>', '', src).replace('__init__', ''), 'unfilled placeholder'
open(os.path.join(OUT, 'new-math-demos.html'), 'w', encoding='utf-8').write(src)
json.dump(bench, open(os.path.join(HERE, 'bench-clean.json'), 'w'), indent=1)
for d, p in bench.items(): print(d, fmt(p['left']), '->', fmt(p['right']))
print('rows', len(rows))
