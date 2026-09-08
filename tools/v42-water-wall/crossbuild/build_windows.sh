#!/bin/sh
# Cross-build Midless (the user's Midless-src tree) for Windows x64 on Linux WITHOUT mingw:
# uses `zig cc` (bundled mingw-w64 headers/libs) as a drop-in x86_64-w64-mingw32-gcc.
# Same flags/sources as tools/midless_build.sh + tools/build_raylib.sh from the rar
# (raylib 4.5.0 static with tools/rcore_patch.py borderless-fullscreen patch).
#
#   tools/v42-water-wall/crossbuild/build_windows.sh <Midless-src> <outdir>
#
# Needs: python3, pip (downloads the `ziglang` wheel once into .deps/), curl, tar.
# Output: <outdir>/game.exe, <outdir>/server.exe (link against the freshly built libraylib.a).
# Note: the resulting exes are built with clang/LLD instead of GCC/binutils - they are NOT
# byte-identical to the user's own mingw builds (they link the UCRT instead of msvcrt), but they
# are built from the same sources with the same defines.
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC=$(CDPATH= cd -- "${1:?path to unpacked Midless-src}" && pwd); OUT=${2:?output dir}; mkdir -p "$OUT"
DEPS="$HERE/.deps"; mkdir -p "$DEPS"
# --- toolchain: zig cc as mingw gcc ------------------------------------------------------------
if [ ! -x "$DEPS/zig/ziglang/zig" ]; then
    python3 -m pip download --no-deps -d "$DEPS" ziglang
    mkdir -p "$DEPS/zig" && (cd "$DEPS/zig" && python3 -c "import glob,zipfile; zipfile.ZipFile(glob.glob('../ziglang-*.whl')[0]).extractall()")
    chmod +x "$DEPS/zig/ziglang/zig"
fi
mkdir -p "$DEPS/bin"
cat > "$DEPS/bin/x86_64-w64-mingw32-gcc" <<EOS
#!/bin/bash
# zig cc wrapper emulating mingw gcc (adds -o <src>.o for 'gcc -c x.c' like gcc does)
args=("\$@"); has_c=0; has_o=0; src=""
for a in "\$@"; do [ "\$a" = "-c" ] && has_c=1; [ "\$a" = "-o" ] && has_o=1; case "\$a" in *.c) src="\$a";; esac; done
if [ \$has_c = 1 ] && [ \$has_o = 0 ] && [ -n "\$src" ]; then args+=("-o" "\$(basename "\${src%.c}").o"); fi
exec "$DEPS/zig/ziglang/zig" cc -target x86_64-windows-gnu -fno-sanitize=undefined "\${args[@]}"
EOS
printf '#!/bin/sh\nexec "%s" ar "$@"\n' "$DEPS/zig/ziglang/zig" > "$DEPS/bin/x86_64-w64-mingw32-ar"
chmod +x "$DEPS/bin/"*; PATH="$DEPS/bin:$PATH"; export PATH
# --- raylib 4.5.0 static + user's fullscreen patch ---------------------------------------------
if [ ! -f "$DEPS/win/libraylib.a" ]; then
    [ -d "$DEPS/raylib-4.5.0" ] || { curl -sSfL https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/4.5.0 -o "$DEPS/raylib.tgz"; tar xzf "$DEPS/raylib.tgz" -C "$DEPS"; }
    (cd "$DEPS/raylib-4.5.0/src" && python3 "$SRC/tools/rcore_patch.py" && rm -f ./*.o libraylib.a &&
     make PLATFORM=PLATFORM_DESKTOP PLATFORM_OS=WINDOWS CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar -j"$(nproc)" >/dev/null)
    mkdir -p "$DEPS/win" && cp "$DEPS/raylib-4.5.0/src/libraylib.a" "$DEPS/win/"
fi
# --- game/server: same recipe as tools/midless_build.sh ---------------------------------------
build() { # MODE
    MODE=$1; RAY="$DEPS/raylib-4.5.0"
    INC="-Iclient/src -Iclient/src/chunk -Iclient/src/block -Iclient/src/entity -Iclient/src/gui -Iclient/src/networking -Iserver/src -Iserver/src/world -Iserver/src/world/chunk -Iserver/src/scripting -Ilibs -Ishared -I$RAY/src -I$RAY/src/external"
    EXTRA=""; [ "$MODE" = client ] && EXTRA="-DMIDLESS_STB_DS_EXTERNAL"
    CFLAGS="-Wall -Wno-missing-braces -std=c99 -D_DEFAULT_SOURCE -DPLATFORM_DESKTOP -DOS_WINDOWS $EXTRA -Os $INC"
    OBJD="$DEPS/obj_$MODE"; rm -rf "$OBJD"; mkdir -p "$OBJD"
    if [ "$MODE" = client ]; then
        SRCS="$(ls client/src/*.c client/src/chunk/*.c client/src/block/*.c client/src/entity/*.c client/src/gui/*.c client/src/networking/*.c) $(ls shared/*.c)"
        SRCS="$SRCS $(ls server/src/*.c | grep -vE '(^|/)(main|server|serverwss)\.c$') $(ls server/src/world/*.c server/src/world/chunk/*.c server/src/scripting/*.c)"
    else
        SRCS="$(ls server/src/*.c server/src/world/*.c server/src/world/chunk/*.c server/src/scripting/*.c | grep -v serverwss.c) $(ls shared/*.c)"
    fi
    OBS=""
    for s in $SRCS; do
        o="$OBJD/$(echo "$s" | tr '/' '_').o"; OBS="$OBS $o"
        # shellcheck disable=SC2086
        x86_64-w64-mingw32-gcc $CFLAGS -c "$s" -o "$o" 2>"$o.log" || { cat "$o.log"; exit 1; }
    done
    # shellcheck disable=SC2086
    x86_64-w64-mingw32-gcc -Os -Wl,--subsystem,windows -o "$OUT/$2" $OBS -L"$DEPS/win" -static -lraylib -lopengl32 -lgdi32 -lwinmm -lpthread -lws2_32 -lm
    echo "OK $OUT/$2 $(stat -c%s "$OUT/$2") bytes"
}
cd "$SRC"
# newer mingw-w64 headers already define clock_gettime; enet.h redefines it -> apply the 3-line guard once
grep -q 'WIN_PTHREADS_TIME_H' libs/enet.h || patch -p1 -s < "$HERE/enet-clock_gettime-newer-mingw.patch"
build server server.exe
build client game.exe
