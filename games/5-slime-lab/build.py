# Inline core.js into src.html -> index.html (self-contained, opens from file://).
import pathlib
here = pathlib.Path(__file__).resolve().parent
src = (here / "src.html").read_text(encoding="utf8")
core = (here / "core.js").read_text(encoding="utf8")
tag = '<script src="core.js"></script>'
assert tag in src, "marker missing"
out = src.replace(tag, "<script>\n" + core + "\n</script>")
(here / "index.html").write_text(out, encoding="utf8")
print("wrote", here / "index.html", len(out), "bytes")
