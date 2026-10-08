"""crop.py <screenshot> <out> x y w h  (CSS px; viewport width 1180)"""
import sys
from PIL import Image
src, out, x, y, w, h = sys.argv[1], sys.argv[2], *map(float, sys.argv[3:7])
im = Image.open(src); s = im.width / 1180
box = tuple(int(round(v * s)) for v in (x - 8, y - 8, x + w + 8, y + h + 8))
c = im.crop(box); c = c.resize((1000, int(c.height * 1000 / c.width)), Image.LANCZOS) if c.width > 1000 else c
c.convert('RGB').save(out, quality=88); print(out, im.size, c.size)
