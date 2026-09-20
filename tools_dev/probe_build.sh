#!/bin/bash
# probe_build.sh - build + run the headless world probes on Linux.
#
# These link the REAL server worldgen (luaengine, luaworldgen, worldgen,
# worldgenfield, worldgenfeatures, chunk) against stub raylib + server
# surface, so mods/cosmic_islands.lua can be validated without a GPU,
# without Windows and without raylib.
#
#   tools_dev/probe_build.sh            # build both probes
#   tools_dev/probe_build.sh run        # build, then run the world probe
#   tools_dev/probe_build.sh modprobe   # build, then run the material-field probe
#
# Artifacts land in build/probe/ (gitignored) and the run sandbox is
# build/probe/sandbox (mods/ symlink + an empty world/ folder, which
# Worldgen_Freeze insists on writing worldgen.meta into).
set -eu
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-std=c99 -D_DEFAULT_SOURCE -DPLATFORM_DESKTOP -DOS_LINUX -O1 -Wall \
 -Wno-missing-braces -Wno-int-conversion -Wno-unused-result -Wno-unused-variable \
 -Wno-sign-compare -Wno-unused-but-set-variable"
INCLUDES="-Itests/vendor -Ilibs -Ishared -Iserver/src -Iserver/src/world \
 -Iserver/src/world/chunk -Iserver/src/scripting"

# worldgenerator.c carries the single FastNoiseLite implementation (FNL_IMPL)
SERVER_SRC="server/src/scripting/luaengine.c server/src/scripting/luabindings.c \
 server/src/scripting/luaentities.c server/src/scripting/luamodels.c \
 server/src/scripting/luavector.c server/src/scripting/luaworldgen.c \
 server/src/world/worldgen.c server/src/world/worldgenfield.c \
 server/src/world/worldgenfeatures.c server/src/world/worldgenerator.c \
 server/src/world/textures.c server/src/world/chunk/chunk.c"
SHARED_SRC=$(ls shared/*.c)

mkdir -p build/probe/sandbox/world
[ -e build/probe/sandbox/mods ] || ln -s ../../../mods build/probe/sandbox/mods

build() {
    local out="$1"; shift
    echo "== building $out =="
    $CC $CFLAGS $INCLUDES "$@" tools_dev/modprobe_stubs.c $SERVER_SRC $SHARED_SRC \
        -o "build/probe/$out" -lm -lpthread
}

build modprobe tools_dev/modprobe_harness.c
build worldprobe tools_dev/worldprobe.c

# v65.44: client-side air fast-path equivalence test (white-box: the real
# chunk.c + chunklightning.c against stubs - no server sources involved)
build_airlight() {
    echo "== building airlight_test =="
    $CC $CFLAGS $DEFINES_AIRLIGHT \
        -I/tmp/raylib45/src -I/tmp/raylib45/src/extras \
        -Iclient/src -Iclient/src/chunk -Iclient/src/block -Iclient/src/entity \
        -Iclient/src/gui -Iclient/src/networking -Ilibs -Ishared \
        tools_dev/airlight_test.c shared/chunkdata.c \
        -o build/probe/airlight -lm
}
DEFINES_AIRLIGHT="-std=c99 -D_DEFAULT_SOURCE -DPLATFORM_DESKTOP -DOS_LINUX -O1 -Wall \
 -Wno-missing-braces -Wno-int-conversion -Wno-unused-result -Wno-unused-variable \
 -Wno-sign-compare -Wno-unused-but-set-variable -Wno-unused-function"
build_airlight

MODE="${1:-build}"
case "$MODE" in
    run|worldprobe)
        echo "== running worldprobe =="
        (cd build/probe/sandbox && ../worldprobe)
        ;;
    modprobe-run)
        echo "== running modprobe =="
        (cd build/probe/sandbox && ../modprobe)
        ;;
    airlight-run)
        echo "== running airlight_test =="
        ./build/probe/airlight
        ;;
    *)
        echo "== build only =="
        ;;
esac
