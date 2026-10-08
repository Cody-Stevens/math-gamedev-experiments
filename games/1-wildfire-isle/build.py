# python build.py  ->  index.html = src.html with core.js (and bench.json, if present) inlined.
# index.html stays a single self-contained file.
import pathlib
d = pathlib.Path(__file__).resolve().parent
src = (d / 'src.html').read_text(encoding='utf-8')
core = (d / 'core.js').read_text(encoding='utf-8')
bench_file = d / 'bench.json'
bench = bench_file.read_text(encoding='utf-8').replace('\n', '') if bench_file.exists() else 'null'
core += '\nconst BENCH = ' + bench + '; // from bench.js (Node); shown only if the browser timer is frozen\n'
assert src.count('/*CORE*/') == 1
(d / 'index.html').write_text(src.replace('/*CORE*/', core), encoding='utf-8', newline='\n')
print('wrote', d / 'index.html')
