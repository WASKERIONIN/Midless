#!/bin/bash
# run_tests.sh - headless regression tests for the glowmoth pollen system.
#
# Compiles the REAL client/src/mobs.c against stub headers (tests/stubs) and
# an exact behavioral model of raylib 4.5's rlgl batch renderer
# (tests/rlmock.h), then runs logic + render suites. No window, no GPU.
#
# Needs raylib 4.5 headers (raylib.h / raymath.h only): set RAYLIB_SRC to a
# raylib source tree (the same pinned version the CI builds: 4.5.0).
#   ./tests/run_tests.sh
#   RAYLIB_SRC=~/raylib-4.5.0/src ./tests/run_tests.sh
set -u
cd "$(dirname "$0")"

RAYLIB_SRC="${RAYLIB_SRC:-/tmp/raylib45/src}"
if [ ! -f "$RAYLIB_SRC/raylib.h" ]; then
    echo "raylib headers not found at '$RAYLIB_SRC'."
    echo "Clone raylib 4.5.0 and point RAYLIB_SRC at its src/ directory:"
    echo "  git clone --depth 1 --branch 4.5.0 https://github.com/raysan5/raylib.git /tmp/raylib45"
    exit 2
fi

BUILD=build
mkdir -p "$BUILD"
# copy the code under test so the stub headers shadow the project ones
cp ../client/src/mobs.c "$BUILD/mobs.c"

gcc -std=c99 -D_DEFAULT_SOURCE -DRAYMATH_STATIC_INLINE -O1 -Wall \
    -Wno-missing-braces -Wno-int-conversion \
    -I"$BUILD" -Istubs -I"$RAYLIB_SRC" -I"$RAYLIB_SRC/extras" \
    -I../client/src -I../client/src/block -I../shared \
    test_moths.c -o "$BUILD/test_moths" -lm || exit 1

"$BUILD/test_moths"
