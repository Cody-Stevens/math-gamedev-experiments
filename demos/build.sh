#!/usr/bin/env bash
# Build (and optionally run) a demo against the harness.
#   ./build.sh 00-sample                 -> demos/00-sample/demo.exe
#   ./build.sh 00-sample run [args...]   -> build, then run with outputs in demos/00-sample/out/
#   ./build.sh 00-sample preview         -> build, then run --preview (no video, poster.png only)
# Extra args after run/preview are passed to the demo (e.g. --frames 120 --nvenc).
# Env: CXXFLAGS_EXTRA adds compiler flags; CXX overrides the compiler.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CXX="${CXX:-/c/Program Files/LLVM/bin/clang++.exe}"
FLAGS=(-O2 -std=c++20 -march=native -Wall -Wno-unused-function -D_CRT_SECURE_NO_WARNINGS)

if [ $# -lt 1 ]; then
  echo "usage: $0 <demo-dir> [run|preview] [demo args...]" >&2
  exit 2
fi
DEMO="${1%/}"; shift
MODE="${1:-}"; [ $# -gt 0 ] && shift || true
SRC="$HERE/$DEMO/main.cpp"
[ -f "$SRC" ] || { echo "no such file: $SRC" >&2; exit 2; }

# Harness object is cached; rebuilt only when demo.h / demo.cpp change.
HOBJ="$HERE/harness/build/demo.o"
mkdir -p "$HERE/harness/build"
if [ ! -f "$HOBJ" ] || [ "$HERE/harness/demo.cpp" -nt "$HOBJ" ] || [ "$HERE/harness/demo.h" -nt "$HOBJ" ]; then
  echo "[build] harness/demo.cpp"
  "$CXX" "${FLAGS[@]}" ${CXXFLAGS_EXTRA:-} -c "$HERE/harness/demo.cpp" -o "$HOBJ"
fi

echo "[build] $DEMO/main.cpp"
"$CXX" "${FLAGS[@]}" ${CXXFLAGS_EXTRA:-} -I"$HERE/harness" "$SRC" "$HOBJ" -o "$HERE/$DEMO/demo.exe" -luser32 -lkernel32

case "$MODE" in
  "")      ;;
  run)     "$HERE/$DEMO/demo.exe" --out "$HERE/$DEMO/out" "$@" ;;
  preview) "$HERE/$DEMO/demo.exe" --out "$HERE/$DEMO/out" --preview "$@" ;;
  *)       echo "unknown mode: $MODE (use run or preview)" >&2; exit 2 ;;
esac
