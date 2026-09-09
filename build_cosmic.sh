#!/bin/bash
# build_cosmic.sh - Direct build script for Midless Cosmic Edition
# Designed for MSYS2 MINGW64 environment
set -ex

CC=gcc
CFLAGS="-Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces -s -Os"
DEFINES="-DPLATFORM_DESKTOP -DOS_WINDOWS"

RAYLIB_INC="/mingw64/include"
RAYLIB_LIB="/mingw64/lib"

# Source directories
CLIENT_DIRS="./client/src ./client/src/chunk ./client/src/block ./client/src/entity ./client/src/gui ./client/src/networking"
SERVER_CORE_DIRS="./server/src ./server/src/world ./server/src/world/chunk ./server/src/scripting"
SHARED_DIRS="./libs ./shared"

ALL_CLIENT_DIRS="$CLIENT_DIRS $SERVER_CORE_DIRS $SHARED_DIRS"

INCLUDES=""
for d in $ALL_CLIENT_DIRS; do
    INCLUDES="$INCLUDES -I$d"
done
INCLUDES="$INCLUDES -I$RAYLIB_INC -I./server/src -I./server/src/world -I./server/src/world/chunk -I./server/src/scripting"

# Collect all source files
CLIENT_SRC=""
for d in $CLIENT_DIRS; do
    for f in $d/*.c; do
        [ -f "$f" ] && CLIENT_SRC="$CLIENT_SRC $f"
    done
done

SERVER_CORE_SRC=""
for d in $SERVER_CORE_DIRS; do
    for f in $d/*.c; do
        [ -f "$f" ] && SERVER_CORE_SRC="$SERVER_CORE_SRC $f"
    done
done

SHARED_SRC=""
for d in $SHARED_DIRS; do
    for f in $d/*.c; do
        [ -f "$f" ] && SHARED_SRC="$SHARED_SRC $f"
    done
done

echo "=== Client sources ==="
echo "$CLIENT_SRC" | tr ' ' '\n' | head -30
echo "=== Shared sources ==="
echo "$SHARED_SRC" | tr ' ' '\n'

# Build client
echo "=== Building client ==="
mkdir -p build/client

# Compile all client + embedded server sources
OBJ_DIR="build/obj_client"
mkdir -p "$OBJ_DIR"

ALL_SRC="$CLIENT_SRC $SERVER_CORE_SRC $SHARED_SRC"
OBJS=""
FAILED=""
ERRORS_FILE="build_errors.txt"
> "$ERRORS_FILE"
for src in $ALL_SRC; do
    obj="$OBJ_DIR/$(echo $src | tr '/' '_').o"
    EXTRA_DEFS="$DEFINES"
    if echo "$src" | grep -q "server/src/world/world.c"; then
        EXTRA_DEFS="$DEFINES -DMIDLESS_STB_DS_EXTERNAL"
    fi
    echo "--- Compiling: $src ---"
    COMPILE_OUTPUT=$($CC $CFLAGS $EXTRA_DEFS $INCLUDES -c "$src" -o "$obj" 2>&1) || {
        echo "FAILED: $src"
        echo "$COMPILE_OUTPUT"
        FIRST_ERROR=$(echo "$COMPILE_OUTPUT" | grep "error:" | head -1)
        echo "::error file=$src::$FIRST_ERROR"
        echo "=== $src ===" >> "$ERRORS_FILE"
        echo "$COMPILE_OUTPUT" >> "$ERRORS_FILE"
        FAILED="$FAILED $src"
        continue
    }
    OBJS="$OBJS $obj"
done

if [ -n "$FAILED" ]; then
    echo "=== COMPILATION FAILURES ==="
    echo "Failed files:$FAILED"
    cat "$ERRORS_FILE"
    exit 1
fi

# Link client
$CC $OBJS -o build/client/game.exe \
    -L"$RAYLIB_LIB" -lraylib -lenet -lopengl32 -lgdi32 -lwinmm -lpthread -lws2_32 \
    -Wl,--subsystem,windows

echo "=== Client built: build/client/game.exe ==="
ls -la build/client/game.exe

# Build server
echo "=== Building server ==="
SERVER_ALL_DIRS="./server/src ./server/src/world ./server/src/world/chunk ./server/src/scripting"

SERVER_SRC=""
for d in $SERVER_ALL_DIRS; do
    for f in $d/*.c; do
        [ -f "$f" ] && SERVER_SRC="$SERVER_SRC $f"
    done
done

OBJ_DIR_S="build/obj_server"
mkdir -p "$OBJ_DIR_S"

OBJS_S=""
FAILED_S=""
> "$ERRORS_FILE"
for src in $SERVER_SRC $SHARED_SRC; do
    obj="$OBJ_DIR_S/$(echo $src | tr '/' '_').o"
    echo "--- Compiling server: $src ---"
    COMPILE_OUTPUT=$($CC $CFLAGS $DEFINES $INCLUDES -c "$src" -o "$obj" 2>&1) || {
        echo "FAILED: $src"
        echo "$COMPILE_OUTPUT"
        FIRST_ERROR=$(echo "$COMPILE_OUTPUT" | grep "error:" | head -1)
        echo "::error file=$src::$FIRST_ERROR"
        echo "=== $src ===" >> "$ERRORS_FILE"
        echo "$COMPILE_OUTPUT" >> "$ERRORS_FILE"
        FAILED_S="$FAILED_S $src"
        continue
    }
    OBJS_S="$OBJS_S $obj"
done

if [ -n "$FAILED_S" ]; then
    echo "=== SERVER COMPILATION FAILURES ==="
    echo "Failed files:$FAILED_S"
    cat "$ERRORS_FILE"
    exit 1
fi

$CC $OBJS_S -o build/server/server.exe \
    -L"$RAYLIB_LIB" -lraylib -lenet -lopengl32 -lgdi32 -lwinmm -lpthread -lws2_32

echo "=== Server built: build/server/server.exe ==="
ls -la build/server/server.exe
