#!/bin/bash
# fetch_raylib.sh - make the pinned raylib 4.5 HEADERS available locally for
# the Linux dev tools (compile_check.sh, tests/run_tests.sh). The headers are
# a build dependency, not a repo tool, so they live outside git; this script
# recreates them in one command if /tmp was wiped.
#
#   tools_dev/fetch_raylib.sh            -> /tmp/raylib45/src
#   RAYLIB_DEST=~/raylib45 tools_dev/fetch_raylib.sh
set -eu
DEST="${RAYLIB_DEST:-/tmp/raylib45}"
if [ -f "$DEST/src/raylib.h" ]; then
    echo "raylib headers already at $DEST/src"
    exit 0
fi
mkdir -p "$DEST"
git clone --depth 1 --branch 4.5.0 https://github.com/raysan5/raylib.git "$DEST"
echo "raylib 4.5 headers at $DEST/src"
