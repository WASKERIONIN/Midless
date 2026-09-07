#!/bin/sh
# Builds and runs the headless worldgen self-check.
#
# It compiles the *real* server worldgen translation units from this repository
# (server/src/world/*.c, server/src/world/chunk/chunk.c, shared/chunkdata.c,
# server/src/scripting/luaworldgen.c).  Only the engine layer that those files
# link against (file IO, logging, allocation) is stubbed in stubs.c, so every
# assertion runs against shipping code paths.
#
# Usage:
#   tools/worldgen-check/build.sh            build + run every test definition
#   tools/worldgen-check/build.sh <mod.lua> [seed]   build + run one definition
#
# Dependencies are downloaded into tools/worldgen-check/.deps (gitignored).
# The project vendors the same single-file libraries in /libs, so if you
# already have them you can point DEPS_DIR at that directory instead.

set -e

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
DEPS_DIR=${DEPS_DIR:-"$HERE/.deps"}
RAYLIB_TAG=4.5.0

mkdir -p "$DEPS_DIR"

fetch() { # fetch <name> <url> <strip-component-dir>
    if [ ! -d "$DEPS_DIR/$3" ]; then
        echo "fetching $1 ..."
        curl -sSfL "$2" -o "$DEPS_DIR/$1.tar.gz"
        tar xzf "$DEPS_DIR/$1.tar.gz" -C "$DEPS_DIR"
    fi
}

fetch stb           https://codeload.github.com/nothings/stb/tar.gz/refs/heads/master          stb-master
fetch FastNoiseLite https://codeload.github.com/Auburn/FastNoiseLite/tar.gz/refs/heads/master  FastNoiseLite-master
fetch minilua       https://codeload.github.com/edubart/minilua/tar.gz/refs/heads/main         minilua-main
fetch raylib        https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/$RAYLIB_TAG    raylib-$RAYLIB_TAG

STB=$(echo "$DEPS_DIR"/stb-*)
FNL=$(echo "$DEPS_DIR"/FastNoiseLite-*/C)
LUA=$(echo "$DEPS_DIR"/minilua-*)
RL=$(echo "$DEPS_DIR"/raylib-*)/src

# MIDLESS_FNL_EXTERNAL: worldgenerator.c would otherwise pull the FastNoiseLite
# implementation in twice (the impl block sits outside the header's include
# guard), so stubs.c provides it instead.
CFLAGS="-std=c99 -O2 -g -D_DEFAULT_SOURCE -DPLATFORM_DESKTOP -DOS_LINUX -DMIDLESS_FNL_EXTERNAL"
CFLAGS="$CFLAGS -Wall -Wno-unused-variable -Wno-aggressive-loop-optimizations"
INC="-I$ROOT/shared -I$ROOT/server/src -I$ROOT/server/src/world -I$ROOT/server/src/world/chunk"
INC="$INC -I$ROOT/server/src/scripting -I$STB -I$FNL -I$LUA -I$RL -I$RL/external"
SRC="$ROOT/server/src/world/worldgen.c $ROOT/server/src/world/worldgenfield.c"
SRC="$SRC $ROOT/server/src/world/worldgenfeatures.c $ROOT/server/src/world/worldgenerator.c"
SRC="$SRC $ROOT/server/src/world/chunk/chunk.c $ROOT/shared/chunkdata.c"
SRC="$SRC $ROOT/shared/blockdefinition.c $ROOT/server/src/scripting/luaworldgen.c"

# shellcheck disable=SC2086
gcc $CFLAGS $INC "$HERE/wgtest.c" "$HERE/stubs.c" $SRC -o "$HERE/wgtest" -lm -lpthread
# shellcheck disable=SC2086
gcc $CFLAGS $INC "$HERE/dumpcolumn.c" "$HERE/stubs.c" $SRC -o "$HERE/dumpcolumn" -lm -lpthread
echo "built $HERE/wgtest and $HERE/dumpcolumn"

# The worldgen writes world/worldgen.meta next to the working directory, so
# every run happens in a fresh temporary directory.
run_one() { # run_one <mod.lua> <seed>
    RUNDIR=$(mktemp -d)
    ( cd "$RUNDIR" && "$HERE/wgtest" "$1" "$2" )
}

if [ "$#" -gt 0 ]; then
    run_one "$1" "${2:-1337}"
    exit $?
fi

status=0
for seed in 1337 7 99991; do
    echo "############ seed $seed ############"
    for mod in "$HERE"/mods/*.lua "$ROOT"/build/client/mods/*.lua; do
        [ -e "$mod" ] || continue
        run_one "$mod" "$seed" || status=1
    done
done
exit $status
