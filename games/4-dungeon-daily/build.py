"""Inline core.js into src.html -> index.html (self-contained, opens from file://)."""
from pathlib import Path
here = Path(__file__).resolve().parent
src = (here / 'src.html').read_text(encoding='utf-8')
core = (here / 'core.js').read_text(encoding='utf-8')
assert '/*@@CORE@@*/' in src and '</script' not in core
(here / 'index.html').write_text(src.replace('/*@@CORE@@*/', core), encoding='utf-8')
print('wrote', here / 'index.html', len(src) + len(core), 'bytes')
