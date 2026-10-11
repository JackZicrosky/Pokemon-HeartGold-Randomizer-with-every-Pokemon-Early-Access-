#!/bin/bash
# Romhack release patch: test.nds (a release build: include/rh/rh_test.h RH_TEST 0) against the clean ROM, as plain
# VCDIFF (xdelta3 -S none: no secondary compression, so Rom Patcher JS and our HeartGold Randomizer Patcher.html can
# apply it), then checks that applying it gives test.nds back.
# Usage (WSL, repository root): bash tools/rh/release/make_patch.sh "PATH/TO/Pokemon HeartGold (USA) v1.0.nds" OUT.xdelta
set -e
CLEAN="$1"; OUT="$2"
rm -f "$OUT" build/release_check.nds
xdelta3 -e -9 -S none -s "$CLEAN" test.nds "$OUT"
xdelta3 -d -s "$CLEAN" "$OUT" build/release_check.nds
cmp test.nds build/release_check.nds && echo "patch OK: $(stat -c %s "$OUT") bytes"
rm -f build/release_check.nds
