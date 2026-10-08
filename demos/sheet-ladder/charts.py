"""Inline-SVG log-log charts for the load-ladder sheet (the kit's charts have no log axes)."""
import math

COL = {'without': '#d9482b', 'with': '#0f8a7e', 'third': '#6d6a75', 'fourth': '#b07a12'}
W, H = 760, 380
ML, MR, MT, MB = 70, 150, 48, 54


def _ticks(lo, hi):
    a, b = math.floor(math.log10(lo)), math.ceil(math.log10(hi))
    return [10 ** k for k in range(a, b + 1)]


def _lbl(v, unit=''):
    if v >= 1e6: s = f'{v/1e6:g}M'
    elif v >= 1e3: s = f'{v/1e3:g}k'
    elif v >= 1: s = f'{v:g}'
    else: s = f'{v:g}'
    return s + unit


def _ms(v):
    if v >= 1000: return f'{v/1000:.3g} s'
    if v >= 1: return f'{v:.3g} ms'
    if v >= 1e-3: return f'{v*1000:.3g} µs'
    return f'{v*1e6:.3g} ns'


def loglog(series, title, xlabel, ylabel='median compute per frame', budget=16.7, ylin=None,
           xfmt=_lbl, yfmt=_ms, hlines=(), note=None, ylog=True):
    """series: list of dict(name, color, pts=[(x, y, flag)], dash=False). flag 'floor' = hollow marker."""
    pts = [(x, y) for s in series for x, y, *_ in s['pts'] if y is not None and y > 0]
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts] + ([budget] if budget else []) + [h[0] for h in hlines]
    x0, x1 = min(xs), max(xs)
    if ylin: y0, y1 = ylin[0], ylin[1]
    else:
        y0, y1 = min(ys) / 1.6, max(ys) * 1.6
    lx0, lx1 = math.log10(x0), math.log10(x1)
    if lx1 - lx0 < 1e-9: lx1 = lx0 + 1
    pw, ph = W - ML - MR, H - MT - MB

    def X(x): return ML + (math.log10(x) - lx0) / (lx1 - lx0) * pw

    if ylog:
        ly0, ly1 = math.log10(y0), math.log10(y1)
        def Y(y): return MT + ph - (math.log10(max(y, y0)) - ly0) / (ly1 - ly0) * ph
    else:
        def Y(y): return MT + ph - (y - y0) / (y1 - y0) * ph

    o = [f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="{title}" class="ll" xmlns="http://www.w3.org/2000/svg">',
         f'<text x="{ML}" y="20" class="t">{title}</text>']
    # grid
    for t in _ticks(x0, x1):
        if x0 / 1.01 <= t <= x1 * 1.01:
            o.append(f'<line x1="{X(t):.1f}" y1="{MT}" x2="{X(t):.1f}" y2="{MT+ph}" class="g"/>'
                     f'<text x="{X(t):.1f}" y="{MT+ph+18}" class="ax" text-anchor="middle">{xfmt(t)}</text>')
    yt = _ticks(y0, y1) if ylog else [y0 + (y1 - y0) * k / 4 for k in range(5)]
    if not ylog and ylin and len(ylin) > 2: yt = ylin[2]
    for t in yt:
        if y0 <= t <= y1:
            o.append(f'<line x1="{ML}" y1="{Y(t):.1f}" x2="{ML+pw}" y2="{Y(t):.1f}" class="g"/>'
                     f'<text x="{ML-6}" y="{Y(t)+4:.1f}" class="ax" text-anchor="end">{yfmt(t)}</text>')
    o.append(f'<rect x="{ML}" y="{MT}" width="{pw}" height="{ph}" class="fr"/>')
    o.append(f'<text x="{ML+pw/2}" y="{H-12}" class="al" text-anchor="middle">{xlabel} (log scale)</text>')
    o.append(f'<text x="16" y="{MT+ph/2}" class="al" text-anchor="middle" transform="rotate(-90 16 {MT+ph/2})">{ylabel}{" (log)" if ylog else ""}</text>')
    labels = []
    if budget:
        o.append(f'<line x1="{ML}" y1="{Y(budget):.1f}" x2="{ML+pw}" y2="{Y(budget):.1f}" class="bud"/>')
        labels.append([Y(budget), 0, dict(name='16.7 ms = 60 fps', color='#1d1b20', cls='budl')])
    for v, lab in hlines:
        o.append(f'<line x1="{ML}" y1="{Y(v):.1f}" x2="{ML+pw}" y2="{Y(v):.1f}" class="bud"/>')
        labels.append([Y(v), 0, dict(name=lab, color='#1d1b20', cls='budl')])
    # series
    for s in series:
        p = [(x, y, (f[0] if f else None)) for x, y, *f in s['pts'] if y is not None and y > 0]
        if not p: continue
        d = ' '.join(f'{"M" if i == 0 else "L"}{X(x):.1f},{Y(y):.1f}' for i, (x, y, _) in enumerate(p))
        dash = ' stroke-dasharray="6 4"' if s.get('dash') else ''
        o.append(f'<path d="{d}" fill="none" stroke="{s["color"]}" stroke-width="2.6"{dash}/>')
        crossed = False
        for x, y, fl in p:
            hollow = fl == 'floor'
            o.append(f'<circle cx="{X(x):.1f}" cy="{Y(y):.1f}" r="4.2" fill="{"#fff" if hollow else s["color"]}" stroke="{s["color"]}" stroke-width="2"/>')
            if budget and not crossed and y > budget:
                crossed = True
                o.append(f'<circle cx="{X(x):.1f}" cy="{Y(y):.1f}" r="10" fill="none" stroke="{s["color"]}" stroke-width="1.6"/>')
        labels.append([Y(p[-1][1]), X(p[-1][0]), s])
    # right-edge labels, de-overlapped
    labels.sort(key=lambda r: r[0])
    for i in range(1, len(labels)):
        if labels[i][0] - labels[i - 1][0] < 16: labels[i][0] = labels[i - 1][0] + 16
    shift = max(0, labels[-1][0] - (H - 8)) if labels else 0
    for r in labels: r[0] -= shift
    for i in range(len(labels) - 2, -1, -1):
        if labels[i + 1][0] - labels[i][0] < 16: labels[i][0] = labels[i + 1][0] - 16
    for y, x, s in labels:
        o.append(f'<text x="{ML+pw+14}" y="{y+4:.1f}" class="{s.get("cls", "sl")}" fill="{s["color"]}">{s["name"]}</text>')
    if note:
        o.append(f'<text x="{ML}" y="38" class="nt">{note}</text>')
    o.append('</svg>')
    return '\n'.join(o)


CSS = """
.ll{width:100%;height:auto;display:block;background:#fffdf7;border:1px solid #d9d3c3;border-radius:6px;margin:.6rem 0}
.ll .t{font:600 15px system-ui,sans-serif;fill:#1d1b20}
.ll .ax{font:12px ui-monospace,Consolas,monospace;fill:#55505c}
.ll .al{font:12px system-ui,sans-serif;fill:#55505c}
.ll .g{stroke:#e7e1d2;stroke-width:1}
.ll .fr{fill:none;stroke:#bdb5a3}
.ll .bud{stroke:#1d1b20;stroke-width:1.4;stroke-dasharray:3 4}
.ll .budl{font:12px system-ui,sans-serif;fill:#1d1b20}
.ll .sl{font:600 13px system-ui,sans-serif}
.ll .nt{font:italic 12px system-ui,sans-serif;fill:#55505c}
"""
