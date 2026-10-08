#!/usr/bin/env bash
# Sequential quiet re-run of all ladder sweeps and videos.
set -u
cd <repo>/demos
log() { echo "[$(date +%T)] $*"; }
for d in L1-snaky-ladder L2-blob-ladder L3-fractal-ladder L4-economy-ladder L5-sweeps/02-fluid-sweep L5-sweeps/05-crowd-sweep L5-sweeps/06-dungeon-sweep L5-sweeps/08-morph-sweep; do
  [ -f $d/out/ladder.json ] && cp $d/out/ladder.json $d/out/ladder.busy.json
  ./build.sh $d >/dev/null || log "BUILD FAIL $d"
done
for d in L1-snaky-ladder L2-blob-ladder L3-fractal-ladder L4-economy-ladder; do
  log "sweep $d"; ./$d/demo.exe --out $d/out --sweep > $d/out/sweep.log 2>&1 || log "SWEEP FAIL $d"
done
for d in L5-sweeps/02-fluid-sweep L5-sweeps/05-crowd-sweep L5-sweeps/06-dungeon-sweep L5-sweeps/08-morph-sweep; do
  log "sweep $d"; ./$d/demo.exe > $d/out/sweep.log 2>&1 || log "SWEEP FAIL $d"
done
for d in L1-snaky-ladder L2-blob-ladder L3-fractal-ladder L4-economy-ladder; do
  log "video $d"; ./$d/demo.exe --out $d/out --quiet > $d/out/video.log 2>&1 || log "VIDEO FAIL $d"
done
log done
