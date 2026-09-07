#!/bin/sh
# Cross-builds Midless for 64-bit Windows from Linux/macOS.
#
# The sandbox/CI machine usually has no MinGW-w64, so this uses Zig's C compiler
# (`zig cc -target x86_64-windows-gnu`), which ships the MinGW-w64 headers and
# import libraries.  Everything is linked statically, so the result needs no
# runtime DLLs.
#
# Usage:
#   tools/windows-build/build-windows.sh
#
# Output:
#   release/Midless-Windows-x64/      game.exe, server.exe, textures/, mods/
#   release/Midless-Windows-x64.zip
#
# Environment:
#   ZIG=/path/to/zig        use a specific zig binary (default: `zig` on PATH,
#                           or a `pip install ziglang` venv under ~/.cache)
#   SKIP_ZIP=1              do not create the zip

set -e

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
DEPS="$ROOT/.winbuild"
LIBS="$ROOT/libs"
OUT="$ROOT/release"
STAGE="$OUT/Midless-Windows-x64"
TARGET=x86_64-windows-gnu
RAYLIB_TAG=4.5.0

# ---------------------------------------------------------------- toolchain --
if [ -z "$ZIG" ]; then
    if command -v zig >/dev/null 2>&1; then
        ZIG=zig
    else
        for candidate in "$HOME"/.cache/midless-zig/bin/zig \
                         /tmp/zigenv/lib/python3.*/site-packages/ziglang/zig; do
            if [ -x "$candidate" ]; then ZIG="$candidate"; break; fi
        done
    fi
fi
if [ -z "$ZIG" ]; then
    echo "zig not found.  Install it with:  python3 -m venv ~/.cache/midless-zig && \\"
    echo "                                 ~/.cache/midless-zig/bin/pip install ziglang"
    exit 1
fi
echo "compiler: $($ZIG version)  target: $TARGET"

mkdir -p "$DEPS" "$LIBS" "$OUT"

# ------------------------------------------------------------- dependencies --
# The project expects its single-file dependencies in /libs (see README).
fetch() { # fetch <name> <url> <dir-under-deps>
    if [ ! -d "$DEPS/$3" ]; then
        echo "fetching $1 ..."
        curl -sSfL "$2" -o "$DEPS/$1.tar.gz"
        tar xzf "$DEPS/$1.tar.gz" -C "$DEPS"
    fi
}
fetch stb           https://codeload.github.com/nothings/stb/tar.gz/refs/heads/master          stb-master
fetch FastNoiseLite https://codeload.github.com/Auburn/FastNoiseLite/tar.gz/refs/heads/master  FastNoiseLite-master
fetch minilua       https://codeload.github.com/edubart/minilua/tar.gz/refs/heads/main         minilua-main
# README pins zpl-c/enet 2.3.6, but that release defines a non-static
# clock_gettime() on _WIN32, which collides with the winpthread one that
# zig's MinGW-w64 headers provide.  2.7.0 renamed its shim to _clock_gettime.
fetch enet          https://codeload.github.com/zpl-c/enet/tar.gz/refs/heads/master            enet-master
fetch raylib        https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/$RAYLIB_TAG    raylib-$RAYLIB_TAG
fetch raygui        https://codeload.github.com/raysan5/raygui/tar.gz/refs/tags/3.2            raygui-3.2

cp -f "$DEPS/stb-master/stb_ds.h"                    "$LIBS/stb_ds.h"
cp -f "$DEPS/FastNoiseLite-master/C/FastNoiseLite.h" "$LIBS/FastNoiseLite.h"
cp -f "$DEPS/minilua-main/minilua.h"                 "$LIBS/minilua.h"
cp -f "$DEPS/enet-master/include/enet.h"             "$LIBS/enet.h"
cp -f "$DEPS/raygui-3.2/src/raygui.h"                "$LIBS/raygui.h"
RAYLIB_SRC_DIR="$DEPS/raylib-$RAYLIB_TAG/src"

# FastNoiseLite's implementation block sits outside the header's include guard,
# so worldgenerator.c (which defines FNL_IMPL) and worldgen.h cannot both pull
# the header in.  Compile worldgenerator.c with the project's own
# MIDLESS_FNL_EXTERNAL escape hatch and provide the implementation once here.
cat > "$DEPS/fnl_impl.c" <<'EOF'
#define FNL_IMPL
#include "FastNoiseLite.h"
EOF

# ------------------------------------------------------------------- raylib --
RAYLIB_LIB="$DEPS/libraylib.a"
if [ ! -f "$RAYLIB_LIB" ]; then
    echo "building raylib $RAYLIB_TAG for $TARGET ..."
    RAYLIB_OBJ="$DEPS/raylib-obj"
    mkdir -p "$RAYLIB_OBJ"
    RCFLAGS="-std=c99 -Os -DNDEBUG -DPLATFORM_DESKTOP -D_GLFW_WIN32 -DSUPPORT_DEFAULT_FONT"
    RCFLAGS="$RCFLAGS -Wno-unused-function -Wno-unused-variable -Wno-sign-compare"
    RINC="-I$RAYLIB_SRC_DIR -I$RAYLIB_SRC_DIR/external -I$RAYLIB_SRC_DIR/external/glfw/include"
    for f in rcore rshapes rtextures rtext utils rglfw rmodels raudio; do
        # shellcheck disable=SC2086
        $ZIG cc -target $TARGET -c $RCFLAGS $RINC "$RAYLIB_SRC_DIR/$f.c" \
            -o "$RAYLIB_OBJ/$f.o"
    done
    # shellcheck disable=SC2086
    $ZIG ar rcs "$RAYLIB_LIB" "$RAYLIB_OBJ"/*.o
    echo "  -> $RAYLIB_LIB"
fi

# ------------------------------------------------------------------ midless --
CFLAGS="-std=c99 -O2 -D_DEFAULT_SOURCE -DPLATFORM_DESKTOP -DOS_WINDOWS"
CFLAGS="$CFLAGS -Wall -Wno-missing-braces"
# The project's Makefile puts every source directory on the include path; the
# sources rely on that (e.g. client/src/block/block.c includes "blockitemrenderer.h"
# from client/src/gui/).
SRC_DIRS=$(find "$ROOT/client" "$ROOT/server" "$ROOT/shared" -type d | sort -u)
INC="-I$ROOT/shared -I$LIBS"
for d in $SRC_DIRS; do INC="$INC -I$d"; done
# Only raylib's own headers go on the path for Midless sources.  Adding
# src/external would make <dirent.h> resolve to raylib's bundled copy, whose
# opendir/readdir are non-static and then clash with the ones inside
# libraylib.a(rcore.o) at link time.
INC="$INC -I$RAYLIB_SRC_DIR"
WINLIBS="-lopengl32 -lgdi32 -lwinmm -lws2_32 -lwinpthread"

OBJ="$DEPS/obj"
rm -rf "$OBJ"
mkdir -p "$OBJ"

compile() { # compile <out.o> <src.c> [extra cflags]
    out="$OBJ/$1"
    shift
    src="$1"
    shift
    mkdir -p "$(dirname "$out")"
    # shellcheck disable=SC2086
    $ZIG cc -target $TARGET -c $CFLAGS $INC "$@" "$src" -o "$out"
}

# server/src minus the two entry points and the websocket server: this is what
# the client links against to run an embedded singleplayer server.
SERVER_CORE=$(find "$ROOT/server/src" -name '*.c' \
    ! -name main.c ! -name server.c ! -name serverwss.c)
SERVER_ALL=$(find "$ROOT/server/src" -name '*.c')
CLIENT_SRC=$(find "$ROOT/client" -name '*.c' | sort -u)
SHARED_SRC=$(find "$ROOT/shared" -name '*.c')

link() { # link <exe> <subsystem> <objects...>
    exe="$1"
    sub="$2"
    shift 2
    # shellcheck disable=SC2086
    $ZIG cc -target $TARGET -static -Wl,--subsystem,$sub "$@" \
        -L"$DEPS" -lraylib $WINLIBS -lm -o "$exe"
    echo "  -> $exe ($(du -h "$exe" | cut -f1))"
}

echo "building game.exe (client + embedded server) ..."
# shellcheck disable=SC2086
for src in $CLIENT_SRC $SHARED_SRC $DEPS/fnl_impl.c; do
    extra=""
    case "$src" in
        *server/src/world/world.c)          extra="-DMIDLESS_STB_DS_EXTERNAL" ;;
        *server/src/world/worldgenerator.c) extra="-DMIDLESS_FNL_EXTERNAL" ;;
    esac
    # shellcheck disable=SC2086
    compile "$(basename "$src" .c)-client.o" "$src" $extra
done
# shellcheck disable=SC2086
for src in $SERVER_CORE; do
    extra=""
    case "$src" in
        *server/src/world/world.c)          extra="-DMIDLESS_STB_DS_EXTERNAL" ;;
        *server/src/world/worldgenerator.c) extra="-DMIDLESS_FNL_EXTERNAL" ;;
    esac
    # shellcheck disable=SC2086
    compile "srv-$(basename "$src" .c).o" "$src" $extra
done
link "$OUT/game.exe" windows "$OBJ"/*.o

echo "building server.exe (dedicated, headless) ..."
rm -f "$OBJ"/*.o
# shellcheck disable=SC2086
for src in $SERVER_ALL $SHARED_SRC $DEPS/fnl_impl.c; do
    extra="-DSERVER_HEADLESS"
    case "$src" in
        *server/src/world/worldgenerator.c) extra="$extra -DMIDLESS_FNL_EXTERNAL" ;;
    esac
    # shellcheck disable=SC2086
    compile "$(basename "$src" .c).o" "$src" $extra
done
link "$OUT/server.exe" console "$OBJ"/*.o

# ------------------------------------------------------------------- staging --
echo "staging $STAGE ..."
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$OUT/game.exe" "$OUT/server.exe" "$STAGE/"
mkdir -p "$STAGE/textures" "$STAGE/mods"
cp -f "$ROOT"/build/client/textures/*.png "$STAGE/textures/" 2>/dev/null || true
cp -f "$ROOT"/build/client/mods/*.lua     "$STAGE/mods/"      2>/dev/null || true
cp -f "$HERE/RUN-ON-WINDOWS.txt"          "$STAGE/README.txt" 2>/dev/null || true

if [ "${SKIP_ZIP:-0}" != "1" ]; then
    rm -f "$OUT/Midless-Windows-x64.zip"
    ( cd "$OUT" && python3 -c "
import shutil, sys
shutil.make_archive('Midless-Windows-x64', 'zip', '.', 'Midless-Windows-x64')
" )
    echo "  -> $OUT/Midless-Windows-x64.zip"
fi
echo "done."
