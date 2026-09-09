#!/bin/bash
# compile_check.sh - Linux header-only compile check of every source file
# (mirrors build_cosmic.sh sources; no linking, so no raylib build needed)
set -u

RAYLIB_SRC="${RAYLIB_SRC:-/tmp/raylib45/src}"

CC=gcc
CFLAGS="-Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces -Wno-int-conversion -O1"
DEFINES="-DPLATFORM_DESKTOP -DOS_LINUX"

CLIENT_DIRS="./client/src ./client/src/chunk ./client/src/block ./client/src/entity ./client/src/gui ./client/src/networking"
SERVER_CORE_DIRS="./server/src ./server/src/world ./server/src/world/chunk ./server/src/scripting"
SHARED_DIRS="./libs ./shared"

INCLUDES="-I$RAYLIB_SRC -I$RAYLIB_SRC/extras"
for d in $CLIENT_DIRS $SERVER_CORE_DIRS $SHARED_DIRS; do
    INCLUDES="$INCLUDES -I$d"
done
INCLUDES="$INCLUDES -I./server/src -I./server/src/world -I./server/src/world/chunk -I./server/src/scripting"

SRC=""
for d in $CLIENT_DIRS $SERVER_CORE_DIRS $SHARED_DIRS; do
    for f in $d/*.c; do
        [ -f "$f" ] && SRC="$SRC $f"
    done
done

# client build excludes dedicated server files; also check them separately with BUILD_SERVER defines
EXCLUDE_SERVER="server/src/main.c server/src/server.c server/src/serverwss.c"
FAILED=0
mkdir -p /tmp/objs
for src in $SRC; do
    SKIP=false
    for ex in $EXCLUDE_SERVER; do
        case "$src" in *$ex*) SKIP=true;; esac
    done
    $SKIP && continue
    EXTRA_DEFS="$DEFINES"
    case "$src" in
        *server/src/world/world.c) EXTRA_DEFS="$DEFINES -DMIDLESS_STB_DS_EXTERNAL";;
        *server/src/main.c|*server/src/server.c) EXTRA_DEFS="-DPLATFORM_DESKTOP -DOS_LINUX -DBUILD_SERVER";;
    esac
    obj="/tmp/objs/$(echo $src | tr '/' '_').o"
    OUT=$($CC $CFLAGS $EXTRA_DEFS $INCLUDES -c "$src" -o "$obj" 2>&1)
    if [ $? -ne 0 ]; then
        echo "FAILED: $src"
        echo "$OUT" | grep -E "error|warning" | head -15
        FAILED=1
    fi
done

# dedicated server sources with BUILD_SERVER
for src in ./server/src/main.c ./server/src/server.c ./server/src/serverwss.c; do
    obj="/tmp/objs/$(echo $src | tr '/' '_').o"
    OUT=$($CC $CFLAGS -DPLATFORM_DESKTOP -DOS_LINUX -DBUILD_SERVER $INCLUDES -c "$src" -o "$obj" 2>&1)
    if [ $? -ne 0 ]; then
        echo "FAILED: $src"
        echo "$OUT" | grep -E "error|warning" | head -15
        FAILED=1
    fi
done

if [ $FAILED -eq 0 ]; then echo "COMPILE CHECK OK ($(echo $SRC | wc -w) + 3 files)"; else echo "COMPILE CHECK FAILED"; exit 1; fi
