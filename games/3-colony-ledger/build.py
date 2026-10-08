"""Inline core.js and app.js into src.html -> index.html (one self-contained file)."""
import os
H = os.path.dirname(os.path.abspath(__file__))
rd = lambda p: open(os.path.join(H, p), encoding='utf-8').read()
core = rd('core.js').replace("if (typeof module !== 'undefined')", "if (typeof module !== 'undefined' && module.exports)")
page = rd('src.html').replace('/*CORE_JS*/', core).replace('/*APP_JS*/', rd('app.js'))
open(os.path.join(H, 'index.html'), 'w', encoding='utf-8').write(page)
print('wrote index.html', len(page.encode('utf-8')), 'bytes')
