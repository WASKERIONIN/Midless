#!/bin/sh
# Full check of the binary patch, run from the repo root:
#   tools/v42-water-wall/binpatch/verify.sh <Midless-Windows-x64 dir with ORIGINAL v42 exes> <unpacked Midless-src>
# 1. patches game.exe/server.exe into ./out/
# 2. counts water walls in the exe's own generator (Unicorn) at the wall hot spots of three seeds
# 3. compares emulated chunks cell-by-cell with a native build of the C-patched worldgenerator.c
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REL=${1:?dir with original v42 game.exe/server.exe}; SRC=${2:?unpacked Midless-src (rar)}
RL=${RAYLIB_SRC:-"$HERE/../.deps/raylib-4.5.0/src"}
python3 -c 'import unicorn' 2>/dev/null || { echo "pip install unicorn"; exit 2; }
mkdir -p "$HERE/out"
python3 "$HERE/patch.py" "$REL/game.exe"   "$HERE/out/game.exe"
python3 "$HERE/patch.py" "$REL/server.exe" "$HERE/out/server.exe"
# native reference: C-patched generator (apply ../worldgenerator-no-caves-below-sea.patch to a copy of SRC)
rm -rf "$HERE/out/src" && cp -r "$SRC" "$HERE/out/src" && (cd "$HERE/out/src" && patch -p1 -s < "$HERE/../worldgenerator-no-caves-below-sea.patch")
S="$HERE/out/src"; INC="-I$S/server/src -I$S/server/src/world -I$S/server/src/world/chunk -I$S/shared -I$S/libs -I$RL"
GEN="$S/server/src/world/worldgenerator.c $S/server/src/world/chunk/chunk.c $S/shared/chunkdata.c"
# shellcheck disable=SC2086
gcc -std=gnu99 -O2 -D_DEFAULT_SOURCE -DPLATFORM_DESKTOP -w $INC "$HERE/../dumpchunk.c" "$HERE/../stubs.c" "$HERE/../raymath_impl.c" $GEN -o "$HERE/out/dumpchunk_fixed" -lm
status=0
for exe in "$HERE/out/game.exe" "$HERE/out/server.exe"; do
    for args in "1234 -4 1 -11 -9" "7 6 8 7 7" "99991 -7 -6 8 8"; do
        # shellcheck disable=SC2086
        python3 "$HERE/emu.py" "$exe" $args | tee /dev/stderr | grep -q "water-next-to-dry-air 0, water-over-air 0, dry air below 48: 0" || status=1
    done
    for c in "1234 -3 2 -10" "7 8 2 7" "99991 -7 2 8"; do
        # shellcheck disable=SC2086
        python3 "$HERE/compare_native.py" "$exe" "$HERE/out/dumpchunk_fixed" $c || status=1
    done
done
echo "verify: $([ $status = 0 ] && echo OK || echo FAILED)"; exit $status
