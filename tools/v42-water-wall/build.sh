#!/bin/sh
# Headless check for the "water wall" in the v42 generator (Midless-src.rar).
#
# It compiles the REAL server/src/world/worldgenerator.c of the unpacked source
# tree (nothing re-implemented) against a few engine stubs, generates a
# 21x21 chunk area around spawn (y 0..95) and counts:
#   - water cells that have dry air as a horizontal neighbour ("wall")
#   - water cells with air directly below ("floating water")
#   - dry air cells below sea level (y < 48)
#
# Usage:
#   tools/v42-water-wall/build.sh <path-to-unpacked-Midless-src> [seed...]
#   e.g. tools/v42-water-wall/build.sh /tmp/Midless-src 1234 7 99991
#
# To test the patched generator, apply worldgenerator-no-caves-below-sea.patch
# to the source tree first (patch -p1 < ...) and run again: all three counters
# must be 0.
#
# raylib 4.5 headers are needed for raylib.h/raymath.h (RAYLIB_SRC, default
# downloads the 4.5.0 tarball into .deps).
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC=${1:?path to unpacked Midless-src required}
shift
SEEDS=${*:-1234 7 99991}
RL=${RAYLIB_SRC:-"$HERE/.deps/raylib-4.5.0/src"}
if [ ! -d "$RL" ]; then
    mkdir -p "$HERE/.deps"
    curl -sSfL https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/4.5.0 -o "$HERE/.deps/raylib.tgz"
    tar xzf "$HERE/.deps/raylib.tgz" -C "$HERE/.deps"
fi
CFLAGS="-std=gnu99 -O2 -D_DEFAULT_SOURCE -DPLATFORM_DESKTOP -Wno-aggressive-loop-optimizations"
INC="-I$SRC/server/src -I$SRC/server/src/world -I$SRC/server/src/world/chunk -I$SRC/shared -I$SRC/libs -I$RL"
GEN="$SRC/server/src/world/worldgenerator.c $SRC/server/src/world/chunk/chunk.c $SRC/shared/chunkdata.c"
# shellcheck disable=SC2086
gcc $CFLAGS $INC "$HERE/wall.c"  "$HERE/stubs.c" "$HERE/raymath_impl.c" $GEN -o "$HERE/wall"  -lm
# shellcheck disable=SC2086
gcc $CFLAGS $INC "$HERE/slice.c" "$HERE/stubs.c" "$HERE/raymath_impl.c" $GEN -o "$HERE/slice" -lm
status=0
for s in $SEEDS; do
    "$HERE/wall" "$s" 1 | head -5
    if "$HERE/wall" "$s" 1 | grep -q "water next to dry air  : [1-9]"; then status=1; fi
done
exit $status
