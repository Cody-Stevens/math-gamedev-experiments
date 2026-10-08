"""Inline the cores, app and certificate into one self-contained page: out/practical-math.html."""
import json, os
H = os.path.dirname(os.path.abspath(__file__))
rd = lambda p: open(os.path.join(H, p), encoding='utf-8').read()
core = '\n'.join(rd(f) for f in ['snaky.js', 'econ.js', 'fk.js', 'dungeon.js'])
cert = json.dumps(json.loads(rd('snaky_cert.json')), separators=(',', ':'))
core = 'const SNAKY_CERT = ' + cert + ';\n' + core
page = rd('src.html').replace('/*CORE_JS*/', core).replace('/*APP_JS*/', rd('app.js'))
os.makedirs(os.path.join(H, 'out'), exist_ok=True)
open(os.path.join(H, 'out', 'practical-math.html'), 'w', encoding='utf-8').write(page)
print('wrote', len(page.encode('utf-8')), 'bytes')
