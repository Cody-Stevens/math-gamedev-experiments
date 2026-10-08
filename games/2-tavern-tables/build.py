"""Inline certificate, C++ ladder reference numbers, core.js and app.js into one self-contained index.html."""
import json, os
H = os.path.dirname(os.path.abspath(__file__))
rd = lambda p: open(os.path.join(H, p), encoding='utf-8').read()
cert = json.dumps(json.loads(rd('snaky_cert.json')), separators=(',', ':'))
# reference panel: C++ measurements, read-only source demos/L1-snaky-ladder/out/ladder.json
lad = json.load(open(os.path.join(H, '..', '..', '..', 'demos', 'L1-snaky-ladder', 'out', 'ladder.json'), encoding='utf-8'))
keep = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 16384, 131072, 262144]
rows = []
for s in lad['stages']:
    if s['n'] not in keep: continue
    q = s.get('quality') or {}
    rows.append({'n': s['n'], 'wo': (s.get('without') or {}).get('median_ms'), 'wi': s['with']['median_ms'],
                 'qa': q.get('without'), 'qg': q.get('without_vs_greedy'), 'cq': q.get('with')})
ladder = {'stages': rows, 'fit': lad['max_n_within_budget']}
data = 'const SNAKY_CERT = ' + cert + ';\nconst LADDER = ' + json.dumps(ladder, separators=(',', ':')) + ';\n'
page = rd('src.html').replace('/*DATA_JS*/', data).replace('/*CORE_JS*/', rd('core.js')).replace('/*APP_JS*/', rd('app.js'))
open(os.path.join(H, 'index.html'), 'w', encoding='utf-8').write(page)
print('wrote index.html', len(page.encode('utf-8')), 'bytes')
